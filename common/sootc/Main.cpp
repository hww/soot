// main.cpp
#include "CommonTypes.hpp"

#include "carbon/lib/SIDBase.hpp"
#include "carbon/lib/StringIdManager.hpp"

#include "sootc/compiler/Compiler.hpp"

#include "util/FileUtil.hpp"
#include "util/ProjectConfig.hpp"
#include "util/Log.hpp"

#include "fmt/color.h"
#include "fmt/core.h"

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
    std::string              run_script; // --run do.st
    std::string              output_name;
    std::string              user_profile = "#f";

    // Behavior.
    /// Tri-state: unset means "use soot.json"; true/false mean
    /// "force REPL on/off, overriding soot.json".
    std::optional<bool> repl;
    bool        print_listing = false;
    std::string log_level = "warn";
    std::string log_file;
    std::string project_root; // --project-root

    // Compiler mode.
    sootc::CompilerMode mode = sootc::CompilerMode::HYBRID;
    bool                debug_ast = false;
    bool                debug_ir = false;
    bool                debug_asm = false;

    // Execution backend.
    std::string runtime_url = "local";

    // SIDBase path.
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

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Project:\n");
    fmt::print("  --project-root <dir> Project root. All relative module paths\n");
    fmt::print("                       are resolved against this root.\n");
    fmt::print("                       Default: current working directory.\n");
    fmt::print("  --project <dir>      Alias for --project-root (legacy).\n");
    fmt::print("  --run <script.st>    Execute a script in the REPL after startup.\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Input / output:\n");
    fmt::print("  --target <dir>       Target directory for compiled files (default: build)\n");
    fmt::print("  --source-root <dir>  Root directory for module namespace (default: .)\n");
    fmt::print("  -o, --output <name>  Output base name\n");
    fmt::print("  --profile <name>     Load user profile\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nBehavior:\n");
    fmt::print("  -r --repl            Open the REPL (CLI mode only)\n");
    fmt::print("                       (overrides soot.json \"repl\")\n");
    fmt::print("  -n --no-repl         Don't open the REPL (CLI mode only)\n");
    fmt::print("                       (overrides soot.json \"repl\")\n");
    fmt::print("  --print-listing      Print binary listing after each file (CLI)\n");
    fmt::print("  --log <level>        Log level: debug|info|warn|error (default: info)\n");
    fmt::print("  --log-file <path>    Write logs to a file (default: stderr only)\n");

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nString IDs:\n");
    fmt::print("  --sidbase <path>     Path to sidbase. Loaded before compilation,\n");
    fmt::print("                       saved after all input files are processed.\n");
    fmt::print("                       If omitted, sidbase lives in memory only.\n");

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
    fmt::print("  {} --sidbase base.bin -n a.sc b.sc  ; compile many files, one sidbase\n",
               program_name);

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
        else if (arg == "--repl" || arg == "-r") {
            opts.repl = true;
        } else if (arg == "--no-repl" || arg == "-n") {
            opts.repl = false;
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

        // ----- Log file -----
        else if (arg == "--log-file" && i + 1 < argc) {
            opts.log_file = argv[++i];
        }
       
        else if ((arg == "--project-root" || arg == "--project") && i + 1 < argc) {
            opts.project_root = argv[++i];
        } else if (arg == "--run" && i + 1 < argc) {
            opts.run_script = argv[++i];
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
        //
        // Convention:
        //   * The loggers (lg::info, lg::warn, lg::error, lg::debug)
        //     always go to stderr, so they never pollute the actual
        //     output of the tool. A caller can redirect stdout to a
        //     file and still see diagnostics on the terminal.
        //   * Log level is controlled by --log (default "info").
        //   * File logging is OPT-IN via --log-file <path>. Without
        //     that flag, no file is created anywhere — not in the
        //     current directory, not in a cache dir. This matches how
        //     rustc, clang, and cargo behave.
        lg::set_stdout_level(parse_log_level(opts.log_level));

        if (!opts.log_file.empty()) {
            lg::set_file(opts.log_file);
            lg::set_file_level(parse_log_level(opts.log_level));
            lg::initialize();
            lg::info("Log file enabled: {}", opts.log_file);
        } else {
            // No file logging. Tell the logger to use stderr only.
            // (See the Log library's API for the exact call; the
            // important thing is that no file is opened.)
            lg::initialize();
        }

         // ----- Project root + soot.json -----
        //
        // SootProjectConfig::load applies the same resolution rules for
        // sootc and carbon:
        //   1. --project-root
        //   2. SOOT_ROOT environment variable
        //   3. Walk up from CWD looking for soot.json
        //   4. Current working directory
        //
        // A missing or malformed soot.json is not an error: defaults
        // are used.
        common::SootProjectConfig project_cfg = common::SootProjectConfig::load(opts.project_root);
        common::set_project_path(project_cfg.project_root);
        lg::info("PROJECT = {}", project_cfg.project_root.string());
        // REPL: CLI --repl / --no-repl override soot.json. If neither
        // is set, soot.json decides; if soot.json is silent, default
        // is false (exit after compile/run).
        const bool   want_repl = opts.repl.value_or(project_cfg.repl);

        // ----- REPL configuration -----
        REPL::Config repl_config(opts.platform);
        repl_config.asm_file_search_dirs.push_back(
            common::get_path(common::PathType::PROJECT).string());
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
        // search_paths: soot.json wins if it lists them; otherwise
        // the CLI default plus --source-root.
        if (!project_cfg.search_paths.empty()) {
            comp_options.search_paths.clear();
            for (const auto &p : project_cfg.search_paths) {
                comp_options.search_paths.push_back(p.string());
            }
        } else {
            comp_options.search_paths = {".", "scripts", "src", "examples", opts.source_root};
        }
        comp_options.project_root = project_cfg.project_root;
        comp_options.target_dir = opts.target_dir;

        // sidbase: soot.json wins; otherwise the CLI value.
        if (opts.sidbase_path.empty() && !project_cfg.sidbase.empty()) {
            comp_options.sidbase_path = project_cfg.sidbase_absolute().string();
            lg::info("ProjectConfig: sidbase = {}", comp_options.sidbase_path);
        }

        // runtime: soot.json wins; otherwise the CLI value.
        if (opts.runtime_url.empty() || opts.runtime_url == "local") {
            if (project_cfg.runtime != "local") { comp_options.runtime_url = project_cfg.runtime; }
        }

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

        // ----- Run --run script (if given) -----
        //
        // The script is executed in the same Compiler instance, so it
        // sees every module already loaded and every prelude macro. A
        // failure aborts immediately: the user is not left wondering
        // whether the build completed.
        if (!opts.run_script.empty()) {
            std::string script = file_util::read_text(opts.run_script);
            if (script.empty()) {
                lg::error("Cannot read script: {}", opts.run_script);
                return 1;
            }
            auto status = compiler->handle_repl_string(script);
            if (status == sootc::ReplStatus::WANT_EXIT) {
                // The script asked to exit. Honour it: --repl or not,
                // we stop here.
                return 0;
            }
            if (status == sootc::ReplStatus::ERR) { return 1; }
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

            // ----- SIDBase: save after compilation -----
            //
            // All strings encountered by the compiler have been
            // registered in StringIdManager by this point (via the
            // StringId constructors). We flush the accumulated table
            // to disk exactly once, so that a single
            // `sootc --sidbase base.bin a.sc b.sc c.sc` run produces
            // a single consistent sidbase regardless of how many
            // files it compiled.
            //
            // If --sidbase was not given, we skip saving: the base
            // lives only in memory for the duration of the process.
            compiler->save_sidbase();

            // If REPL is disabled, exit after compiling.
            if (!want_repl) { return 0; }
        } else if (!want_repl) {
            // No files, no --run, no REPL: nothing to do.
            // --run was already executed above; if it did something,
            // this branch is not reached. Reaching it means the user
            // ran sootc with no input and no --repl, which is almost
            // certainly a mistake.
            if (opts.run_script.empty()) {
                lg::error("Nothing to do: no input files, no --run, and REPL is disabled. "
                          "Pass a file to compile, pass --run <script>, "
                          "or add --repl to open the REPL.");
                return 1;
            }
            // --run was given and executed successfully. Just exit.
            return 0;
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

        // ----- SIDBase: save on REPL exit -----
        //
        // If the user ran the REPL (interactive mode) and typed any
        // #name literals, those strings were registered during the
        // session. Flush them to disk on the way out. This is best-
        // effort: if the user kills the terminal, we can't save.
        if (want_repl && !comp_options.sidbase_path.empty()) {
            if (carbon::StringIdManager::instance().save_dconstruct_sidbase(
                    comp_options.sidbase_path)) {
                lg::info("Saved sidbase: {} ({} entries)", comp_options.sidbase_path,
                         carbon::StringIdManager::instance().size());
            } else {
                lg::error("Failed to save sidbase: {}", comp_options.sidbase_path);
            }
        }

        return 0;

    } catch (const std::exception &e) {
        lg::error("Fatal error: {}", e.what());
        return 1;
    }
}