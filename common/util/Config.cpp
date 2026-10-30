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

    std::optional<std::string> CommonConfig::set(const std::string    &name,
                                                 const script::Object &value) {
        if (name == "project-root")
            return "\"project-root\" is read-only (set it on the command line)";
        if (name == "sidbase") return "\"sidbase\" is read-only (set it on the command line)";
        if (name == "platform") return "\"platform\" is read-only (set it on the command line)";

        if (name == "log") {
            auto s = as_string(value, "log");
            if (!s) return s;
            auto lvl = parse_log_level(*s);
            if (!lvl) return fmt::format("(opt ...): unknown log level '{}'", *s);
            log_level = *lvl;
            return std::nullopt;
        }
        if (name == "log-file") {
            auto s = as_string(value, "log-file");
            if (!s) return s;
            log_file = *s;
            return std::nullopt;
        }
        if (name == "color") {
            auto s = as_string(value, "color");
            if (!s) return s;
            if (*s != "auto" && *s != "always" && *s != "never")
                return fmt::format("(opt ...): color must be auto|always|never, got '{}'", *s);
            color = *s;
            return std::nullopt;
        }
        if (name == "search-paths") {
            auto v = as_path_list(value, "search-paths");
            if (!v) return v.error();
            search_paths = *v;
            return std::nullopt;
        }
        return fmt::format("(opt ...): unknown key '{}'", name);
    }

    std::optional<script::Object> CommonConfig::get(const std::string &name) const {
        if (!g_interp) return std::nullopt;
        if (name == "project-root")
            return script::Object::make_string(project_root.string().c_str());
        if (name == "sidbase") return script::Object::make_string(sidbase.string().c_str());
        if (name == "platform")
            return script::Object::make_string(std::to_string(static_cast<int>(platform)).c_str());
        if (name == "log")
            return script::Object::make_string(std::string(log_level_name(log_level)).c_str());
        if (name == "log-file") return script::Object::make_string(log_file.string().c_str());
        if (name == "color") return script::Object::make_string(color.c_str());
        if (name == "search-paths") {
            std::vector<script::Object> items;
            for (const auto &p : search_paths)
                items.push_back(script::Object::make_string(p.string().c_str()));
            return script::build_list(items);
        }
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

    std::optional<std::string> CompilerConfig::set(const std::string    &name,
                                                   const script::Object &value) {
        if (name == "mode") {
            auto s = as_string(value, "mode");
            if (!s) return s;
            if (*s != "compile" && *s != "interpret" && *s != "hybrid")
                return fmt::format("(opt ...): mode must be compile|interpret|hybrid, got '{}'",
                                   *s);
            mode = *s;
            return std::nullopt;
        }
        if (name == "dump") {
            auto s = as_string(value, "dump");
            if (!s) return s;
            dump_ast = dump_ir = dump_asm = print_listing = false;
            std::stringstream ss(*s);
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
                    return fmt::format("(opt ...): unknown dump kind '{}'", item);
            }
            return std::nullopt;
        }
        if (name == "out-dir") {
            auto s = as_string(value, "out-dir");
            if (!s) return s;
            out_dir = *s;
            return std::nullopt;
        }
        if (name == "repl") {
            auto b = as_bool(value, "repl");
            if (!b) return b.error();
            repl = *b;
            return std::nullopt;
        }
        if (name == "profile") {
            auto s = as_string(value, "profile");
            if (!s) return s;
            user_profile = *s;
            return std::nullopt;
        }
        if (name == "runtime") {
            auto s = as_string(value, "runtime");
            if (!s) return s;
            runtime_url = (*s == "local") ? std::string{} : *s;
            return std::nullopt;
        }
        return fmt::format("(opt ...): unknown key '{}'", name);
    }

    std::optional<script::Object> CompilerConfig::get(const std::string &name) const {
        if (!g_interp) return std::nullopt;
        if (name == "mode") return script::Object::make_string(mode.c_str());
        if (name == "dump") {
            std::string s;
            if (dump_ast) s += s.empty() ? "ast" : ",ast";
            if (dump_ir) s += s.empty() ? "ir" : ",ir";
            if (dump_asm) s += s.empty() ? "asm" : ",asm";
            if (print_listing) s += s.empty() ? "listing" : ",listing";
            return script::Object::make_string(s.c_str());
        }
        if (name == "out-dir") return script::Object::make_string(out_dir.string().c_str());
        if (name == "repl") {
            if (!repl) return script::Object::make_null();
            return g_interp->true_or_false(*repl);
        }
        if (name == "profile") return script::Object::make_string(user_profile.c_str());
        if (name == "runtime")
            return script::Object::make_string(runtime_url.empty() ? "local" : runtime_url.c_str());
        return std::nullopt;
    }

    // ---------- HostConfig ----------

    const std::vector<FieldInfo> &HostConfig::fields() {
        static const std::vector<FieldInfo> kFields = {
            {"listen", "string", Scope::Host, false, "carbon: address to listen on (host:port)."},
        };
        return kFields;
    }

    std::optional<std::string> HostConfig::set(const std::string &name,
                                               const script::Object & /*value*/) {
        if (name == "listen") return "\"listen\" is read-only (set it on the command line)";
        return fmt::format("(opt ...): unknown key '{}'", name);
    }

    std::optional<script::Object> HostConfig::get(const std::string &name) const {
        if (!g_interp) return std::nullopt;
        if (name == "listen") return script::Object::make_string(listen.c_str());
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
                cfg.compiler.run_scripts.emplace_back(*v);
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

    // ---------- (opt) ----------

    namespace {

        void print_field(const Config &cfg, const FieldInfo &f) {
            std::optional<script::Object> v;
            switch (f.scope) {
            case Scope::Common: v = cfg.common.get(std::string(f.name)); break;
            case Scope::Compiler: v = cfg.compiler.get(std::string(f.name)); break;
            case Scope::Host: v = cfg.host.get(std::string(f.name)); break;
            }
            const std::string rendered = v ? v->print() : std::string("(unavailable)");
            fmt::print(stderr, "  {:<14} {:<10} {}\n", f.name, f.type, rendered);
        }

    } // namespace

    bool opt_list(Config &cfg) {
        fmt::print(stderr, "\n");
        fmt::print(stderr, "  {:<14} {:<10} {}\n", "KEY", "TYPE", "VALUE");
        fmt::print(stderr, "  {:-<14} {:-<10} {:-<20}\n", "", "", "");

        auto section = [&](const char *title, const std::vector<FieldInfo> &fields) {
            fmt::print(stderr, "\n  {}\n", title);
            for (const auto &f : fields) print_field(cfg, f);
        };

        section("Common", CommonConfig::fields());
        section("Compiler", CompilerConfig::fields());
        section("Host (reserved for carbon)", HostConfig::fields());

        fmt::print(stderr, "\n");
        return true;
    }

    bool opt_get(Config &cfg, const std::string &key) {
        auto try_find = [&](const std::vector<FieldInfo> &fields, auto get_fn) -> bool {
            for (const auto &f : fields) {
                if (f.name != key) continue;
                auto v = get_fn(std::string(f.name));
                fmt::print(stderr, "{} = {}\n", f.name, v ? v->print() : "(unavailable)");
                return true;
            }
            return false;
        };

        if (try_find(CommonConfig::fields(),
                     [&](const std::string &n) { return cfg.common.get(n); }))
            return true;
        if (try_find(CompilerConfig::fields(),
                     [&](const std::string &n) { return cfg.compiler.get(n); }))
            return true;
        if (try_find(HostConfig::fields(), [&](const std::string &n) { return cfg.host.get(n); }))
            return true;

        fmt::print(stderr, "(opt): unknown key '{}'\n", key);
        return false;
    }

    bool opt_set(Config &cfg, const std::string &key, const script::Object &value) {
        auto try_set = [&](const std::vector<FieldInfo> &fields,
                           auto                          set_fn) -> std::optional<bool> {
            for (const auto &f : fields) {
                if (f.name != key) continue;
                if (!f.mutable_in_repl) {
                    fmt::print(stderr, "(opt): '{}' is read-only; set it on the command line\n",
                               key);
                    return false;
                }
                auto err = set_fn(std::string(f.name), value);
                if (err) {
                    fmt::print(stderr, "(opt): {}\n", *err);
                    return false;
                }
                return true;
            }
            return std::nullopt;
        };

        if (auto r =
                try_set(CommonConfig::fields(), [&](const std::string &n, const script::Object &v) {
                    return cfg.common.set(n, v);
                })) {
            if (*r) fmt::print(stderr, "{} = {}\n", key, value.print());
            return *r;
        }
        if (auto r = try_set(CompilerConfig::fields(),
                             [&](const std::string &n, const script::Object &v) {
                                 return cfg.compiler.set(n, v);
                             })) {
            if (*r) fmt::print(stderr, "{} = {}\n", key, value.print());
            return *r;
        }
        if (auto r =
                try_set(HostConfig::fields(), [&](const std::string &n, const script::Object &v) {
                    return cfg.host.set(n, v);
                })) {
            if (*r) fmt::print(stderr, "{} = {}\n", key, value.print());
            return *r;
        }

        fmt::print(stderr, "(opt): unknown key '{}'\n", key);
        return false;
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

} // namespace common