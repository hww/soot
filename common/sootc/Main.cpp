// main.cpp
#include "CommonTypes.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "sootc/compiler/Compiler.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Command-line options for the SOOT compiler binary.
//
// Naming:
//   * `sootc`  — the tool (this binary).
//   * `soots`  — the interpreter (lib.st).
//   * `soot`   — the compiled language (lib.sc).
//
// This struct covers ONLY the flags that sootc itself understands.
// Anything that concerns the execution backend (local VM, remote host,
// null) is expressed through `--runtime` and passed to the Compiler as
// CompilationOptions::runtime_url.
struct CommandLineOptions {
    // Input / output.
    std::vector<std::string> input_files;
    std::string              target_dir = "build";
    std::string              source_root = ".";
    std::string              output_name;
    std::string              user_profile = "#f";

    // Behavior.
    bool        interactive = true;    // open REPL?
    bool        print_listing = false; // print listing in CLI mode?
    std::string log_level = "info";    // debug | info | warn | error

    // Compiler mode.
    sootc::CompilerMode mode = sootc::CompilerMode::HYBRID;
    bool                debug_ast = false;
    bool                debug_ir = false;
    bool                debug_asm = false;

    // Execution backend.
    //   "local"         -> LocalVmRuntime (default)
    //   "null"          -> NullRuntime
    //   "<host>:<port>" -> RemoteRuntime
    std::string runtime_url = "local";

    // SIDBase path (currently stored but not used; loaded/saved by a
    // future change to Compiler).
    std::string sidbase_path;

    // Soot platform.
    SootPlatform platform = SootPlatform::Default;

    // Meta.
    bool help = false;
    bool version = false;
};

void print_banner() {
    fmt::print(fmt::fg(fmt::color::cyan) | fmt::emphasis::bold, "=== SOOT Compiler v1.0 ===\n");
    fmt::print("Type (help) for help, (e) to quit\n");
}

void print_help(const char *program_name) {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "SOOT Compiler\n\n");
    fmt::print("Usage: {} [options] [files...]\n\n", program_name);

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Input / output:\n");
    fmt::print("  --target <dir>       Target directory for compiled files (default: build)\n");
    fmt::print("  --source-root <dir>  Root directory for module namespace (default: .)\n");
    fmt::print("  -o, --output <name>  Output base name\n");
    fmt::print("  --profile <name>     Load user profile\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nBehavior:\n");
    fmt::print("  -n, --no-repl        Don't open the REPL (CLI mode only)\n");
    fmt::print("  --print-listing      Print binary listing after each file (CLI)\n");
    fmt::print("  --log <level>        Log level: debug|info|warn|error (default: info)\n");
    fmt::print("  --sidbase <path>     Path to sidbase (reserved; not used yet)\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nCompiler mode:\n");
    fmt::print("  --compile-only       Only compile, no interpretation at REPL\n");
    fmt::print("  --interpret-only     Only interpret, no compilation at REPL\n");
    fmt::print("  --debug-ast          Print AST during compilation\n");
    fmt::print("  --debug-ir           Print intermediate representation\n");
    fmt::print("  --debug-asm          Print generated assembly\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nExecution backend:\n");
    fmt::print("  --runtime <url>      local  -> in-process VM (default)\n");
    fmt::print("                       null   -> do not execute anything\n");
    fmt::print("                       host:port -> remote host\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nMeta:\n");
    fmt::print("  -h, --help           Show this help\n");
    fmt::print("  -v, --version        Show version\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nExamples:\n");
    fmt::print("  {}                                  ; open REPL\n", program_name);
    fmt::print("  {} script.sc                        ; compile then open REPL\n", program_name);
    fmt::print("  {} -n script.sc                     ; compile only, no REPL\n", program_name);
    fmt::print("  {} --runtime null -n script.sc      ; compile, don't run\n", program_name);
    fmt::print("  {} --runtime 127.0.0.1:8181         ; open REPL, talk to remote host\n",
               program_name);
    fmt::print("  {} --target out --source-root src -n math/add.sc\n", program_name);

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nREPL commands (in Lisp syntax):\n");
    fmt::print("  (help)                Show REPL help\n");
    fmt::print("  (e)                   Exit\n");
    fmt::print("  (reload)              Reload compiler\n");
    fmt::print("  (clear)               Clear screen\n");
    fmt::print("  (sizes)               Print structure sizes\n");
    fmt::print("  (inspect-env)         Show *soot-env* (compiler env)\n");
    fmt::print("  (inspect-env-soots)   Show *global-env* (interpreter env)\n");
    fmt::print("  (has-macro \"name\")    Check if name is a macro\n");
    fmt::print("  (list-symbols)        List symbols in Globals\n");
    fmt::print("  (save-session [\"n\"])  Save session to .bin / .h\n");
    fmt::print("  (sc-run name args...) Run a compiled function\n");
    fmt::print("  (sc-eval expr)        Evaluate in the interpreter\n");
}

// Parse a log level name into a lg::level.
static lg::level parse_log_level(const std::string &name) {
    if (name == "debug") return lg::level::debug;
    if (name == "info") return lg::level::info;
    if (name == "warn" || name == "warning") return lg::level::warn;
    if (name == "error") return lg::level::error;
    return lg::level::info;
}

CommandLineOptions parse_args(int argc, char *argv[]) {
    CommandLineOptions opts;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // ----- Input / output -----
        if (arg == "--target" && i + 1 < argc) {
            opts.target_dir = argv[++i];
        } else if (arg == "--source-root" && i + 1 < argc) {
            opts.source_root = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opts.output_name = argv[++i];
        } else if (arg == "--profile" && i + 1 < argc) {
            opts.user_profile = argv[++i];
        }

        // ----- Behavior -----
        else if (arg == "--no-repl" || arg == "-n") {
            opts.interactive = false;
        } else if (arg == "--print-listing") {
            opts.print_listing = true;
        } else if (arg == "--log" && i + 1 < argc) {
            opts.log_level = argv[++i];
        } else if (arg == "--sidbase" && i + 1 < argc) {
            opts.sidbase_path = argv[++i];
        }

        // ----- Compiler mode -----
        else if (arg == "--compile-only") {
            opts.mode = sootc::CompilerMode::COMPILE_ONLY;
        } else if (arg == "--interpret-only") {
            opts.mode = sootc::CompilerMode::INTERPRET_ONLY;
        } else if (arg == "--debug-ast") {
            opts.debug_ast = true;
        } else if (arg == "--debug-ir") {
            opts.debug_ir = true;
        } else if (arg == "--debug-asm") {
            opts.debug_asm = true;
        }

        // ----- Execution backend -----
        else if (arg == "--runtime" && i + 1 < argc) {
            opts.runtime_url = argv[++i];
        }

        // ----- Meta -----
        else if (arg == "-h" || arg == "--help") {
            opts.help = true;
        } else if (arg == "-v" || arg == "--version") {
            opts.version = true;
        }

        // ----- Positional: input file -----
        else if (arg[0] != '-') {
            opts.input_files.push_back(arg);
        }

        // ----- Unknown -----
        else {
            fmt::print(stderr, "Unknown option: {}\n", arg);
            opts.help = true;
        }
    }

    return opts;
}

int main(int argc, char *argv[]) {
    try {
        auto opts = parse_args(argc, argv);

        if (opts.help) {
            print_help(argv[0]);
            return 0;
        }
        if (opts.version) {
            fmt::print("SOOT Compiler v1.0\n");
            return 0;
        }

        // ----- Logging -----
        lg::set_file_level(lg::level::info);
        lg::set_stdout_level(parse_log_level(opts.log_level));
        lg::set_file("compiler");
        lg::initialize();

        // ----- REPL configuration -----
        REPL::Config repl_config(opts.platform);
        repl_config.asm_file_search_dirs.push_back(
            file_util::get_path(file_util::PathType::PROJECT).string());
        repl_config.per_game_history = true;

        auto startup_file = REPL::load_user_startup_file(opts.user_profile, opts.platform);

        // ----- Compiler options -----
        sootc::CompilationOptions comp_options;
        comp_options.platform = opts.platform;
        comp_options.mode = opts.mode;
        comp_options.debug_print_ir = opts.debug_ir;
        comp_options.debug_print_ast = opts.debug_ast;
        comp_options.debug_print_asm = opts.debug_asm;
        comp_options.print_listing = opts.print_listing;
        comp_options.log_level = opts.log_level;
        comp_options.sidbase_path = opts.sidbase_path;
        comp_options.user_profile = opts.user_profile;
        comp_options.search_paths = {".", "scripts", "src", "examples", opts.source_root};

        // Translate the CLI runtime name into the Compiler's runtime_url.
        // The Compiler expects an empty string for the local VM, "null"
        // for NullRuntime, and "<host>:<port>" for RemoteRuntime.
        if (opts.runtime_url == "local" || opts.runtime_url.empty()) {
            comp_options.runtime_url = "";
        } else {
            comp_options.runtime_url = opts.runtime_url;
        }

        // ----- Compiler -----
        std::unique_ptr<sootc::Compiler> compiler;
        try {
            compiler = std::make_unique<sootc::Compiler>(
                comp_options, repl_config, opts.user_profile,
                std::make_unique<REPL::Wrapper>(opts.user_profile, repl_config, startup_file,
                                                /*nrepl_alive=*/false));
        } catch (const std::exception &e) {
            lg::error("Failed to create Compiler: {}", e.what());
            return 1;
        }

        // ----- Run startup commands -----
        for (const auto &cmd : startup_file.run_before_listen) {
            compiler->handle_repl_string(cmd);
        }

        // ----- Compile input files -----
        if (!opts.input_files.empty()) {
            namespace fs = std::filesystem;

            for (const auto &input_file : opts.input_files) {
                lg::info("Processing: {}", input_file);

                auto result = compiler->compile_file(input_file);
                if (!result) {
                    lg::error("Failed: {} - {}", input_file, result.error());
                    continue;
                }

                // If compilation produced no binary (only type/enum
                // declarations), there is nothing to save. That is a
                // normal outcome, not an error.
                if (!*result) {
                    lg::info("No binary output for: {}", input_file);
                    continue;
                }

                if (opts.print_listing) { compiler->print_listing(**result); }

                fs::path target = opts.target_dir;
                compiler->save_binary(**result, target);
                compiler->save_header(**result, target);

                lg::info("Compiled: {}", input_file);
            }

            // If REPL is disabled, exit after compiling.
            if (!opts.interactive) { return 0; }
        } else if (!opts.interactive) {
            // No files, no REPL: nothing to do. This is a mistake.
            lg::error("Nothing to do: no input files and REPL is disabled. "
                      "Pass a file to compile, or remove --no-repl to open the REPL.");
            return 1;
        }

        // ----- REPL -----
        print_banner();

        sootc::ReplStatus status = sootc::ReplStatus::OK;
        while (status != sootc::ReplStatus::WANT_EXIT) {
            if (status == sootc::ReplStatus::WANT_RELOAD) {
                lg::info("Reloading compiler...");
                compiler = std::make_unique<sootc::Compiler>(
                    comp_options, repl_config, opts.user_profile,
                    std::make_unique<REPL::Wrapper>(opts.user_profile, repl_config, startup_file,
                                                    /*nrepl_alive=*/false));
                status = sootc::ReplStatus::OK;
            }

            std::string input = compiler->get_repl_input();
            if (!input.empty()) {
                status = compiler->handle_repl_string(input);
                compiler->save_repl_history();
            }
        }

        return 0;

    } catch (const std::exception &e) {
        lg::error("Fatal error: {}", e.what());
        return 1;
    }
}