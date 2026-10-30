// common/util/Config.cpp
#include "util/Config.hpp"

#include "util/FileUtil.hpp"
#include "util/JsonUtil.hpp"
#include "util/ProjectConfig.hpp"

#include "script/Interpreter.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <sstream>

namespace fs = std::filesystem;

namespace common {

    // ---------- helpers ----------

    namespace {

        std::string to_lower(std::string s) {
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        std::optional<lg::level> parse_log_level(const std::string &name) {
            const std::string n = to_lower(name);
            if (n == "trace") return lg::level::trace;
            if (n == "debug") return lg::level::debug;
            if (n == "info") return lg::level::info;
            if (n == "warn" || n == "warning") return lg::level::warn;
            if (n == "error") return lg::level::error;
            return std::nullopt;
        }

        std::string_view log_level_name(lg::level l) {
            switch (l) {
            case lg::level::trace: return "trace";
            case lg::level::debug: return "debug";
            case lg::level::info: return "info";
            case lg::level::warn: return "warn";
            case lg::level::error: return "error";
            case lg::level::die: return "die";
            case lg::level::off: return "off";
            case lg::level::off_unless_die: return "off_unless_die";
            }
            return "unknown";
        }

        std::optional<std::string> as_string(const script::Object &v, const char *what) {
            if (v.is_string()) return v.to_std_string();
            if (v.is_symbol()) return std::string(v.as_symbol().name_ptr);
            return fmt::format("(opt ...): '{}' expects a string, got {}", what, v.print());
        }

        std::expected<bool, std::string> as_bool(const script::Object &v, const char *what) {
            if (v.is_symbol()) {
                const std::string s = v.as_symbol().name_ptr;
                if (s == "#t" || s == "true") return true;
                if (s == "#f" || s == "false") return false;
            }
            if (v.is_integer()) return v.as_integer() != 0;
            return std::unexpected(
                fmt::format("(opt ...): '{}' expects #t or #f, got {}", what, v.print()));
        }

        std::expected<std::vector<fs::path>, std::string> as_path_list(const script::Object &v,
                                                                       const char           *what) {
            std::vector<fs::path> out;
            if (v.is_string()) {
                out.emplace_back(v.to_std_string());
                return out;
            }
            if (v.is_pair() || v.is_null()) {
                auto cur = v;
                while (cur.is_pair()) {
                    const auto &item = cur.as_pair()->car;
                    if (!item.is_string()) {
                        return std::unexpected(
                            fmt::format("(opt ...): '{}' expects a list of strings, got {}", what,
                                        item.print()));
                    }
                    out.emplace_back(item.to_std_string());
                    cur = cur.as_pair()->cdr;
                }
                if (!cur.is_null()) {
                    return std::unexpected(
                        fmt::format("(opt ...): '{}' expects a proper list", what));
                }
                return out;
            }
            return std::unexpected(
                fmt::format("(opt ...): '{}' expects a string or list, got {}", what, v.print()));
        }

        script::Interpreter *g_interp = nullptr;

    } // namespace

    void set_repl_interpreter(script::Interpreter *interp) { g_interp = interp; }

    // ---------- CommonConfig ----------

    const std::vector<FieldInfo> &CommonConfig::fields() {
        static const std::vector<FieldInfo> kFields = {
            {"project-root", "path", Scope::Common, false,
             "Project root. All relative module paths resolve against it."},
            {"sidbase", "path", Scope::Common, false, "String-ID table: load before, save after."},
            {"platform", "enum", Scope::Common, false, "Target platform (SootPlatform)."},
            {"log", "enum", Scope::Common, true, "Log level: trace|debug|info|warn|error."},
            {"log-file", "path", Scope::Common, true,
             "Additional log destination. stderr is always on."},
            {"color", "enum", Scope::Common, true, "auto|always|never."},
        };
        return kFields;
    }

    std::optional<std::string> CommonConfig::set_string(const std::string &name,
                                                        const std::string &value) {
        if (name == "log") {
            auto lvl = parse_log_level(value);
            if (!lvl) return fmt::format("unknown log level '{}'", value);
            log_level = *lvl;
            return std::nullopt;
        }
        if (name == "log-file") {
            log_file = value;
            return std::nullopt;
        }
        if (name == "color") {
            if (value != "auto" && value != "always" && value != "never") {
                return fmt::format("color must be auto|always|never, got '{}'", value);
            }
            color = value;
            return std::nullopt;
        }
        if (name == "project-root" || name == "sidbase" || name == "platform") {
            return fmt::format("\"{}\" is read-only", name);
        }
        return fmt::format("unknown string field '{}'", name);
    }

    std::optional<std::string> CommonConfig::set_bool(const std::string &, bool) {
        return "CommonConfig has no bool fields";
    }

    std::optional<std::string> CommonConfig::set_int(const std::string &, int64_t) {
        return "CommonConfig has no int fields";
    }

    std::optional<std::string>
    CommonConfig::set_path_list(const std::string                        &name,
                                const std::vector<std::filesystem::path> &value) {
        if (name == "search-paths") {
            search_paths = value;
            return std::nullopt;
        }
        return fmt::format("unknown path-list field '{}'", name);
    }

    std::optional<std::string> CommonConfig::get_string(const std::string &name) const {
        if (name == "project-root") return project_root.string();
        if (name == "sidbase") return sidbase.string();
        if (name == "platform") return std::to_string(static_cast<int>(platform));
        if (name == "log") return std::string(log_level_name(log_level));
        if (name == "log-file") return log_file.string();
        if (name == "color") return color;
        return std::nullopt;
    }

    std::optional<bool> CommonConfig::get_bool(const std::string &) const { return std::nullopt; }

    std::optional<int64_t> CommonConfig::get_int(const std::string &) const { return std::nullopt; }

    std::optional<std::vector<std::filesystem::path>>
    CommonConfig::get_path_list(const std::string &name) const {
        if (name == "search-paths") return search_paths;
        return std::nullopt;
    }

    // ---------- CompilerConfig ----------

    const std::vector<FieldInfo> &CompilerConfig::fields() {
        static const std::vector<FieldInfo> kFields = {
            {"mode", "enum", Scope::Compiler, true, "compile|interpret|hybrid."},
            {"dump", "string", Scope::Compiler, true, "Comma-separated: ast,ir,asm,listing."},
            {"out-dir", "path", Scope::Compiler, true, "Output directory for .bin/.h."},
            {"search-paths", "path-list", Scope::Compiler, true, "Module search paths."},
            {"repl", "bool?", Scope::Compiler, true,
             "Open REPL after compiling. Unset = use soot.json."},
            {"profile", "string", Scope::Compiler, true, "User profile to load at startup."},
            {"runtime", "string", Scope::Compiler, true, "local|null|host:port."},
            {"do", "path-list", Scope::Compiler, false, "Script(s) to execute before compiling input files."},
        };
        return kFields;
    }

     std::optional<std::string> CompilerConfig::set_string(const std::string &name,
                                                          const std::string &value) {
        if (name == "mode") {
            if (value != "compile" && value != "interpret" && value != "hybrid") {
                return fmt::format("mode must be compile|interpret|hybrid, got '{}'", value);
            }
            mode = value;
            return std::nullopt;
        }
        if (name == "out-dir") {
            out_dir = value;
            return std::nullopt;
        }
        if (name == "profile") {
            user_profile = value;
            return std::nullopt;
        }
        if (name == "runtime") {
            runtime_url = (value == "local") ? std::string{} : value;
            return std::nullopt;
        }
        return fmt::format("unknown string field '{}'", name);
    }

    std::optional<std::string> CompilerConfig::set_bool(const std::string &name, bool value) {
        if (name == "repl") {
            repl = value;
            return std::nullopt;
        }
        return fmt::format("unknown bool field '{}'", name);
    }

    std::optional<std::string> CompilerConfig::set_int(const std::string &, int64_t) {
        return "CompilerConfig has no int fields";
    }

    std::optional<std::string>
    CompilerConfig::set_path_list(const std::string                        &name,
                                  const std::vector<std::filesystem::path> &value) {
        if (name == "do") {
            do_files = value;
            return std::nullopt;
        }
        return fmt::format("unknown path-list field '{}'", name);
    }

    std::optional<std::string> CompilerConfig::get_string(const std::string &name) const {
        if (name == "mode") return mode;
        if (name == "out-dir") return out_dir.string();
        if (name == "profile") return user_profile;
        if (name == "runtime") return runtime_url.empty() ? "local" : runtime_url;
        return std::nullopt;
    }

    std::optional<bool> CompilerConfig::get_bool(const std::string &name) const {
        if (name == "repl") {
            if (!repl) return std::nullopt;
            return *repl;
        }
        return std::nullopt;
    }

    std::optional<int64_t> CompilerConfig::get_int(const std::string &) const {
        return std::nullopt;
    }

    std::optional<std::vector<std::filesystem::path>>
    CompilerConfig::get_path_list(const std::string &name) const {
        if (name == "do") return do_files;
        return std::nullopt;
    }

        std::optional<std::string> CompilerConfig::set_dump(const std::string &value) {
        dump_ast = dump_ir = dump_asm = print_listing = false;
        std::stringstream ss(value);
        std::string       item;
        while (std::getline(ss, item, ',')) {
            if (item == "ast") dump_ast = true;
            else if (item == "ir")
                dump_ir = true;
            else if (item == "asm")
                dump_asm = true;
            else if (item == "listing")
                print_listing = true;
            else
                return fmt::format("unknown dump kind '{}'", item);
        }
        return std::nullopt;
    }

    std::optional<std::string> CompilerConfig::get_dump() const {
        std::string s;
        if (dump_ast) s += s.empty() ? "ast" : ",ast";
        if (dump_ir) s += s.empty() ? "ir" : ",ir";
        if (dump_asm) s += s.empty() ? "asm" : ",asm";
        if (print_listing) s += s.empty() ? "listing" : ",listing";
        return s;
    }

    // ---------- HostConfig ----------

    const std::vector<FieldInfo> &HostConfig::fields() {
        static const std::vector<FieldInfo> kFields = {
            {"listen", "string", Scope::Host, true, "Address to listen on (host:port).", ""},
            {"serve", "bool", Scope::Host, true, "Run the HTTP/REPL host loop.", "true|false"},
            {"hz", "int", Scope::Host, true, "Tick rate in Hz.", ""},
            {"ticks", "int", Scope::Host, true, "Tick budget; -1 = unlimited.", ""},
            {"run", "string", Scope::Host, true, "State-script name to spawn.", ""},
            {"load", "path-list", Scope::Host, false, ".bin files to load explicitly.", ""},
        };
        return kFields;
    }

    std::optional<std::string> HostConfig::set_string(const std::string &name,
                                                      const std::string &value) {
        if (name == "listen") {
            listen = value;
            return std::nullopt;
        }
        if (name == "run") {
            run_state_script = value;
            return std::nullopt;
        }
        return fmt::format("unknown string field '{}'", name);
    }

    std::optional<std::string> HostConfig::set_bool(const std::string &name, bool value) {
        if (name == "serve") {
            serve = value;
            return std::nullopt;
        }
        return fmt::format("unknown bool field '{}'", name);
    }

    std::optional<std::string> HostConfig::set_int(const std::string &name, int64_t value) {
        if (name == "hz") {
            hz = static_cast<int>(value);
            return std::nullopt;
        }
        if (name == "ticks") {
            ticks = value;
            return std::nullopt;
        }
        return fmt::format("unknown int field '{}'", name);
    }

    std::optional<std::string>
    HostConfig::set_path_list(const std::string                        &name,
                              const std::vector<std::filesystem::path> &value) {
        if (name == "load") {
            load_files = value;
            return std::nullopt;
        }
        return fmt::format("unknown path-list field '{}'", name);
    }

    std::optional<std::string> HostConfig::get_string(const std::string &name) const {
        if (name == "listen") return listen;
        if (name == "run") return run_state_script;
        return std::nullopt;
    }

    std::optional<bool> HostConfig::get_bool(const std::string &name) const {
        if (name == "serve") return serve;
        return std::nullopt;
    }

    std::optional<int64_t> HostConfig::get_int(const std::string &name) const {
        if (name == "hz") return hz;
        if (name == "ticks") return ticks;
        return std::nullopt;
    }

    std::optional<std::vector<std::filesystem::path>>
    HostConfig::get_path_list(const std::string &name) const {
        if (name == "load") return load_files;
        return std::nullopt;
    }

    // ---------- Config ----------

    std::vector<FieldInfo> Config::all_fields() {
        std::vector<FieldInfo> out;
        for (const auto &f : CommonConfig::fields()) out.push_back(f);
        for (const auto &f : CompilerConfig::fields()) out.push_back(f);
        for (const auto &f : HostConfig::fields()) out.push_back(f);
        return out;
    }

    // ---------- JSON ----------

    namespace {

        std::optional<std::string> read_common(const nlohmann::json &j, CommonConfig &c,
                                               const std::filesystem::path &json_dir) {
            if (j.contains("project-root")) {
                std::filesystem::path p = j.at("project-root").get<std::string>();
                if (p.is_relative()) p = json_dir / p;
                std::error_code ec;
                p = std::filesystem::weakly_canonical(p, ec);
                if (ec) p = std::filesystem::absolute(p);
                c.project_root = p;
            }
            if (j.contains("sidbase")) c.sidbase = j.at("sidbase").get<std::string>();
            if (j.contains("log")) {
                const auto s = j.at("log").get<std::string>();
                auto       lvl = parse_log_level(s);
                if (!lvl) return fmt::format("soot.json: unknown log level '{}'", s);
                c.log_level = *lvl;
            }
            if (j.contains("log-file")) c.log_file = j.at("log-file").get<std::string>();
            if (j.contains("color")) c.color = j.at("color").get<std::string>();
            if (j.contains("search-paths")) {
                c.search_paths.clear();
                for (const auto &p : j.at("search-paths"))
                    c.search_paths.emplace_back(p.get<std::string>());
            }
            return std::nullopt;
        }

        std::optional<std::string> read_compiler(const nlohmann::json &j, CompilerConfig &c) {
            if (j.contains("mode")) {
                const auto s = j.at("mode").get<std::string>();
                if (s != "compile" && s != "interpret" && s != "hybrid")
                    return fmt::format("soot.json: unknown mode '{}'", s);
                c.mode = s;
            }
            if (j.contains("dump")) {
                c.dump_ast = c.dump_ir = c.dump_asm = c.print_listing = false;
                std::stringstream ss(j.at("dump").get<std::string>());
                std::string       item;
                while (std::getline(ss, item, ',')) {
                    if (item == "ast") c.dump_ast = true;
                    else if (item == "ir")
                        c.dump_ir = true;
                    else if (item == "asm")
                        c.dump_asm = true;
                    else if (item == "listing")
                        c.print_listing = true;
                    else
                        return fmt::format("soot.json: unknown dump kind '{}'", item);
                }
            }
            if (j.contains("out-dir")) c.out_dir = j.at("out-dir").get<std::string>();
            if (j.contains("repl")) c.repl = j.at("repl").get<bool>();
            if (j.contains("profile")) c.user_profile = j.at("profile").get<std::string>();
            if (j.contains("runtime")) {
                const auto s = j.at("runtime").get<std::string>();
                c.runtime_url = (s == "local") ? std::string{} : s;
            }
            return std::nullopt;
        }

        std::optional<std::string> read_host(const nlohmann::json &j, HostConfig &h) {
            if (j.contains("listen")) h.listen = j.at("listen").get<std::string>();
            return std::nullopt;
        }

    } // namespace

    std::optional<std::string> load_json(const std::string &explicit_config,
                                         const std::string &explicit_root, Config &cfg,
                                         fs::path *resolved_json_path) {
        fs::path json_path;

        if (!explicit_config.empty()) {
            json_path = fs::absolute(explicit_config);
            if (!fs::exists(json_path))
                return fmt::format("--config: file not found: {}", json_path.string());
        } else {
            fs::path dir;
            if (!explicit_root.empty()) dir = fs::absolute(explicit_root);
            else {
                dir = SootProjectConfig::find(fs::current_path());
                if (dir.empty()) dir = fs::current_path();
            }
            json_path = dir / "soot.json";
        }

        if (resolved_json_path) *resolved_json_path = json_path;

        if (!fs::exists(json_path)) {
            if (cfg.common.project_root.empty()) cfg.common.project_root = json_path.parent_path();
            return std::nullopt;
        }

        try {
            const std::string content = file_util::read_text(json_path);
            const auto        j = parse_commented_json(content, json_path.string());

            if (!j.is_object())
                return fmt::format("soot.json: top-level value must be an object: {}",
                                   json_path.string());

            if (cfg.common.project_root.empty()) cfg.common.project_root = json_path.parent_path();

            if (j.contains("common")) {
                if (!j.at("common").is_object()) {
                    return "soot.json: \"common\" must be an object";
                }
                if (auto err = read_common(j.at("common"), cfg.common, json_path.parent_path())) {
                    return err;
                }
            }
            if (j.contains("compiler")) {
                if (!j.at("compiler").is_object())
                    return "soot.json: \"compiler\" must be an object";
                if (auto err = read_compiler(j.at("compiler"), cfg.compiler)) return err;
            }
            if (j.contains("host")) {
                if (!j.at("host").is_object()) return "soot.json: \"host\" must be an object";
                if (auto err = read_host(j.at("host"), cfg.host)) return err;
            }

            return std::nullopt;
        } catch (const std::exception &e) { return fmt::format("soot.json: {}", e.what()); }
    }

    // ---------- env ----------

    std::optional<std::string> apply_env(Config &cfg) {
        auto getenv_str = [](const char *name) -> std::optional<std::string> {
            const char *v = std::getenv(name);
            if (!v) return std::nullopt;
            return std::string(v);
        };

        if (auto v = getenv_str("SOOT_PROJECT_ROOT")) cfg.common.project_root = *v;
        if (auto v = getenv_str("SOOT_SIDBASE")) cfg.common.sidbase = *v;
        if (auto v = getenv_str("SOOT_LOG")) {
            auto lvl = parse_log_level(*v);
            if (!lvl) return fmt::format("SOOT_LOG: unknown level '{}'", *v);
            cfg.common.log_level = *lvl;
        }
        if (auto v = getenv_str("SOOT_LOG_FILE")) cfg.common.log_file = *v;
        if (auto v = getenv_str("SOOT_COLOR")) cfg.common.color = *v;

        if (auto v = getenv_str("SOOT_MODE")) {
            if (*v != "compile" && *v != "interpret" && *v != "hybrid")
                return fmt::format("SOOT_MODE: unknown mode '{}'", *v);
            cfg.compiler.mode = *v;
        }
        if (auto v = getenv_str("SOOT_RUNTIME"))
            cfg.compiler.runtime_url = (*v == "local") ? std::string{} : *v;
        if (auto v = getenv_str("SOOT_OUT_DIR")) cfg.compiler.out_dir = *v;

        return std::nullopt;
    }

    // ---------- CLI ----------

    namespace {
        bool is_option(const std::string &arg) { return arg.size() > 1 && arg[0] == '-'; }
    } // namespace

    std::optional<std::string> apply_cli(int argc, char *argv[], Config &cfg, Scope scope,
                                         std::vector<std::string> &inputs, bool &no_config) {
        bool end_of_options = false;
        bool config_given = false;
        no_config = false;

        auto need_value = [&](int i, const char *name) -> std::optional<std::string> {
            if (i + 1 >= argc) return fmt::format("{}: requires an argument", name);
            return std::string(argv[i + 1]);
        };

        // ----- Boolean option handler -----
        //
        // Every option whose FieldInfo::type is "bool" or "bool?"
        // accepts three forms: --name, --no-name, --name=true|false.
        // The rule is applied here once, so adding a new bool field
        // to a config struct is enough; no per-option parser code.
        auto set_bool = [&](const std::string &name, bool value) -> std::optional<std::string> {
            if (name == "repl") {
                cfg.compiler.repl = value;
                return std::nullopt;
            }
            return fmt::format("internal: no bool option '{}'", name);
        };

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            if (end_of_options) {
                inputs.push_back(arg);
                continue;
            }
            if (arg == "--") {
                end_of_options = true;
                continue;
            }

            // ----- Meta -----
            if (arg == "-h" || arg == "--help") {
                print_help(Scope::Compiler, "sootc");
                std::exit(0);
            }
            if (arg == "-V" || arg == "--version") {
                fmt::print("sootc v1.0\n");
                std::exit(0);
            }

            // ----- Config file control -----
            if (arg == "--config") {
                auto v = need_value(i, "--config");
                if (!v) return v;
                config_given = true;
                // --config is consumed by scan_early_args in main();
                // here we only remember that it was given, so that
                // --no-config can reject the combination.
                ++i;
                continue;
            }
            if (arg == "--no-config") {
                no_config = true;
                continue;
            }

            // ----- Boolean options with =value form -----
            //
            // Parse --name=value first, then --no-name, then --name.
            // The order matters: --no-repl=false means "enable repl".
            if (auto eq = arg.find('='); eq != std::string::npos) {
                const std::string name = arg.substr(0, eq);
                const std::string value = arg.substr(eq + 1);

                if (name == "--repl" || name == "--no-repl") {
                    if (value != "true" && value != "false") {
                        return fmt::format("{}: expected true|false, got '{}'", name, value);
                    }
                    const bool on = (value == "true");
                    return set_bool("repl", name == "--repl" ? on : !on);
                }
            }

            if (arg == "--repl" || arg == "-i") return set_bool("repl", true);
            if (arg == "--no-repl") return set_bool("repl", false);

            // ----- Common -----
            if (arg == "--project-root" || arg == "--project") {
                auto v = need_value(i, "--project-root");
                if (!v) return v;
                cfg.common.project_root = *v;
                ++i;
                continue;
            }
            if (arg == "--sidbase") {
                auto v = need_value(i, "--sidbase");
                if (!v) return v;
                cfg.common.sidbase = *v;
                ++i;
                continue;
            }
            if (arg == "--log") {
                auto v = need_value(i, "--log");
                if (!v) return v;
                auto lvl = parse_log_level(*v);
                if (!lvl) return fmt::format("--log: unknown level '{}'", *v);
                cfg.common.log_level = *lvl;
                ++i;
                continue;
            }
            if (arg == "-q") {
                cfg.common.log_level = lg::level::error;
                continue;
            }
            if (arg == "-v") {
                cfg.common.log_level = lg::level::info;
                continue;
            }
            if (arg == "-vv") {
                cfg.common.log_level = lg::level::debug;
                continue;
            }
            if (arg == "--log-file") {
                auto v = need_value(i, "--log-file");
                if (!v) return v;
                cfg.common.log_file = *v;
                ++i;
                continue;
            }
            if (arg == "--color") {
                auto v = need_value(i, "--color");
                if (!v) return v;
                if (*v != "auto" && *v != "always" && *v != "never") {
                    return fmt::format("--color: must be auto|always|never, got '{}'", *v);
                }
                cfg.common.color = *v;
                ++i;
                continue;
            }
            if (arg == "-I" || arg == "--include") {
                auto v = need_value(i, "-I");
                if (!v) return v;
                cfg.common.search_paths.emplace_back(*v);
                ++i;
                continue;
            }
            // ----- Compiler -----
            if (arg == "--mode") {
                auto v = need_value(i, "--mode");
                if (!v) return v;
                if (*v != "compile" && *v != "interpret" && *v != "hybrid") {
                    return fmt::format("--mode: must be compile|interpret|hybrid, got '{}'", *v);
                }
                cfg.compiler.mode = *v;
                ++i;
                continue;
            }
            if (arg == "--dump") {
                auto v = need_value(i, "--dump");
                if (!v) return v;
                cfg.compiler.dump_ast = cfg.compiler.dump_ir = cfg.compiler.dump_asm =
                    cfg.compiler.print_listing = false;
                std::stringstream ss(*v);
                std::string       item;
                while (std::getline(ss, item, ',')) {
                    if (item == "ast") cfg.compiler.dump_ast = true;
                    else if (item == "ir")
                        cfg.compiler.dump_ir = true;
                    else if (item == "asm")
                        cfg.compiler.dump_asm = true;
                    else if (item == "listing")
                        cfg.compiler.print_listing = true;
                    else
                        return fmt::format("--dump: unknown kind '{}'", item);
                }
                ++i;
                continue;
            }
            if (arg == "--out-dir" || arg == "--target") {
                auto v = need_value(i, "--out-dir");
                if (!v) return v;
                cfg.compiler.out_dir = *v;
                ++i;
                continue;
            }
            if (arg == "--profile") {
                auto v = need_value(i, "--profile");
                if (!v) return v;
                cfg.compiler.user_profile = *v;
                ++i;
                continue;
            }
            if (arg == "--runtime") {
                auto v = need_value(i, "--runtime");
                if (!v) return v;
                cfg.compiler.runtime_url = (*v == "local") ? std::string{} : *v;
                ++i;
                continue;
            }

            // ----- Do-scripts -----
            if (arg == "--do" || arg == "--run") {
                auto v = need_value(i, arg.c_str());
                if (!v) return v;
                cfg.compiler.do_files.emplace_back(*v);
                ++i;
                continue;
            }

            // ----- Positional -----
            if (!is_option(arg)) {
                inputs.push_back(arg);
                continue;
            }

            // Recognise options from the other tool and report them as
            // such, instead of "unknown option". A user who types
            // `carbon --mode hybrid` should see that --mode belongs to
            // sootc, not to carbon.
            for (const auto &f : Config::all_fields()) {
                if (f.scope == scope || f.scope == Scope::Common) continue;
                if (arg == "--" + std::string(f.name)) {
                    const char *owner = (f.scope == Scope::Compiler) ? "sootc" : "carbon";
                    const char *mine = (scope == Scope::Compiler) ? "sootc" : "carbon";
                    return fmt::format("{} is a {} option, not a {} option", arg, owner, mine);
                }
            }
            return fmt::format("unknown option '{}'. Try '{} --help'", arg,
                               (scope == Scope::Compiler) ? "sootc" : "carbon");
        }

        if (no_config && config_given) { return "--config and --no-config are mutually exclusive"; }

        return std::nullopt;
    }

    // ---------- help ----------

    void print_help(Scope scope, const char *program_name) {
        fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "sootc\n\n");
        fmt::print("Usage: {} [options] [file...]\n\n", program_name);

        auto section = [&](const char *title, const std::vector<FieldInfo> &fields) {
            fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "{}\n", title);
            for (const auto &f : fields)
                fmt::print("  --{:<14} {:<10} {}\n", f.name, f.type, f.description);
            fmt::print("\n");
        };

        section("Common", CommonConfig::fields());
        if (scope == Scope::Compiler) section("Compiler", CompilerConfig::fields());
        else if (scope == Scope::Host)
            section("Host", HostConfig::fields());

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Meta\n");
        fmt::print("  -h, --help      Show this help\n");
        fmt::print("  -V, --version   Show version\n");
        fmt::print("  -v, -vv, -q     More / less log output\n");
        fmt::print("  --config FILE   Use FILE instead of searching for soot.json\n");
        fmt::print("\n");
        fmt::print("Priority: CLI > env > soot.json > defaults.\n");
        fmt::print("Inside the REPL, type (opt) to list all options.\n");
    }

    // ================================================================
    // CommonConfig — dump (no-op)
    // ================================================================
    //
    // set_dump/get_dump are declared on every struct because the
    // REPL dispatcher instantiates the same lambda for all three.
    // Only CompilerConfig has a real "dump" field; here they are
    // stubs.

    std::optional<std::string> CommonConfig::set_dump(const std::string &) {
        return "CommonConfig has no 'dump' field";
    }

    std::optional<std::string> CommonConfig::get_dump() const { return std::nullopt; }
    
    // ================================================================
    // HostConfig — dump (no-op)
    // ================================================================

    std::optional<std::string> HostConfig::set_dump(const std::string &) {
        return "HostConfig has no 'dump' field";
    }

    std::optional<std::string> HostConfig::get_dump() const { return std::nullopt; }

} // namespace common