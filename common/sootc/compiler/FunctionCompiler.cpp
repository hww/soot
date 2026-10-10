// FunctionCompiler.cpp
//
// Compiles `lambda`, `function`, and `defmethod` forms into FunctionNode.
//
// The two public entry points are:
//   * compile_function  — build a FunctionNode from a lambda-like form
//   * parse_arguments   — parse the parameter list of a FunctionNode
//
// Parameter syntax accepted by parse_arguments:
//   (name type)   — typed parameter, e.g. (x int)
//   name          — untyped parameter, treated as `object`
//
// Untyped parameters are allowed by default because SOOT is a
// dynamically typed Lisp: lib.soc itself contains lambdas of the form
// (lambda (n) ...) with no type annotation. Set
// ALLOW_SIMPLE_ARGUMENT_SYNTAX to 0 to require explicit types.

#include "sootc/compiler/FunctionCompiler.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/ReturnNode.hpp"
#include <stdexcept>
#include <sootc/node/SequenceNode.hpp>

// Controls whether untyped lambda parameters such as (lambda (n) ...)
// are accepted. Defaults to 1 so that dynamically typed SOOT code
// (including lib.soc) compiles out of the box.
#ifndef ALLOW_SIMPLE_ARGUMENT_SYNTAX
#define ALLOW_SIMPLE_ARGUMENT_SYNTAX 1
#endif

namespace sootc {

    namespace FunctionCompiler {

        // Build a FunctionNode from a (lambda ...), (function ...), or
        // (defmethod ...) form.
        //
        // The grammar accepted here is:
        //   (lambda <keyword>* <param-list> <body>*)
        //
        // Recognised keywords (all optional):
        //   :name        <symbol|string>   — override the function name
        //   :immediate   <any>             — ignored, part of the calling
        //                                    convention in lib.soc
        //   :segment     <any>             — ignored
        //   :no-typecheck <any>            — ignored
        //
        // Any other keyword is consumed and ignored, matching the loose
        // contract used by lib.soc's let/let* macros.
        std::unique_ptr<FunctionNode> compile_function(const soot::Object &form, Node *node,
                                                       NodeBuilder &builder) {

            auto rest = form.as_pair()->cdr;

            // Skip keyword options: :name, :immediate, :segment, :no-typecheck.
            soot::Object current = rest;
            std::string  function_name;

            while (current.is_pair()) {
                auto head = current.as_pair()->car;
                if (!head.is_symbol() || !head.is_keyword()) break;

                std::string kw = head.as_symbol().name_ptr;
                current = current.as_pair()->cdr;
                if (!current.is_pair()) {
                    throw builder.m_compiler->make_error(form, "FunctionCompiler::compile_function")
                        .expected("value after " + kw)
                        .got("end of form");
                }
                auto value = current.as_pair()->car;
                current = current.as_pair()->cdr;

                if (kw == ":name") {
                    if (value.is_symbol() || value.is_string()) {
                        function_name = value.to_std_string();
                    }
                }
                // :immediate, :segment, :no-typecheck — ignored.
            }

            if (!current.is_pair()) {
                throw builder.m_compiler->make_error(form, "FunctionCompiler::compile_function")
                    .expected("parameter list after keyword options")
                    .got("end of form");
            }

            auto args_list = current.as_pair()->car;
            auto body_forms = current.as_pair()->cdr;

            auto fn =
                std::make_unique<FunctionNode>(function_name.empty() ? "lambda" : function_name);

            parse_arguments(args_list, fn.get(), node, builder);

            // Compile every top-level form in the body and keep them all.
            //
            // The previous implementation overwrote `last_expr` on every
            // iteration, so only the LAST top-level form ended up in the
            // function body. Every earlier form was silently discarded.
            // This is invisible when the body is a single form (e.g. a
            // single `let` or a single `while`), but breaks as soon as
            // the user writes a sequence such as
            //
            //     (defun gcd (a b)
            //       (while (!= b 0) ...)
            //       a)
            //
            // where the `while` was dropped and only `a` was compiled.
            //
            // We now build a SequenceNode from all body forms. The
            // SequenceNode's value is the value of its last element, which
            // is exactly the return value of the function. If the body is
            // a single form, the SequenceNode has a single element, and
            // the result is identical to the old behaviour for that case.
            std::vector<std::unique_ptr<ExpressionNode>> body_exprs;
            body_exprs.reserve(4);

            auto current_body = body_forms;
            while (current_body.is_pair()) {
                body_exprs.push_back(
                    builder.build_expression(current_body.as_pair()->car, fn.get()));
                current_body = current_body.as_pair()->cdr;
            }

            if (body_exprs.empty()) {
                // Empty body: an empty function. Nothing to return.
                // The prologue is still emitted by FunctionNode::emit_body.
                return fn;
            }

            // If the body is a single expression, wrap it in a ReturnNode
            // directly. This preserves the exact codegen (and exact byte
            // layout) of every existing test that uses a single-form body.
            if (body_exprs.size() == 1) {
                auto &last_expr = body_exprs[0];
                Type *return_type = last_expr->get_type();
                if (return_type) { fn->set_return_type(return_type); }

                auto ret = std::make_unique<ReturnNode>(std::move(last_expr));
                fn->set_body(std::move(ret));
                return fn;
            }

            // Multi-form body: build a SequenceNode, then wrap in a
            // ReturnNode. The SequenceNode emits every expression in order
            // and forwards the temp register of the last one, so the
            // ReturnNode returns the value of the final form.
            auto seq = std::make_unique<SequenceNode>();
            for (auto &expr : body_exprs) { seq->add(std::move(expr)); }

            Type *return_type = seq->get_type();
            if (return_type) { fn->set_return_type(return_type); }

            auto ret = std::make_unique<ReturnNode>(std::move(seq));
            fn->set_body(std::move(ret));

            return fn;
        }

        // Parse a parameter list into the given FunctionNode.
        //
        // Each element of `args_form` is one of:
        //   (name type)  — typed parameter; the type must resolve to a Type*
        //   name         — untyped parameter; treated as `object`
        //
        // Anything else is a hard error and is reported with the exact
        // form that was rejected, so that macro-expanded code is debuggable.
        void parse_arguments(const soot::Object &args_form, FunctionNode *func_node, Node *node,
                             NodeBuilder &builder) {

            (void)node; // Kept for API symmetry with other FunctionCompiler
                        // helpers. The context node is not needed here
                        // because the FunctionNode is passed explicitly.

            auto current = args_form;

            while (current.is_pair()) {
                auto arg = current.as_pair()->car;

                if (arg.is_pair()) {
                    // Typed parameter form: (name type).
                    auto name_form = arg.as_pair()->car;
                    auto after_name = arg.as_pair()->cdr;

                    if (!name_form.is_symbol()) {
                        throw builder.m_compiler
                            ->make_error(arg, "FunctionCompiler::parse_arguments")
                            .expected("symbol as parameter name")
                            .got(name_form.print());
                    }
                    if (!after_name.is_pair()) {
                        throw builder.m_compiler
                            ->make_error(arg, "FunctionCompiler::parse_arguments")
                            .expected("(name type) pair")
                            .got(arg.print());
                    }

                    auto  type_form = after_name.as_pair()->car;
                    Type *type = builder.parse_type(type_form, func_node);
                    func_node->add_parameter(name_form.as_symbol(), type);
                } else if (arg.is_symbol()) {
                    // Bare symbol: (lambda (x) ...).
                    //
                    // This is the dynamically-typed form used throughout
                    // lib.soc. We accept it when ALLOW_SIMPLE_ARGUMENT_SYNTAX
                    // is enabled (the default) and default the type to
                    // `object`. When the flag is disabled, produce a
                    // concrete, actionable error that names the offending
                    // symbol and shows the fix.
#if ALLOW_SIMPLE_ARGUMENT_SYNTAX
                    Type *type = builder.parse_type(arg, func_node);
                    func_node->add_parameter(arg.as_symbol(), type);
#else
                    // `CompilerError::note` takes a single string, not a
                    // fmt-style format + args. Pre-format the message so the
                    // call has the correct arity.
                    const std::string note =
                        fmt::format("Annotate the parameter, e.g. ({} object), "
                                    "or set ALLOW_SIMPLE_ARGUMENT_SYNTAX to 1.",
                                    arg.print());

                    throw builder.m_compiler->make_error(arg, "FunctionCompiler::parse_arguments")
                        .expected("(name type) pair")
                        .got(fmt::format("bare symbol '{}'", arg.print()))
                        .note(note);
#endif
                } else {
                    // Anything else (integer, string, quoted form, ...) is
                    // not a valid parameter. Report it verbatim so the user
                    // can see what the macro expander produced.
                    throw builder.m_compiler->make_error(arg, "FunctionCompiler::parse_arguments")
                        .expected("parameter as (name type) or bare symbol")
                        .got(arg.print());
                }

                current = current.as_pair()->cdr;
            }
        }

    } // namespace FunctionCompiler
} // namespace sootc