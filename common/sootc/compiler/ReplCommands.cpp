// sootc/compiler/ReplCommands.cpp
//
// REPL command dispatch and handlers. Commands are recognised by
// handle_repl_string() before the compile pipeline runs; see
// Compiler.hpp for the contract.
//
// Handlers that change compiler options go through common::opt_set/
// opt_get/opt_list, which own the dispatch across all scopes and know
// which fields are mutable in the REPL. The const_cast is local to
// those three calls: m_config is a const reference, but the REPL is
// the one place that is allowed to mutate a copy in memory.

#include "sootc/compiler/Compiler.hpp"

#include "carbon/file/Globals.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "script/Interpreter.hpp"
#include "script/Object.hpp"

#include "sootc/node/FileNode.hpp"
#include "sootc/node/NodeInspector.hpp"

#include "type_system/TypeSystem.hpp"

#include "util/Config.hpp"
#include "util/Log.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <file/SizeAssertions.hpp>

namespace sootc {

    // ===============================================================
    // Dispatch
    // ===============================================================

    const std::set<std::string> &Compiler::repl_commands() {
        static const std::set<std::string> kCommands = {"help",
                                                        "clear",
                                                        "e",
                                                        "reload",
                                                        "sizes",
                                                        "inspect-env",
                                                        "inspect-env-soots",
                                                        "has-macro",
                                                        "list-symbols",
                                                        "save-session",
                                                        "save-module",
                                                        "save-module-recursive",
                                                        "sc-run",
                                                        "sc-eval",
                                                        "arg-set",
                                                        "arg-get",
                                                        "arg-ls",
                                                        "gcls",
                                                        "gc",
                                                        "ts-info",
                                                        "ast"};
        return kCommands;
    }

    std::optional<ReplStatus> Compiler::try_handle_repl_command(const script::Object &form) {
        if (!form.is_pair()) return std::nullopt;
        const auto &head = form.as_pair()->car;
        if (!head.is_symbol()) return std::nullopt;
        const std::string name = head.as_symbol().name_ptr;
        if (!repl_commands().count(name)) return std::nullopt;
        return dispatch_repl_command(name, form);
    }

    ReplStatus Compiler::dispatch_repl_command(const std::string    &name,
                                               const script::Object &form) {
        if (name == "help") return cmd_help(form);
        if (name == "clear") return cmd_clear(form);
        if (name == "e") return cmd_e(form);
        if (name == "reload") return cmd_reload(form);
        if (name == "sizes") return cmd_sizes(form);
        if (name == "inspect-env") return cmd_inspect_env(form);
        if (name == "inspect-env-soots") return cmd_inspect_env_soots(form);
        if (name == "has-macro") return cmd_has_macro(form);
        if (name == "list-symbols") return cmd_list_symbols(form);
        if (name == "save-session") return cmd_save_session(form);
        if (name == "save-module") return cmd_save_module(form);
        if (name == "save-module-recursive") return cmd_save_module_recursive(form);
        if (name == "sc-run") return cmd_sc_run(form);
        if (name == "sc-eval") return cmd_sc_eval(form);
        if (name == "arg-set") return cmd_arg_set(form);
        if (name == "arg-get") return cmd_arg_get(form);
        if (name == "arg-ls") return cmd_arg_list(form);
        if (name == "gcls") return cmd_gc_list(form);
        if (name == "gc") return cmd_gc_collect(form);
        if (name == "ts-info") return cmd_ts_info(form);
        if (name == "ast") return cmd_ast(form);

        fmt::print(fg(fmt::color::crimson), "; ERROR: unknown REPL command '{}'\n", name);
        return ReplStatus::ERR;
    }

    // ===============================================================
    // Simple commands
    // ===============================================================

    ReplStatus Compiler::cmd_help(const script::Object &form) {
        (void)form;
        if (m_repl) m_repl->print_help_message();
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_clear(const script::Object &form) {
        (void)form;
        if (m_repl) m_repl->clear_screen();
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_e(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_EXIT;
    }

    ReplStatus Compiler::cmd_reload(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_RELOAD;
    }

    ReplStatus Compiler::cmd_sizes(const script::Object &form) {
        (void)form;
        carbon::print_all_struct_sizes();
        return ReplStatus::OK;
    }

    // ===============================================================
    // Environment inspection
    // ===============================================================

    // Dump the compiler-side environment (*soot-env*).
    //
    // *soot-env* is the environment where lib.sc registers its macros.
    // try_getting_macro_from_soot consults only this environment, so
    // whatever appears here is what the compiler sees as macros.
    ReplStatus Compiler::cmd_inspect_env(const script::Object &form) {
        (void)form;
        auto sootc_env = m_soot.get_soot_environment().as_env_ptr();

        fmt::print("\n=== Compiler environment (*soot-env*) ===\n");

        const auto &entries = sootc_env->vars.get_all_entries();
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
        return ReplStatus::OK;
    }

    // Dump the interpreter-side environment (*global-env*).
    //
    // *global-env* is where lib.st and the interpreter register their
    // macros and built-in functions. The compiler does NOT consult this
    // environment for macro dispatch.
    ReplStatus Compiler::cmd_inspect_env_soots(const script::Object &form) {
        (void)form;
        auto global_env = m_soot.get_global_environment().as_env_ptr();

        fmt::print("\n=== Interpreter environment (*global-env*) ===\n");

        const auto &entries = global_env->vars.get_all_entries();
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
        return ReplStatus::OK;
    }

    // Check whether `name` is a macro visible to the compiler, i.e. a
    // macro registered in *soot-env*.
    ReplStatus Compiler::cmd_has_macro(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (has-macro \"name\") requires a name\n");
            return ReplStatus::ERR;
        }
        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (has-macro ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();
        const bool        is_macro = is_soot_macro(name);

        fmt::print(fg(is_macro ? fmt::color::green : fmt::color::yellow),
                   "; has-macro \"{}\" => {}\n", name, is_macro ? "true" : "false");
        return ReplStatus::OK;
    }

    // ===============================================================
    // Symbol listing
    // ===============================================================

    ReplStatus Compiler::cmd_list_symbols(const script::Object &form) {
        (void)form;

        carbon::Carbon *carbon = carbon::Carbon::current();
        if (!carbon) {
            fmt::print(fg(fmt::color::yellow), "; WARN: no active Carbon context\n");
            return ReplStatus::OK;
        }

        auto symbols = carbon->globals()->all_symbols();
        for (auto &s : symbols) { fmt::print("  {}\n", s.to_cstring()); }
        return ReplStatus::OK;
    }

    // ===============================================================
    // Session / module saving
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

    // (save-module "path/to/foo.sc" ["out_dir"])
    //
    // Compile a .sc file as its own module and save it. This is the
    // module-level counterpart to save-session: it writes what the
    // file contains, not what the REPL has accumulated.
    ReplStatus Compiler::cmd_save_module(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module \"path/to/file.sc\" [\"out_dir\"]) "
                       "requires a path\n");
            return ReplStatus::ERR;
        }

        const auto &path_obj = rest.as_pair()->car;
        if (!path_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module ...) path must be a string\n");
            return ReplStatus::ERR;
        }

        std::string path = path_obj.to_std_string();
        std::string out_dir = "build";

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

    ReplStatus Compiler::cmd_save_module_recursive(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module-recursive \"path\" [\"out_dir\"]) "
                       "requires a path\n");
            return ReplStatus::ERR;
        }

        const auto &path_obj = rest.as_pair()->car;
        if (!path_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module-recursive ...) path must be a string\n");
            return ReplStatus::ERR;
        }

        std::string path = path_obj.to_std_string();
        std::string out_dir = "build";

        auto after_path = rest.as_pair()->cdr;
        if (after_path.is_pair()) {
            const auto &dir_obj = after_path.as_pair()->car;
            if (!dir_obj.is_string()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (save-module-recursive ...) out_dir must be a string\n");
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
    // Execution
    // ===============================================================

    // (sc-run name arg...)
    //
    // Execute a compiled function in the active runtime.
    //
    // Argument types are inferred from the SOOT object kind:
    //   integer   -> i64
    //   float     -> f32
    //   string    -> std::string
    //   symbol    -> StringId
    //
    // Example: (sc-run gcd 48 18)
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

    // (sc-eval expr)
    //
    // Evaluate a SOOT expression in the interpreter's global environment
    // and print the result.
    ReplStatus Compiler::cmd_sc_eval(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (sc-eval <expr>) requires an expression\n");
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

    // ===============================================================
    // Compiler options
    // ===============================================================
    //
    // (arg-set "key" value), (arg-get "key"), (arg-ls [filter])
    //
    // These delegate to common::opt_set / opt_get / opt_list, which
    // own the dispatch across all scopes and know which fields are
    // mutable in the REPL. The const_cast is local to these three
    // calls: m_config is a const reference, but the REPL is the one
    // place that is allowed to mutate the in-memory snapshot.

    ReplStatus Compiler::cmd_arg_set(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set \"key\" value) requires a key and a value\n");
            return ReplStatus::ERR;
        }
        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }
        auto after_key = rest.as_pair()->cdr;
        if (!after_key.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set \"{}\" value) requires a value\n",
                       key_obj.to_std_string());
            return ReplStatus::ERR;
        }
        const std::string key = key_obj.to_std_string();
        const auto       &value = after_key.as_pair()->car;

        if (!common::opt_set(const_cast<common::Config &>(m_config), key, value)) {
            return ReplStatus::ERR;
        }
        Type::log_level = m_config.common.log_level;
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_arg_get(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (arg-get \"key\") requires a key\n");
            return ReplStatus::ERR;
        }
        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-get ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }
        if (!common::opt_get(const_cast<common::Config &>(m_config), key_obj.to_std_string())) {
            return ReplStatus::ERR;
        }
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_arg_list(const script::Object &form) {
        (void)form;
        common::opt_list(const_cast<common::Config &>(m_config));
        return ReplStatus::OK;
    }

    // ===============================================================
    // Garbage collector
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

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                   "; gc-collect: {} -> {} live objects\n", before, after);
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

    // ===============================================================
    // Type system
    // ===============================================================

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

    // ===============================================================
    // AST
    // ===============================================================

    ReplStatus Compiler::cmd_ast(const script::Object &form) {
        (void)form;

        fmt::print("=== FileNodes ({}) ===\n", m_file_nodes.size());
        for (auto &[name, fn] : m_file_nodes) {
            fmt::print("--- {} ---\n", name.string());
            inspect_ast(fn.get());
        }

        fmt::print("=== GlobalNode ({}) ===\n", m_global_env->symbols().size());
        inspect_ast(m_global_env.get());

        return ReplStatus::OK;
    }

} // namespace sootc