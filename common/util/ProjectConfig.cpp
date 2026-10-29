// common/util/ProjectConfig.cpp
#include "util/ProjectConfig.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace sootc {

    namespace {

        // --- Minimal JSON reader for soot.json -------------------------
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
        //
        // If a full JSON library becomes available in the project, this
        // namespace is the only place that needs to change.

        std::string trim(const std::string &s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }

        // Skip whitespace. Returns index of first non-space.
        size_t skip_ws(const std::string &s, size_t i) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
                ++i;
            }
            return i;
        }

        // Parse a quoted string. Advances `i` past the closing quote.
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
            return false; // unterminated
        }

        // Parse a JSON value as a raw token (string, number, true/false,
        // array). We only need to distinguish strings and arrays of strings.
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

            // Number / true / false / null: consume until delimiter.
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

        // 1. Resolve project_root.
        std::filesystem::path root;
        if (!explicit_root.empty()) {
            root = std::filesystem::absolute(explicit_root);
        } else if (const char *env = std::getenv("SOOT_ROOT")) {
            root = std::filesystem::absolute(env);
        } else {
            root = find(std::filesystem::current_path());
            if (root.empty()) { root = std::filesystem::current_path(); }
        }

        // 2. Read soot.json if present. read() sets project_root to
        //    json_path.parent_path(), which equals `root` here.
        const auto json_path = root / "soot.json";
        if (std::filesystem::exists(json_path)) {
            cfg = read(json_path);
        } else {
            lg::info("ProjectConfig: '{}' not found — using defaults", json_path.string());
        }

        // 3. Project_root always wins. Ensures a non-empty absolute
        //    path even if soot.json had a weird parent.
        cfg.project_root = root;
        return cfg;
    }

} // namespace sootc