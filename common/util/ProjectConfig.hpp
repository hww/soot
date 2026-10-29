// common/util/ProjectConfig.hpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sootc {

    /// Parsed contents of `soot.json` plus the project root that
    /// contains it.
    ///
    /// This is a *user project* concept: it describes where the user's
    /// modules live, which runtime to use, and where the SID base is.
    /// It is deliberately separate from file_util, which only knows
    /// about primitive path operations.
    struct SootProjectConfig {
        /// Root of the user project. Set by find() or by main() when
        /// the user passed --project-root explicitly.
        std::filesystem::path project_root;

        /// Runtime backend. "local" (default), "null", or "host:port".
        std::string runtime = "local";

        /// SID base path, relative to project_root (or absolute).
        std::filesystem::path sidbase = "build/sidbase.bin";

        /// PROJECT-relative search paths. Used by require and by the
        /// prelude lookup.
        std::vector<std::filesystem::path> search_paths = {".", "scripts", "src", "examples",
                                                           "soot_src"};

        /// @brief Walk up from `start` looking for `soot.json`.
        /// @return Absolute path to the directory containing soot.json,
        ///         or an empty path if not found.
        static std::filesystem::path find(const std::filesystem::path &start);

        /// @brief Read soot.json at `json_path`.
        /// @details On any parse error, returns a default-constructed
        ///          config with project_root set to json_path.parent_path()
        ///          and a warning logged. Never throws: a malformed
        ///          soot.json must not prevent the tool from starting.
        static SootProjectConfig read(const std::filesystem::path &json_path);

        /// @brief Resolve project_root, read soot.json, apply defaults.
        ///
        /// @details This is the SINGLE entry point used by both sootc
        ///          and carbon. It applies the same resolution rules
        ///          to both:
        ///
        ///            1. `explicit_root` (--project-root), if non-empty
        ///            2. SOOT_ROOT environment variable
        ///            3. find(soot.json) from current working directory
        ///            4. current working directory
        ///
        ///          Then it reads soot.json (if present) and fills the
        ///          remaining fields. Missing fields keep their defaults.
        ///
        ///          The returned project_root is always absolute and
        ///          never empty. The caller is expected to call
        ///          file_util::set_project_path(cfg.project_root)
        ///          before using any file_util helper that depends on
        ///          PathType::PROJECT.
        ///
        /// @param explicit_root  Value of --project-root, or empty.
        /// @return Config with project_root always set.
        static SootProjectConfig load(const std::string &explicit_root);

        /// @brief Full path to the SID base.
        /// @details If `sidbase` is absolute, it is returned as-is.
        ///          Otherwise it is project_root / sidbase.
        std::filesystem::path sidbase_absolute() const {
            return sidbase.is_absolute() ? sidbase : project_root / sidbase;
        }
    };

} // namespace sootc