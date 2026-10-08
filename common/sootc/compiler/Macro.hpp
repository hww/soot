#pragma once

#include "common/soot/Object.hpp"

namespace soot {

    class Interpreter;

} // namespace soot

namespace sootc {

    /// @brief Look up a macro by name in the interpreter's "sootc-env".
    ///
    /// Mirrors GOAL's Compiler::try_getting_macro_from_goos:
    ///   - search happens in the compiler-side environment (goal_env / sootc-env),
    ///   - the macro object is returned via `dest` if found.
    ///
    /// @param soot        The interpreter instance.
    /// @param macro_name  Symbol naming the macro.
    /// @param dest        Optional out-parameter receiving the macro object.
    /// @return true if a macro with that name exists in sootc-env.
    bool try_getting_macro_from_soot(soot::Interpreter &soot, const soot::Object &macro_name,
                                     soot::Object *dest = nullptr);

    /// @brief Expand a macro invocation exactly once.
    ///
    /// Mirrors GOAL's Compiler::expand_macro_once.
    ///
    /// Steps:
    ///   1. Ensure `src` is a proper list whose head is a symbol.
    ///   2. Look the symbol up in sootc-env (see try_getting_macro_from_soot).
    ///   3. Bind macro arguments in a fresh env whose parent is
    ///      global_environment (so GOOS helpers like car/cdr/length are visible).
    ///   4. Evaluate the macro body once.
    ///
    /// @param soot   The interpreter instance.
    /// @param src    Candidate macro form, e.g. (defun foo () 1).
    /// @param out    Receives the expanded form on success.
    /// @return true if `src` was a macro invocation and `*out` was filled.
    bool expand_macro_once(soot::Interpreter &soot, const soot::Object &src, soot::Object *out);

    /// @brief Expand macros repeatedly until the result is no longer a macro call.
    ///
    /// Mirrors GOAL's Compiler::expand_macro_completely.
    soot::Object expand_macro_completely(soot::Interpreter &soot, const soot::Object &src);

    /// @brief Expand a macro form on behalf of NodeBuilder.
    ///
    /// If `form` is not a macro invocation, it is returned unchanged.
    /// Otherwise the fully expanded form is returned.
    ///
    /// This is the single entry point used by the compiler backend.
    soot::Object expand_soot_macro_for_compiler(soot::Interpreter &soot, const soot::Object &form);

} // namespace sootc