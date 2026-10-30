// common/util/ProjectConfig.hpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "util/Config.hpp"

namespace common {

    enum class PathType { CWD, EXE, HOME, CONFIG, CACHE, SHARE, PROJECT };

    void                  set_project_path(const std::filesystem::path &path);
    std::filesystem::path get_path(PathType type);
    std::filesystem::path
    detect_project_root(std::filesystem::path start = std::filesystem::current_path());

    std::filesystem::path find_project_file(const std::string                        &filename,
                                            const std::filesystem::path              &base_dir,
                                            const std::vector<std::filesystem::path> &search_paths);

    std::filesystem::path find_config_file(const std::string &filename);
    std::filesystem::path get_absolute_path(const std::filesystem::path &path);
    std::string           get_file_path(const std::vector<std::string> &input);

    // ================================================================
    // SootProjectConfig
    // ================================================================
    //
    // Thin wrapper over common::Config. Its only purpose is to keep
    // the historical field names (runtime, sidbase, search_paths,
    // repl, project_root) working for the callers that already use
    // them, while the real values live in Config.
    //
    // New code should use Config directly. This struct will be
    // removed once every caller has migrated.
    struct SootProjectConfig {
        // The underlying config. Every field below proxies into it.
        Config config;

        // Proxy accessors, kept in sync with config.
        std::filesystem::path              project_root;
        std::string                        runtime;
        std::filesystem::path              sidbase;
        std::vector<std::filesystem::path> search_paths;
        bool                               repl = false;
        std::filesystem::path              project_root_field; // legacy field name

        // Resolve project_root and read soot.json / env, but do not
        // touch argv. CLI parsing is main()'s job: it calls
        // common::apply_cli(config, ...) after load(), so that CLI
        // wins over env and JSON.
        //
        // explicit_root: --project-root (empty = search).
        static SootProjectConfig load(const std::string &explicit_root);

        // Read only the JSON layer (no env, no CLI). Useful for
        // tests and for callers that build Config by hand.
        static SootProjectConfig read(const std::filesystem::path &json_path);

        static std::filesystem::path find(const std::filesystem::path &start);

        std::filesystem::path sidbase_absolute() const {
            return sidbase.is_absolute() ? sidbase : project_root / sidbase;
        }

        // Copy the current Config values into the proxy fields.
        // Called by load() and read() before returning.
        void refresh_proxies();
    };

} // namespace common