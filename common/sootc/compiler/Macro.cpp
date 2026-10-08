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
    bool try_getting_macro_from_soot(soot::Interpreter &soot, const soot::Object &macro_name,
                                     soot::Object *dest) {
        if (!macro_name.is_symbol()) { return false; }

        auto sootc_env = soot.get_soot_environment().as_env_ptr();

        Object macro_obj;
        try {
            macro_obj = soot.eval_symbol(macro_name, sootc_env);
        } catch (const soot::EvalException &) {
            // Symbol not defined in sootc-env.
            return false;
        }

        if (!macro_obj.is_macro()) { return false; }

        if (dest) { *dest = macro_obj; }
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