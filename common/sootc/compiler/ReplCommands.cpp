#include "sootc/compiler/ReplCommands.hpp"

#include "common/repl/CommandTable.hpp"
#include "common/repl/ReplWrapper.hpp"

#include "sootc/compiler/Compiler.hpp"
#include "sootc/compiler/ConfigRepl.hpp"
#include "sootc/compiler/NodeInspector.hpp"

#include "sootc/node/FileNode.hpp"
#include "sootc/node/GlobalNode.hpp"

#include "carbon/file/SizeAssertions.hpp"

#include "script/Object.hpp"

using namespace repl;

namespace sootc::repl_commands {


    const std::vector<CommandInfo> &commands() {
        static const std::vector<CommandInfo> kCommands = {
            // ----- Session -----
            {"help",
             "h",
             CommandGroup::Session,
             CommandMode::Both,
             "(help [cmd])",
             "show help, optionally for one command",
             {{"cmd", "string", "command name; omit for the full list"}},
             &Compiler::cmd_help},
            
            {"exit",
             "e",
             CommandGroup::Session,
             CommandMode::Both,
             "(exit)",
             "leave the REPL",
             {},
             &Compiler::cmd_e},
            
            {"clear",
             "",
             CommandGroup::Session,
             CommandMode::Both,
             "(clear)",
             "clear the screen",
             {},
             &Compiler::cmd_clear},
            
            {"reload",
             "",
             CommandGroup::Session,
             CommandMode::Both,
             "(reload)",
             "rebuild the compiler (keeps (opt) changes)",
             {},
             &Compiler::cmd_reload},

            // ----- Options -----
            {"opt",
             "",
             CommandGroup::Options,
             CommandMode::Both,
             "(opt [name [value]])",
             "list, read, or change an option",
             {{"name", "string", "option name; see the list below"},
              {"value", "any", "new value; type depends on the option"}},
             &Compiler::cmd_opt},
            
            {"opt-reset",
             "",
             CommandGroup::Options,
             CommandMode::Both,
             "(opt-reset [name])",
             "reset one option (or all) to its startup value",
             {{"name", "string", "option name; omit to reset every option"}},
             &Compiler::cmd_opt_reset},

            // ----- Run -----
            {"call",
             "",
             CommandGroup::Run,
             CommandMode::Compile,
             "(sc-run name arg...)",
             "call a compiled function",
             {{"name", "symbol", "function name"},
              {"arg", "any",
               "argument; integer -> i64, float -> f32, string -> string, symbol -> StringId"}},
             &Compiler::cmd_sc_run},
            
            {"eval",
             "ev",
             CommandGroup::Run,
             CommandMode::Compile,
             "(st-eval expr)",
             "evaluate in the interpreter",
             {{"expr", "any", "SOOT expression to evaluate in *global-env*"}},
             &Compiler::cmd_st_eval},

            // ----- Inspect -----
            {"envc",
             "",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(sc-env)",
             "dump *soot-env* (compiler)",
             {},
             &Compiler::cmd_sc_env},
            
            {"env",
             "",
             CommandGroup::Inspect,
             CommandMode::Both,
             "(st-env)",
             "dump *global-env* (interpreter)",
             {},
             &Compiler::cmd_st_env},
            
            {"macro?",
             "",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(sc-macro? name)",
             "is `name` a macro visible to the compiler?",
             {{"name", "symbol", "macro name to check"}},
             &Compiler::cmd_sc_macro_p},

            {"symbols",
             "sym",
             CommandGroup::Inspect,
             CommandMode::Both,
             "(symbols [filter])",
             "list Globals symbols, optionally filtered",
             {{"filter", "string", "substring to match; omit for all symbols"}},
             &Compiler::cmd_symbols},

            {"tinfo",
             "ti",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(tinfo type)",
             "print type info",
             {{"type", "symbol", "type name"}},
             &Compiler::cmd_ts_info},
            
            {"types",
             "",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(types)",
             "print types info",
             {},
             &Compiler::cmd_ts_list},

            {"ast",
             "",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(ast)",
             "dump the AST of compiled files",
             {},
             &Compiler::cmd_ast},

            {"sizes",
             "",
             CommandGroup::Inspect,
             CommandMode::Compile,
             "(sizes)",
             "print structure sizes",
             {},
             &Compiler::cmd_sizes},

            // ----- Save -----
            {"save-session",
             "ss",
             CommandGroup::Save,
             CommandMode::Compile,
             "(save-session [name])",
             "save the REPL session to <name>.bin/.h",
             {{"name", "string", "base name; default: the session name"}},
             &Compiler::cmd_save_session},

            {"save-module",
             "sm",
             CommandGroup::Save,
             CommandMode::Compile,
             "(save-module path [out])",
             "compile and save one .sc file as a module",
             {{"path", "path", "path to the .sc file"},
              {"out", "path", "output directory; default: compiler.out-dir"}},
             &Compiler::cmd_save_module},

            {"save-module-deps",
             "smr",
             CommandGroup::Save,
             CommandMode::Compile,
             "(save-module-deps path [out])",
             "save a module and all its dependencies",
             {{"path", "path", "path to the .sc file"},
              {"out", "path", "output directory; default: compiler.out-dir"}},
             &Compiler::cmd_save_module_deps},

            // ----- GC -----
            {"gc",
             "",
             CommandGroup::GC,
             CommandMode::Compile,
             "(gc)",
             "run a GC pass",
             {},
             &Compiler::cmd_gc_collect},

            {"gcls",
             "",
             CommandGroup::GC,
             CommandMode::Compile,
             "(gcls)",
             "list live GC objects (by type)",
             {},
             &Compiler::cmd_gc_list},
        };
        return kCommands;
    }

} // namespace sootc::repl_commands

namespace sootc {
    using namespace sootc::repl_commands;
    // ===============================================================
    // Session commands
    // ===============================================================

    // (help)        — list every command, grouped by CommandGroup
    // (help "cmd")  — show usage, summary, and old names for one command
    ReplStatus Compiler::cmd_help(const script::Object &form) {


        // ----- (help) -----
        auto rest = form.as_pair()->cdr;
        if (rest.is_null()) {
            fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nSOOTC REPL — help\n\n");

            // Walk the table once, group by CommandGroup. The groups
            // come in the order declared in CommandGroup; within a
            // group, commands appear in table order.
            const CommandGroup groups[] = {
                CommandGroup::Session, CommandGroup::Options, CommandGroup::Run,
                CommandGroup::Inspect, CommandGroup::Save,    CommandGroup::GC,
            };

            auto mode = m_config.compiler.mode;

            for (CommandGroup g : groups) {
                bool header_printed = false;
                for (const auto &c : commands()) {
                    if (c.group != g) continue;
                    if (!c.is_available_for(mode)) continue;

                    if (!header_printed) {
                        fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "\n── {} ──\n",
                                   group_name(g));
                        header_printed = true;
                    }

                    const std::string name =
                        c.short_name.empty() ? std::string(c.long_name)
                                             : fmt::format("{} ({})", c.long_name, c.short_name);

                    fmt::print("  {:<22} {}\n", name, c.summary);
                }
            }

            fmt::print("\n");
            fmt::print("Type (help \"command\") for details about one command.\n\n");
            return ReplStatus::OK;
        }

        // ----- (help "cmd") -----
        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_string() && !name_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (help ...) name must be string or symbol\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();

        const CommandInfo *cmd = find_command(commands(), name, m_config.compiler.mode);
        if (!cmd) {
            fmt::print(fg(fmt::color::yellow), "; help: unknown command '{}'\n", name);
            return ReplStatus::ERR;
        }

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\n{}\n", cmd->long_name);
        if (!cmd->short_name.empty()) { fmt::print("  alias:  {}\n", cmd->short_name); }
        fmt::print("  group:  {}\n", group_name(cmd->group));
        fmt::print("  usage:  {}\n", cmd->usage);
        fmt::print("\n  {}\n\n", cmd->summary);

        if (!cmd->params.empty()) {
            fmt::print("  Parameters:\n");
            for (const auto &p : cmd->params) {
                fmt::print("    {:<10} {:<8} {}\n", std::string(p.name), std::string(p.type),
                           std::string(p.description));
            }
            fmt::print("\n");
        }

        // `opt` takes option names as its first argument; list them
        // here so the user does not have to guess. Other commands
        // describe their arguments in `params`.
        if (cmd->long_name == "opt") {
            fmt::print("  Options (from Config::fields()):\n");
            auto section = [](const char *title, const auto &fields) {
                fmt::print("    ── {} ──\n", title);
                for (const auto &f : fields) {
                    const std::string allowed =
                        f.allowed_values.empty()
                            ? ""
                            : std::string(" [") + std::string(f.allowed_values) + "]";
                    fmt::print("    {:<14} {:<10}{}  {}\n", std::string(f.name),
                               std::string(f.type), allowed, std::string(f.description));
                }
            };
            section("Common", common::CommonConfig::fields());
            section("Compiler", common::CompilerConfig::fields());
            section("Host", common::HostConfig::fields());
            fmt::print("\n");
        }

        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_e(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_EXIT;
    }

    ReplStatus Compiler::cmd_clear(const script::Object &form) {
        (void)form;
        if (m_repl) m_repl->clear_screen();
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_reload(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_RELOAD;
    }

    // ===============================================================
    // Options
    // ===============================================================

    // (opt)             — list every option
    // (opt "key")       — print one option
    // (opt "key" value) — change one option in memory
    ReplStatus Compiler::cmd_opt(const script::Object &form) {
        auto rest = form.as_pair()->cdr;

        // (opt)
        if (rest.is_null()) {
            opt_list(const_cast<common::Config &>(m_config));
            return ReplStatus::OK;
        }

        // (opt "key")
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (opt \"key\" [value]) requires a key\n");
            return ReplStatus::ERR;
        }

        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (opt ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }
        const std::string key = key_obj.to_std_string();

        auto after_key = rest.as_pair()->cdr;

        // (opt "key")
        if (after_key.is_null()) {
            if (!opt_get(const_cast<common::Config &>(m_config), key)) { return ReplStatus::ERR; }
            return ReplStatus::OK;
        }

        // (opt "key" value)
        if (!after_key.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (opt \"{}\" value) requires a value\n",
                       key);
            return ReplStatus::ERR;
        }
        const auto &value = after_key.as_pair()->car;

        if (!opt_set(const_cast<common::Config &>(m_config), key, value)) {
            return ReplStatus::ERR;
        }
        Type::log_level = m_config.common.log_level;
        return ReplStatus::OK;
    }

    // (opt-reset)      — reset every option to its startup value
    // (opt-reset "key") — reset one option
    ReplStatus Compiler::cmd_opt_reset(const script::Object &form) {
        auto rest = form.as_pair()->cdr;

        if (rest.is_null()) {
            const_cast<common::Config &>(m_config) = m_startup_config;
            Type::log_level = m_config.common.log_level;
            fmt::print(fg(fmt::color::green), "; opt-reset: all options restored\n");
            return ReplStatus::OK;
        }

        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (opt-reset ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }
        const std::string key = key_obj.to_std_string();

        const auto *field = [&]() -> const common::FieldInfo * {
            static const std::vector<common::FieldInfo> all = common::Config::all_fields();
            for (const auto &f : all) {
                if (f.name == key) return &f;
            }
            return nullptr;
        }();

        if (!field) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (opt-reset) unknown key '{}'\n", key);
            return ReplStatus::ERR;
        }
        if (!field->mutable_in_repl) {
            fmt::print(fg(fmt::color::yellow), "; ERROR: '{}' is read-only\n", key);
            return ReplStatus::ERR;
        }

        // Copy just this field from the startup snapshot, using the
        // typed getters/setters so we do not need to know the field
        // names here.
        common::Config &mutable_cfg = const_cast<common::Config &>(m_config);
        switch (field->scope) {
        case common::Scope::Common:
            if (field->type == "bool" || field->type == "bool?") {
                if (auto v = m_startup_config.common.get_bool(key)) {
                    mutable_cfg.common.set_bool(key, *v);
                }
            } else if (field->type == "int") {
                if (auto v = m_startup_config.common.get_int(key)) {
                    mutable_cfg.common.set_int(key, *v);
                }
            } else if (field->type == "path-list") {
                if (auto v = m_startup_config.common.get_path_list(key)) {
                    mutable_cfg.common.set_path_list(key, *v);
                }
            } else {
                if (auto v = m_startup_config.common.get_string(key)) {
                    mutable_cfg.common.set_string(key, *v);
                }
            }
            break;
        case common::Scope::Compiler:
            if (field->type == "bool" || field->type == "bool?") {
                if (auto v = m_startup_config.compiler.get_bool(key)) {
                    mutable_cfg.compiler.set_bool(key, *v);
                }
            } else if (field->type == "int") {
                if (auto v = m_startup_config.compiler.get_int(key)) {
                    mutable_cfg.compiler.set_int(key, *v);
                }
            } else if (field->type == "path-list") {
                if (auto v = m_startup_config.compiler.get_path_list(key)) {
                    mutable_cfg.compiler.set_path_list(key, *v);
                }
            } else {
                if (auto v = m_startup_config.compiler.get_string(key)) {
                    mutable_cfg.compiler.set_string(key, *v);
                }
            }
            break;
        case common::Scope::Host:
            if (field->type == "bool" || field->type == "bool?") {
                if (auto v = m_startup_config.host.get_bool(key)) {
                    mutable_cfg.host.set_bool(key, *v);
                }
            } else if (field->type == "int") {
                if (auto v = m_startup_config.host.get_int(key)) {
                    mutable_cfg.host.set_int(key, *v);
                }
            } else if (field->type == "path-list") {
                if (auto v = m_startup_config.host.get_path_list(key)) {
                    mutable_cfg.host.set_path_list(key, *v);
                }
            } else {
                if (auto v = m_startup_config.host.get_string(key)) {
                    mutable_cfg.host.set_string(key, *v);
                }
            }
            break;
        }

        Type::log_level = m_config.common.log_level;
        fmt::print(fg(fmt::color::green), "; opt-reset: '{}' restored\n", key);
        return ReplStatus::OK;
    }

    // ===============================================================
    // Run
    // ===============================================================

    ReplStatus Compiler::cmd_sc_run(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (sc-run name arg...) requires a name\n");
            return ReplStatus::ERR;
        }

        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (sc-run ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();

        std::vector<carbon::Variant> vm_args;
        auto                         args_rest = rest.as_pair()->cdr;
        while (args_rest.is_pair()) {
            const auto &o = args_rest.as_pair()->car;
            if (o.is_integer()) {
                vm_args.push_back(carbon::Variant(static_cast<i64>(o.as_integer())));
            } else if (o.is_float()) {
                vm_args.push_back(carbon::Variant(static_cast<f32>(o.as_float())));
            } else if (o.is_string()) {
                vm_args.push_back(carbon::Variant(o.to_std_string()));
            } else if (o.is_symbol()) {
                vm_args.push_back(carbon::Variant(carbon::StringId(o.as_symbol().name_ptr)));
            } else {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (sc-run ...) unsupported argument kind: {}\n", o.print());
                return ReplStatus::ERR;
            }
            args_rest = args_rest.as_pair()->cdr;
        }

        auto result = m_runtime->call(name, vm_args);
        if (!result.ok) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                       result.error);
            return ReplStatus::ERR;
        }

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; {} => {}\n", name,
                   result.value.to_string());
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_st_eval(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (st-eval <expr>) requires an expression\n");
            return ReplStatus::ERR;
        }
        try {
            auto result =
                m_soot.eval_form(rest.as_pair()->car, m_soot.get_global_environment().as_env_ptr());
            fmt::print(fg(fmt::color::green), "{}\n", result.print());
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: {}\n", e.what());
            return ReplStatus::ERR;
        }
        return ReplStatus::OK;
    }

    std::optional<ReplStatus> Compiler::try_handle_repl_command(const script::Object &form) {
        if (!form.is_pair()) return std::nullopt;
        const auto &head = form.as_pair()->car;
        if (!head.is_symbol()) return std::nullopt;

        const std::string  name = head.as_symbol().name_ptr;
        const CommandInfo *cmd = find_command(commands(), name, m_config.compiler.mode);
        if (!cmd) return std::nullopt;

        return dispatch_repl_command(*cmd, form);
    }

    ReplStatus Compiler::dispatch_repl_command(const CommandInfo &cmd, const script::Object &form) {
        if (!cmd.handler) {
            fmt::print(stderr, "; ERROR: '{}' is not implemented yet\n", cmd.long_name);
            return ReplStatus::ERR;
        }
        return (this->*cmd.handler)(form);
    }


    // ===============================================================
    // Inspect
    // ===============================================================

    namespace {

        // Helper shared by (sc-env) and (st-env). Prints name -> kind
        // for every used slot of `env`.
        void print_env(const char *title, const script::EnvironmentObject &env) {
            fmt::print("\n=== {} ===\n", title);

            const auto &entries = env.vars.get_all_entries();
            std::size_t used = 0;
            for (const auto &entry : entries) {
                if (!entry.key) continue;
                ++used;

                const char *kind = "value";
                if (entry.value.is_macro()) kind = "macro";
                else if (entry.value.is_function())
                    kind = "function";
                else if (entry.value.is_special_form())
                    kind = "special-form";
                else if (entry.value.is_primitive())
                    kind = "primitive";

                fmt::print("  {:<40} {}\n", entry.key, kind);
            }

            fmt::print("=== End ({} slots, {} used) ===\n\n", entries.size(), used);
        }

    } // namespace

    ReplStatus Compiler::cmd_sc_env(const script::Object &form) {
        (void)form;
        auto env = m_soot.get_soot_environment().as_env_ptr();
        print_env("Compiler environment (*soot-env*)", *env);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_st_env(const script::Object &form) {
        (void)form;
        auto env = m_soot.get_global_environment().as_env_ptr();
        print_env("Interpreter environment (*global-env*)", *env);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_sc_macro_p(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (sc-macro? name) requires a name\n");
            return ReplStatus::ERR;
        }
        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (sc-macro? ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();
        const bool        is_macro = is_soot_macro(name);

        fmt::print(fg(is_macro ? fmt::color::green : fmt::color::yellow),
                   "; sc-macro? \"{}\" => {}\n", name, is_macro ? "true" : "false");
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_symbols(const script::Object &form) {
        auto rest = form.as_pair()->cdr;

        std::string filter;
        bool        has_filter = false;
        if (rest.is_pair()) {
            const auto &filter_obj = rest.as_pair()->car;
            if (filter_obj.is_string() || filter_obj.is_symbol()) {
                filter = filter_obj.to_std_string();
                has_filter = true;
            }
        }

        carbon::Carbon *carbon = carbon::Carbon::current();
        if (!carbon) {
            fmt::print(fg(fmt::color::yellow), "; WARN: no active Carbon context\n");
            return ReplStatus::OK;
        }

        auto symbols = carbon->globals()->all_symbols();
        for (auto &s : symbols) {
            const std::string name = s.to_cstring();
            if (has_filter && name.find(filter) == std::string::npos) continue;
            fmt::print("  {}\n", name);
        }
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_ts_info(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (ts-info <type-name>) requires a type name\n");
            return ReplStatus::ERR;
        }

        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (ts-info ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }

        const std::string type_name = name_obj.to_std_string();

        Type *type = m_ts.lookup_type_no_throw(type_name);
        if (!type) {
            fmt::print(fg(fmt::color::yellow), "; ts-info: unknown type '{}'\n", type_name);
            return ReplStatus::ERR;
        }

        fmt::print("{}\n", type->inspect());
        fmt::print("  is_reference:           {}\n", type->is_reference());
        fmt::print("  load_size:              {}\n", type->get_load_size());
        fmt::print("  preferred_reg_class:    {}\n",
                   reg_kind_to_string(type->get_preferred_reg_class()));

        const auto &states = type->get_states_declared_for_type();
        if (!states.empty()) {
            fmt::print("  states ({}):\n", states.size());
            for (const auto &[k, v] : states) { fmt::print("    {:<24}  {}\n", k, v.print()); }
        }

        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_ts_list(const script::Object &form) {
        auto rest = form.as_pair()->cdr;

        std::string filter;
        bool        has_filter = false;
        if (rest.is_pair()) {
            const auto &filter_obj = rest.as_pair()->car;
            if (filter_obj.is_string() || filter_obj.is_symbol()) {
                filter = filter_obj.to_std_string();
                has_filter = true;
            }
        }

        auto names = m_ts.get_all_type_names();
        std::sort(names.begin(), names.end());

        std::size_t shown = 0;
        for (const auto &name : names) {
            if (has_filter && name.find(filter) == std::string::npos) continue;

            Type       *type = m_ts.lookup_type_no_throw(name);
            const char *kind = type ? type->class_name().c_str() : "?";

            fmt::print("  {:<40} {}\n", name, kind);
            ++shown;
        }

        if (shown == 0) {
            fmt::print(fg(fmt::color::yellow), "; types: no matches\n");
            return ReplStatus::OK;
        }

        fmt::print(fg(fmt::color::cyan), "; types: {} shown\n", shown);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_ast(const script::Object &form) {
        (void)form;

        fmt::print("=== FileNodes ({}) ===\n", m_file_nodes.size());
        for (auto &[name, fn] : m_file_nodes) {
            fmt::print("--- {} ---\n", name.string());
            sootc::inspect_ast(fn.get());
        }

        fmt::print("=== GlobalNode ({}) ===\n", m_global_env->symbols().size());
        inspect_ast(m_global_env.get());

        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_sizes(const script::Object &form) {
        (void)form;
        carbon::print_all_struct_sizes();
        return ReplStatus::OK;
    }

    // ===============================================================
    // Save
    // ===============================================================

    ReplStatus Compiler::cmd_save_session(const script::Object &form) {
        std::string session_name = m_session_name;

        auto rest = form.as_pair()->cdr;
        if (rest.is_pair() && rest.as_pair()->car.is_string()) {
            session_name = rest.as_pair()->car.to_std_string();
        }

        try {
            if (save_session(session_name, ".")) {
                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                           "; saved session as {}.bin and {}.h\n", session_name, session_name);
            } else {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: failed to save session\n");
                return ReplStatus::ERR;
            }
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n", e.what());
            return ReplStatus::ERR;
        }
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_save_module(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module \"path\" [\"out_dir\"]) requires a path\n");
            return ReplStatus::ERR;
        }

        const auto &path_obj = rest.as_pair()->car;
        if (!path_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module ...) path must be a string\n");
            return ReplStatus::ERR;
        }

        std::string path = path_obj.to_std_string();
        std::string out_dir = m_config.compiler.out_dir.string();

        auto after_path = rest.as_pair()->cdr;
        if (after_path.is_pair()) {
            const auto &dir_obj = after_path.as_pair()->car;
            if (!dir_obj.is_string()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (save-module ...) out_dir must be a string\n");
                return ReplStatus::ERR;
            }
            out_dir = dir_obj.to_std_string();
        }

        if (save_module(path, out_dir)) {
            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; OK: module '{}' saved\n",
                       path);
            return ReplStatus::OK;
        }
        return ReplStatus::ERR;
    }

    ReplStatus Compiler::cmd_save_module_deps(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module-deps \"path\" [\"out_dir\"]) requires a path\n");
            return ReplStatus::ERR;
        }

        const auto &path_obj = rest.as_pair()->car;
        if (!path_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module-deps ...) path must be a string\n");
            return ReplStatus::ERR;
        }

        std::string path = path_obj.to_std_string();
        std::string out_dir = m_config.compiler.out_dir.string();

        auto after_path = rest.as_pair()->cdr;
        if (after_path.is_pair()) {
            const auto &dir_obj = after_path.as_pair()->car;
            if (!dir_obj.is_string()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (save-module-deps ...) out_dir must be a string\n");
                return ReplStatus::ERR;
            }
            out_dir = dir_obj.to_std_string();
        }

        if (save_module_recursive(path, out_dir)) {
            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                       "; OK: module '{}' and dependencies saved\n", path);
            return ReplStatus::OK;
        }
        return ReplStatus::ERR;
    }

    // ===============================================================
    // GC
    // ===============================================================

    namespace {

        struct GcListEntry {
            sid64       type;
            void       *payload;
            std::size_t size;
        };

        struct GcListContext {
            std::vector<GcListEntry>               *entries;
            std::vector<sid64>                     *order;
            std::unordered_map<sid64, std::size_t> *counts;
        };

        void gc_list_visitor(sid64 type_sid, void *payload, std::size_t size, void *user) {
            auto *ctx = static_cast<GcListContext *>(user);

            if (ctx->counts->find(type_sid) == ctx->counts->end()) {
                (*ctx->counts)[type_sid] = 0;
                ctx->order->push_back(type_sid);
            }
            (*ctx->counts)[type_sid] += 1;

            ctx->entries->push_back({type_sid, payload, size});
        }

    } // namespace

    ReplStatus Compiler::cmd_gc_collect(const script::Object &form) {
        (void)form;

        const auto before = gc::stats().live_objects;
        gc::collect();
        const auto after = gc::stats().live_objects;

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; gc: {} -> {} live objects\n",
                   before, after);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_gc_list(const script::Object &form) {
        (void)form;

        const auto s = gc::stats();

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "=== GC heap ===\n");
        fmt::print("  live objects:  {}\n", s.live_objects);
        fmt::print("  total allocs:  {}\n", s.total_allocated);
        fmt::print("  total frees:   {}\n", s.total_freed);
        fmt::print("  passes:        {}\n", s.total_passes);
        fmt::print("  committed:     {} bytes\n", s.bytes_committed);

        std::vector<GcListEntry>               entries;
        std::vector<sid64>                     order;
        std::unordered_map<sid64, std::size_t> counts;

        GcListContext ctx{&entries, &order, &counts};
        gc::for_each_live(&gc_list_visitor, &ctx);

        if (entries.empty()) {
            fmt::print(fg(fmt::color::yellow), "  (no live objects)\n");
            return ReplStatus::OK;
        }

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nBy type:\n");

        std::sort(order.begin(), order.end(),
                  [&](sid64 a, sid64 b) { return counts[a] > counts[b]; });

        for (sid64 sid : order) {
            const std::string name = carbon::StringId(sid).to_string();
            fmt::print("  {:<30} {}\n", name, counts[sid]);
        }

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nObjects (first 32):\n");
        constexpr std::size_t kMaxShow = 32;
        for (std::size_t i = 0; i < entries.size() && i < kMaxShow; ++i) {
            const auto       &e = entries[i];
            const std::string name = carbon::StringId(e.type).to_string();
            fmt::print("  [{}] {:<24} size={}  ptr={}\n", i, name, e.size, e.payload);
        }
        if (entries.size() > kMaxShow) {
            fmt::print("  ... and {} more\n", entries.size() - kMaxShow);
        }

        return ReplStatus::OK;
    }

} // namespace sootc