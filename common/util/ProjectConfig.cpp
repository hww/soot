// common/util/ProjectConfig.cpp
#include "util/ProjectConfig.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace common {

    // ================================================================
    // Project root + paths
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

        if (file.is_absolute()) { return fs::exists(file) ? file : fs::path{}; }

        const bool explicit_relative = filename.starts_with("./") || filename.starts_with("../");
        if (explicit_relative) {
            if (base_dir.empty()) return {};
            fs::path candidate = (base_dir / file).lexically_normal();
            return fs::exists(candidate) ? candidate : fs::path{};
        }

        if (!base_dir.empty()) {
            fs::path candidate = (base_dir / file).lexically_normal();
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
        for (auto &str : input) { current_path /= str; }
        return current_path.string();
    }

    // ================================================================
    // SootProjectConfig
    // ================================================================

    namespace {

        // --- Minimal JSON reader for soot.json ---------------------
        //
        // soot.json has a small, fixed schema:
        //   {
        //     "runtime": "local",
        //     "sidbase": "build/sidbase.bin",
        //     "search_paths": ["lib", "src", "examples", "soot_src"]
        //   }
        //
        // Rather than pull in a full JSON library for three keys, we
        // parse it by hand. The parser is intentionally strict about
        // syntax (trailing commas, comments, unquoted keys are rejected)
        // but tolerant of missing keys (defaults are used).

        std::string trim(const std::string &s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }

        size_t skip_ws(const std::string &s, size_t i) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
                ++i;
            }
            return i;
        }

        bool parse_string(const std::string &s, size_t &i, std::string &out) {
            i = skip_ws(s, i);
            if (i >= s.size() || s[i] != '"') return false;
            ++i;
            out.clear();
            while (i < s.size()) {
                char c = s[i++];
                if (c == '"') return true;
                if (c == '\\' && i < s.size()) {
                    char e = s[i++];
                    switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    default: out += e; break;
                    }
                } else {
                    out += c;
                }
            }
            return false;
        }

        struct Value {
            enum class Kind { STRING, ARRAY_OF_STRINGS, OTHER } kind = Kind::OTHER;
            std::string              str;
            std::vector<std::string> arr;
        };

        bool parse_value(const std::string &s, size_t &i, Value &out) {
            i = skip_ws(s, i);
            if (i >= s.size()) return false;

            if (s[i] == '"') {
                out.kind = Value::Kind::STRING;
                return parse_string(s, i, out.str);
            }

            if (s[i] == '[') {
                ++i;
                out.kind = Value::Kind::ARRAY_OF_STRINGS;
                while (true) {
                    i = skip_ws(s, i);
                    if (i >= s.size()) return false;
                    if (s[i] == ']') {
                        ++i;
                        return true;
                    }
                    std::string elem;
                    if (!parse_string(s, i, elem)) return false;
                    out.arr.push_back(std::move(elem));
                    i = skip_ws(s, i);
                    if (i < s.size() && s[i] == ',') {
                        ++i;
                        continue;
                    }
                    if (i < s.size() && s[i] == ']') {
                        ++i;
                        return true;
                    }
                    return false;
                }
            }

            size_t start = i;
            while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' &&
                   s[i] != '\t' && s[i] != '\r' && s[i] != '\n') {
                ++i;
            }
            out.kind = Value::Kind::OTHER;
            out.str = s.substr(start, i - start);
            return true;
        }

        bool parse_object(const std::string &s, size_t &i,
                          std::vector<std::pair<std::string, Value>> &out) {
            i = skip_ws(s, i);
            if (i >= s.size() || s[i] != '{') return false;
            ++i;
            while (true) {
                i = skip_ws(s, i);
                if (i >= s.size()) return false;
                if (s[i] == '}') {
                    ++i;
                    return true;
                }

                std::string key;
                if (!parse_string(s, i, key)) return false;
                i = skip_ws(s, i);
                if (i >= s.size() || s[i] != ':') return false;
                ++i;

                Value v;
                if (!parse_value(s, i, v)) return false;
                out.emplace_back(std::move(key), std::move(v));

                i = skip_ws(s, i);
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == '}') {
                    ++i;
                    return true;
                }
                return false;
            }
        }

    } // namespace

    fs::path SootProjectConfig::find(const fs::path &start) {
        fs::path         current = fs::absolute(start);
        constexpr size_t kMaxIterations = 100;
        size_t           iterations = 0;

        while (current.has_parent_path() && iterations < kMaxIterations) {
            if (fs::exists(current / "soot.json")) { return current; }
            fs::path parent = current.parent_path();
            if (parent == current) break;
            current = parent;
            ++iterations;
        }

        return {};
    }

    SootProjectConfig SootProjectConfig::read(const fs::path &json_path) {
        SootProjectConfig cfg;
        cfg.project_root = json_path.parent_path();

        std::ifstream in(json_path);
        if (!in.is_open()) {
            lg::warn("ProjectConfig: cannot open '{}' — using defaults", json_path.string());
            return cfg;
        }

        std::stringstream ss;
        ss << in.rdbuf();
        const std::string content = ss.str();

        size_t                                     i = 0;
        std::vector<std::pair<std::string, Value>> entries;
        if (!parse_object(content, i, entries)) {
            lg::warn("ProjectConfig: malformed JSON in '{}' — using defaults", json_path.string());
            return cfg;
        }

        for (auto &[key, v] : entries) {
            if (key == "runtime" && v.kind == Value::Kind::STRING) {
                cfg.runtime = v.str;
            } else if (key == "sidbase" && v.kind == Value::Kind::STRING) {
                cfg.sidbase = v.str;
            } else if (key == "search_paths" && v.kind == Value::Kind::ARRAY_OF_STRINGS) {
                cfg.search_paths.clear();
                for (auto &s : v.arr) cfg.search_paths.emplace_back(s);
            } else {
                lg::warn("ProjectConfig: unknown key '{}' in '{}' — ignored", key,
                         json_path.string());
            }
        }

        lg::info("ProjectConfig: loaded '{}' (runtime='{}', sidbase='{}', search_paths={})",
                 json_path.string(), cfg.runtime, cfg.sidbase.string(), cfg.search_paths.size());

        return cfg;
    }

    SootProjectConfig SootProjectConfig::load(const std::string &explicit_root) {
        SootProjectConfig cfg;

        fs::path root;
        if (!explicit_root.empty()) {
            root = fs::absolute(explicit_root);
        } else if (const char *env = std::getenv("SOOT_ROOT")) {
            root = fs::absolute(env);
        } else {
            root = find(fs::current_path());
            if (root.empty()) { root = fs::current_path(); }
        }

        const auto json_path = root / "soot.json";
        if (fs::exists(json_path)) {
            cfg = read(json_path);
        } else {
            lg::info("ProjectConfig: '{}' not found — using defaults", json_path.string());
        }

        cfg.project_root = root;
        return cfg;
    }

} // namespace common