// main.cpp
#include "CommonTypes.hpp"

#include "carbon/lib/SIDBase.hpp"
#include "carbon/lib/StringIdManager.hpp"

#include "sootc/compiler/Compiler.hpp"

#include "util/Config.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "util/ProjectConfig.hpp"

#include "fmt/color.h"
#include "fmt/core.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    // ================================================================
    // Early scan
    // ================================================================
    //
    // --config, --no-config and --project-root decide where (and
    // whether) soot.json is read, so they must be known before
    // load_json(). Everything else goes through apply_cli() after
    // the layers are resolved.
    //
    // This is a tiny pass over argv, not a parser.

    struct EarlyArgs {
        std::string project_root;
        std::string config_file;
        bool        no_config = false;
    };

    EarlyArgs scan_early_args(int argc, char *argv[]) {
        EarlyArgs out;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                out.config_file = argv[++i];
            } else if (arg == "--project-root" || arg == "--project") {
                if (i + 1 < argc) out.project_root = argv[++i];
            } else if (arg == "--no-config") {
                out.no_config = true;
            }
        }
        return out;
    }

    // ================================================================
    // Logging setup
    // ================================================================

    void setup_logging(const common::Config &cfg) {
        lg::set_console_level(cfg.common.log_level);
        if (!cfg.common.log_file.empty()) {
            lg::set_file(cfg.common.log_file.string());
            lg::set_file_level(cfg.common.log_level);
            lg::info("Log file enabled: {}", cfg.common.log_file.string());
        }
        lg::initialize();
    }

} // namespace

int main(int argc, char *argv[]) {
    try {
        // ----- Layers -----
        //
        // 1. Early scan: --config, --no-config, --project-root.
        // 2. load_json:    soot.json (or --config file), unless --no-config.
        // 3. apply_env:    SOOT_* variables.
        // 4. apply_cli:    everything else on the command line.
        //
        // Later layers win. Each layer only writes the keys it has.

        const EarlyArgs early = scan_early_args(argc, argv);

        if (early.no_config && !early.config_file.empty()) {
            fmt::print(stderr, "error: --config and --no-config are mutually exclusive\n");
            return 2;
        }

        common::Config cfg;

        if (!early.no_config) {
            if (auto err = common::load_json(early.config_file, early.project_root, cfg)) {
                fmt::print(stderr, "error: {}\n", *err);
                return 2;
            }
        }

        if (auto err = common::apply_env(cfg)) {
            fmt::print(stderr, "error: {}\n", *err);
            return 2;
        }

        std::vector<std::string> inputs;
        bool                     no_config = early.no_config;
        if (auto err =
                common::apply_cli(argc, argv, cfg, common::Scope::Compiler, inputs, no_config)) {
            fmt::print(stderr, "error: {}\n", *err);
            return 2;
        }

        // ----- Logging -----
        //
        // Must happen before anything logs.

        setup_logging(cfg);

        // ----- Project root -----
        //
        // With --no-config, project_root comes from --project-root
        // (already applied by apply_cli), or SOOT_ROOT (apply_env),
        // or CWD. The same fallback chain is used with soot.json:
        // the config file sets it, or it defaults to the file's dir.

        if (cfg.common.project_root.empty()) {
            cfg.common.project_root = fs::current_path();
        } else if (cfg.common.project_root.is_relative()) {
            cfg.common.project_root = fs::absolute(cfg.common.project_root);
        }
        common::set_project_path(cfg.common.project_root);
        lg::info("PROJECT = {}", cfg.common.project_root.string());

        // ----- REPL decision -----

        const bool want_repl = cfg.compiler.repl.value_or(false);

        // ----- REPL shell configuration -----

        REPL::Config repl_config(cfg.common.platform);
        repl_config.asm_file_search_dirs.push_back(cfg.common.project_root.string());
        repl_config.per_game_history = true;

        auto startup_file =
            REPL::load_user_startup_file(cfg.compiler.user_profile, cfg.common.platform);

        // ----- Compiler -----

        std::unique_ptr<sootc::Compiler> compiler;
        try {
            compiler = std::make_unique<sootc::Compiler>(
                cfg, repl_config, cfg.compiler.user_profile,
                std::make_unique<REPL::Wrapper>(cfg.compiler.user_profile, repl_config,
                                                startup_file,
                                                /*nrepl_alive=*/false));
        } catch (const std::exception &e) {
            lg::error("Failed to create Compiler: {}", e.what());
            return 1;
        }

        // ----- Startup commands -----
        for (const auto &cmd : startup_file.run_before_listen) {
            compiler->handle_repl_string(cmd);
        }

        // ----- Do-scripts (--do / --run) -----
        //
        // Executed in order, in the same Compiler instance, before
        // input files. A failure aborts immediately (non-zero exit).
        // (e) inside a script exits with code 0.

        const bool had_do_scripts = !cfg.compiler.run_scripts.empty();

        for (const auto &script_path : cfg.compiler.run_scripts) {
            std::string script = file_util::read_text(script_path);
            if (script.empty()) {
                lg::error("Cannot read script: {}", script_path.string());
                compiler->save_sidbase();
                return 1;
            }
            auto status = compiler->handle_repl_string(script);
            if (status == sootc::ReplStatus::WANT_EXIT) {
                compiler->save_sidbase();
                return 0;
            }
            if (status == sootc::ReplStatus::ERR) {
                compiler->save_sidbase();
                return 1;
            }
        }

        // ----- Compile input files -----
        if (!inputs.empty()) {
            for (const auto &input_file : inputs) {
                lg::info("Processing: {}", input_file);

                auto result = compiler->compile_file(input_file);
                if (!result) {
                    lg::error("Failed: {} - {}", input_file, result.error());
                    continue;
                }

                if (!*result) {
                    lg::info("No binary output for: {}", input_file);
                    continue;
                }

                if (cfg.compiler.print_listing) { compiler->print_listing(**result); }

                compiler->save_binary(**result, cfg.compiler.out_dir);
                compiler->save_header(**result, cfg.compiler.out_dir);

                lg::info("Compiled: {}", input_file);
            }

            compiler->save_sidbase();

            if (!want_repl) return 0;
        } else if (!want_repl) {
            // No input files, no REPL. If at least one do-script ran
            // successfully, that is a normal exit, not "nothing to do".
            if (!had_do_scripts) {
                lg::error("Nothing to do: no input files, no --do/--run, and REPL is disabled. "
                          "Pass a file to compile, pass --do <script>, "
                          "or add --repl to open the REPL.");
                return 1;
            }
            compiler->save_sidbase();
            return 0;
        }

        // ----- REPL -----
        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "=== SOOT Compiler v1.0 ===\n");
        fmt::print("Type (help) for help, (e) to quit\n");

        sootc::ReplStatus status = sootc::ReplStatus::OK;
        while (status != sootc::ReplStatus::WANT_EXIT) {
            if (status == sootc::ReplStatus::WANT_RELOAD) {
                lg::info("Reloading compiler...");
                compiler = std::make_unique<sootc::Compiler>(
                    cfg, repl_config, cfg.compiler.user_profile,
                    std::make_unique<REPL::Wrapper>(cfg.compiler.user_profile, repl_config,
                                                    startup_file,
                                                    /*nrepl_alive=*/false));
                status = sootc::ReplStatus::OK;
            }

            std::string input = compiler->get_repl_input();
            if (!input.empty()) {
                status = compiler->handle_repl_string(input);
                compiler->save_repl_history();
            }
        }

        // ----- SIDBase on REPL exit -----
        compiler->save_sidbase();

        return 0;

    } catch (const std::exception &e) {
        lg::error("Fatal error: {}", e.what());
        return 1;
    }
}