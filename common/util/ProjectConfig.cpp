// common/util/ProjectConfig.cpp
#include "util/ProjectConfig.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"

#include <cstdlib>
#include <string>

namespace fs = std::filesystem;

namespace common {

    // ================================================================
    // Paths (unchanged)
    // ================================================================

    namespace {
        fs::path g_project_path;
    } // namespace

    void set_project_path(const fs::path &path) {
        if (fs::exists(path) && fs::is_directory(path)) { g_project_path = fs::absolute(path); }
    }

    fs::path get_path(PathType type) {
        switch (type) {
        case PathType::CWD: return fs::current_path();
        case PathType::EXE: return file_util::get_executable_dir();
        case PathType::PROJECT:
            return g_project_path.empty() ? (g_project_path = detect_project_root())
                                          : g_project_path;
        case PathType::HOME: {
            const char *home = std::getenv("HOME");
            return home ? fs::path(home) : fs::current_path();
        }
        case PathType::CONFIG: {
            const char *xdg = std::getenv("XDG_CONFIG_HOME");
            if (xdg) return fs::path(xdg) / "soot";
            return get_path(PathType::HOME) / ".config" / "soot";
        }
        case PathType::CACHE: {
            const char *xdg = std::getenv("XDG_CACHE_HOME");
            if (xdg) return fs::path(xdg) / "soot";
            return get_path(PathType::HOME) / ".cache" / "soot";
        }
        case PathType::SHARE: return "/usr/local/share/soot";
        }
        return fs::current_path();
    }

    fs::path detect_project_root(fs::path start) {
        fs::path         current = fs::absolute(start);
        constexpr size_t kMaxIterations = 100;
        size_t           iterations = 0;

        while (current.has_parent_path() && iterations < kMaxIterations) {
            if (fs::exists(current / "soot.json") || fs::exists(current / ".soot-root") ||
                fs::exists(current / ".git")) {
                return current;
            }
            fs::path parent = current.parent_path();
            if (parent == current) break;
            current = parent;
            ++iterations;
        }

        lg::debug("Project root not found, falling back to: {}", fs::current_path().string());
        return fs::current_path();
    }

    fs::path find_project_file(const std::string &filename, const fs::path &base_dir,
                               const std::vector<fs::path> &search_paths) {
        const fs::path file(filename);

        if (file.is_absolute()) return fs::exists(file) ? file : fs::path{};

        fs::path abs_base = base_dir;
        if (!abs_base.empty() && abs_base.is_relative()) {
            abs_base = get_path(PathType::PROJECT) / abs_base;
        }

        const bool explicit_relative = filename.starts_with("./") || filename.starts_with("../");
        if (explicit_relative) {
            if (abs_base.empty()) return {};
            fs::path candidate = (abs_base / file).lexically_normal();
            return fs::exists(candidate) ? candidate : fs::path{};
        }

        if (!abs_base.empty()) {
            fs::path candidate = (abs_base / file).lexically_normal();
            if (fs::exists(candidate)) return candidate;
        }
        {
            fs::path candidate = fs::current_path() / file;
            if (fs::exists(candidate)) return candidate;
        }
        {
            fs::path candidate = file_util::get_executable_dir() / file;
            if (fs::exists(candidate)) return candidate;
        }
        const fs::path project_root = get_path(PathType::PROJECT);
        if (!project_root.empty()) {
            fs::path candidate = project_root / file;
            if (fs::exists(candidate)) return candidate;
        }
        if (!project_root.empty()) {
            for (const auto &dir : search_paths) {
                fs::path candidate = project_root / dir / file;
                if (fs::exists(candidate)) return candidate;
            }
        }
        {
            fs::path candidate = get_path(PathType::CONFIG) / file;
            if (fs::exists(candidate)) return candidate;
        }
        {
            fs::path candidate = get_path(PathType::SHARE) / file;
            if (fs::exists(candidate)) return candidate;
        }
        return {};
    }

    fs::path find_config_file(const std::string &filename) {
        fs::path p = get_path(PathType::PROJECT) / filename;
        if (fs::exists(p)) return p;
        p = get_path(PathType::CONFIG) / filename;
        if (fs::exists(p)) return p;
        p = get_path(PathType::SHARE) / filename;
        if (fs::exists(p)) return p;
        return {};
    }

    fs::path get_absolute_path(const fs::path &path) {
        if (!fs::exists(path)) {
            throw std::runtime_error("File does not exist: " + path.string());
        }
        return fs::absolute(path).lexically_normal();
    }

    std::string get_file_path(const std::vector<std::string> &input) {
        if (input.size() == 1 && fs::path(input.at(0)).is_absolute()) { return input.at(0); }
        auto current_path = get_path(PathType::PROJECT);
        for (auto &str : input) current_path /= str;
        return current_path.string();
    }

    // ================================================================
    // SootProjectConfig
    // ================================================================

    fs::path SootProjectConfig::find(const fs::path &start) {
        fs::path         current = fs::absolute(start);
        constexpr size_t kMaxIterations = 100;
        size_t           iterations = 0;

        while (current.has_parent_path() && iterations < kMaxIterations) {
            if (fs::exists(current / "soot.json")) return current;
            fs::path parent = current.parent_path();
            if (parent == current) break;
            current = parent;
            ++iterations;
        }
        return {};
    }

    void SootProjectConfig::refresh_proxies() {
        project_root = config.common.project_root;
        sidbase = config.common.sidbase;

        runtime = config.compiler.runtime_url.empty() ? "local" : config.compiler.runtime_url;

        search_paths.clear();
        for (const auto &p : config.common.search_paths) search_paths.push_back(p);

        if (config.compiler.repl) repl = *config.compiler.repl;
    }

    SootProjectConfig SootProjectConfig::read(const fs::path &json_path) {
        SootProjectConfig out;
        if (auto err = load_json(std::string{}, json_path.string(), out.config)) {
            lg::warn("ProjectConfig: {} — using defaults", *err);
        }
        out.refresh_proxies();
        return out;
    }

    SootProjectConfig SootProjectConfig::load(const std::string &explicit_root) {
        SootProjectConfig out;

        // 1. Resolve the location of soot.json and read it.
        //    A missing soot.json is not an error: defaults are kept.
        if (auto err = load_json(std::string{}, explicit_root, out.config)) {
            lg::warn("ProjectConfig: {} — using defaults", *err);
        }

        // 2. Apply environment. CLI is applied later by main(),
        //    because SootProjectConfig does not own argv.
        if (auto env_err = apply_env(out.config)) {
            lg::warn("ProjectConfig: {} — env ignored", *env_err);
        }

        // 3. Ensure project_root is absolute and non-empty.
        if (out.config.common.project_root.empty()) {
            out.config.common.project_root = fs::current_path();
        } else if (out.config.common.project_root.is_relative()) {
            out.config.common.project_root = fs::absolute(out.config.common.project_root);
        }

        // 4. Project-root field: if soot.json has "project-root", it
        //    overrides the directory of soot.json. This is already
        //    handled inside load_json (which sets common.project_root
        //    from the JSON if present). Keep the legacy field name
        //    for callers that inspect it.
        out.project_root_field = out.config.common.project_root;

        out.refresh_proxies();

        lg::info("ProjectConfig: loaded (runtime='{}', sidbase='{}', "
                 "search_paths={}, repl={})",
                 out.runtime, out.sidbase.string(), out.search_paths.size(),
                 out.repl ? "true" : "false");

        return out;
    }

} // namespace common