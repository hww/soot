// common/util/ProjectConfig.hpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace common {

    /// @brief Where a tool-owned resource lives.
    ///
    /// @details PROJECT is the user's project root, resolved once via
    ///          set_project_path() and never recomputed. It is the
    ///          anchor for every module id and for the prelude lookup.
    enum class PathType {
        CWD,    ///< Current working directory.
        EXE,    ///< Directory containing the running executable.
        HOME,   ///< User's home directory.
        CONFIG, ///< ~/.config/soot/ (or XDG equivalent).
        CACHE,  ///< ~/.cache/soot/ (or XDG equivalent).
        SHARE,  ///< /usr/local/share/soot/ (or platform equivalent).
        PROJECT ///< User's project root (set via set_project_path).
    };

    /// @brief Set the project root used by get_path(PathType::PROJECT).
    /// @details The path is stored absolute. Subsequent calls to
    ///          get_path(PathType::PROJECT) return this value.
    void set_project_path(const std::filesystem::path &path);

    /// @brief Get a tool-owned path of the given kind.
    std::filesystem::path get_path(PathType type);

    /// @brief Walk up from `start`, looking for soot.json.
    /// @return Absolute path to the directory containing soot.json,
    ///         or empty if not found.
    std::filesystem::path
    detect_project_root(std::filesystem::path start = std::filesystem::current_path());

    /// @brief Find a file by searching, in order:
    ///
    ///          1. Relative to `base_dir`, if given.
    ///          2. Relative to CWD.
    ///          3. Relative to EXE.
    ///          4. Relative to PROJECT.
    ///          5. Relative to each entry of PROJECT/search_paths.
    ///          6. Relative to CONFIG.
    ///          7. Relative to SHARE.
    ///
    /// @details A path starting with "./" or "../" is treated as
    ///          explicitly relative to `base_dir`: only step 1 is tried.
    ///          An absolute path is returned as-is if it exists.
    ///          The first existing file wins. Returns empty on failure.
    std::filesystem::path find_project_file(const std::string                        &filename,
                                            const std::filesystem::path              &base_dir,
                                            const std::vector<std::filesystem::path> &search_paths);

    /// @brief Find a config file in PROJECT, then CONFIG, then SHARE.
    std::filesystem::path find_config_file(const std::string &filename);

    /// @brief Absolute, normalized path. Throws if the file does not exist.
    std::filesystem::path get_absolute_path(const std::filesystem::path &path);

    std::string get_file_path(const std::vector<std::string> &input);
    // ================================================================
    // SootProjectConfig — parsed contents of soot.json.
    // ================================================================

    struct SootProjectConfig {
        std::filesystem::path              project_root;
        std::string                        runtime = "local";
        std::filesystem::path              sidbase = "build/sidbase.bin";
        std::vector<std::filesystem::path> search_paths = {".", "scripts", "src", "examples",
                                                           "soot_src"};

        /// @brief Walk up from `start` looking for soot.json.
        static std::filesystem::path find(const std::filesystem::path &start);

        /// @brief Read soot.json at `json_path`.
        /// @details On any parse error, returns defaults with
        ///          project_root set to json_path.parent_path().
        static SootProjectConfig read(const std::filesystem::path &json_path);

        /// @brief Resolve project_root, read soot.json, apply defaults.
        ///
        /// @details Single entry point for sootc, carbon, and any other
        ///          tool that needs project config:
        ///
        ///            1. explicit_root (--project-root)
        ///            2. SOOT_ROOT environment variable
        ///            3. find(soot.json) from CWD
        ///            4. CWD
        ///
        ///          Then reads soot.json (if present) and fills the
        ///          remaining fields. Missing fields keep their defaults.
        ///
        ///          The returned project_root is always absolute and never
        ///          empty. The caller is expected to call
        ///          common::set_project_path(cfg.project_root) before
        ///          using get_path(PathType::PROJECT).
        static SootProjectConfig load(const std::string &explicit_root);

        /// @brief Full path to the SID base.
        std::filesystem::path sidbase_absolute() const {
            return sidbase.is_absolute() ? sidbase : project_root / sidbase;
        }
    };

} // namespace common