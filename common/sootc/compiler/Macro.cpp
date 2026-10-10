#include "common/sootc/compiler/Macro.hpp"

#include "common/soot/Errors.hpp"
#include "common/soot/Interpreter.hpp"
#include "common/util/Log.hpp"

namespace sootc {

    using soot::Arguments;
    using soot::EnvironmentObject;
    using soot::Object;

    // ============================================================================
    // try_getting_macro_from_soot
    // ============================================================================
    // Look up a macro by name in *soot-env* ONLY.
    //
    // The interpreter (lib.sot) and the compiler (lib.soc) have
    // different ideas about what some names mean. `if`, `when`, `not`,
    // `unless`, `==`, `!=`, `zero?`, `max`, `min` are macros in the
    // interpreter, but built-in compiler forms in NodeBuilder. If the
    // compiler looked those names up in *global-env* (the interpreter's
    // own environment), it would expand them with the interpreter's
    // semantics and generate code that either fails to compile
    // (if without else) or references functions that are not in the
    // VM's Globals table (eq?, =).
    //
    // In OpenGOAL, the equivalent separation is achieved by having two
    // environments: *global-env* for GOOS, *goal-env* for GOAL. Macros
    // defined with `defsmacro` live in *global-env*; macros defined
    // with `defgmacro` live in *goal-env*. The compiler only ever
    // consults *goal-env*, so the interpreter's `if`/`when`/... never
    // shadow the compiler's own forms.
    //
    // We mirror that design here. *soot-env* is the compiler-side
    // environment. lib.soc registers its macros there (see
    // NodeBuilder::build_define). lib.sot registers its macros in
    // *global-env* via m_soot.eval_string(). The two never mix as
    // long as this lookup does NOT walk up the parent chain.
    //
    // The lookup below reads the binding directly out of *soot-env*'s
    // own var table. If the name is not bound there, it is not a
    // compiler macro, regardless of whether it is a macro in
    // *global-env*.
    bool try_getting_macro_from_soot(soot::Interpreter &soot, const soot::Object &macro_name,
                                     soot::Object *dest) {
        if (!macro_name.is_symbol()) { return false; }

        auto sootc_env = soot.get_soot_environment().as_env_ptr();

        // Look up ONLY in soot-env's own bindings, without walking
        // up to *global-env*. The exact API depends on your
        // EnvironmentObject; the one used elsewhere in the codebase
        // is `vars.lookup(symbol)`.
        //
        // If your EnvironmentObject exposes a `lookup_local` /
        // `lookup_here` method, use that instead.
        auto *entry = sootc_env->vars.lookup(macro_name.as_symbol());
        if (!entry) { return false; }
        if (!entry->is_macro()) { return false; }

        if (dest) { *dest = *entry; }
        return true;
    }

    // ============================================================================
    // expand_macro_once
    // ============================================================================
    bool expand_macro_once(soot::Interpreter &soot, const soot::Object &src, soot::Object *out) {
        if (!src.is_pair()) { return false; }

        const Object &first = src.as_pair()->car;
        const Object &rest = src.as_pair()->cdr;

        if (!first.is_symbol()) { return false; }

        Object macro_obj;
        if (!try_getting_macro_from_soot(soot, first, &macro_obj)) { return false; }

        const auto &macro = macro_obj.as_macro();

        Arguments args = soot.get_args(src, rest, macro->args);

        // Create fresh env with parent = global_environment.
        // Mirrors GOAL Macro.cpp:
        //     mac_env->parent_env = m_goos.global_environment.as_env_ptr();
        auto mac_env_obj = EnvironmentObject::make_new();
        auto mac_env = mac_env_obj.as_env_ptr();
        mac_env->parent_env = soot.get_global_environment().as_env_ptr();

        soot.set_args_in_env(src, args, macro->args, mac_env);

        Object result = soot.eval_list_return_last(macro->body, macro->body, mac_env);

        *out = result;
        return true;
    }

    // ============================================================================
    // expand_macro_completely
    // ============================================================================
    soot::Object expand_macro_completely(soot::Interpreter &soot, const soot::Object &src) {
        Object result = src;
        while (expand_macro_once(soot, result, &result)) {
            // repeat
        }
        return result;
    }

    // ============================================================================
    // expand_soot_macro_for_compiler
    // ============================================================================
    soot::Object expand_soot_macro_for_compiler(soot::Interpreter &soot, const soot::Object &form) {
        Object expanded;
        if (!expand_macro_once(soot, form, &expanded)) { return form; }
        return expand_macro_completely(soot, expanded);
    }

} // namespace sootc