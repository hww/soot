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
#include "sootc/node/FileNode.hpp"
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
        std::unique_ptr<FunctionNode> compile_function(const script::Object &form, Node *node,
                                                       NodeBuilder &builder)
        {

            auto rest = form.as_pair()->cdr;

            // Skip keyword options: :name, :immediate, :segment, :no-typecheck.
            script::Object current = rest;
            std::string    function_name;

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

            // ---- 1. Create (owned) ----
            auto fn =
                std::make_unique<FunctionNode>(function_name.empty() ? "lambda" : function_name);

            parse_arguments(args_list, fn.get(), node, builder);

            // ---- 2. Attach to FileNode BEFORE building the body ----
            //
            // The body may reference top-level names declared in the same file
            // (e.g. (-> a add-twice 5) where `a` is a top-level define).
            // build_variable resolves those names through node->file(). If `fn`
            // is not yet attached, its file() is nullptr and the lookup silently
            // fails.
            //
            // We attach `fn` to the owning FileNode, build the body (during
            // which fn->file() works), then detach it so the caller can take
            // ownership back. This keeps the existing "return unique_ptr"
            // contract for build_define / build_begin / build_call.
            FileNode *owner_file = nullptr;
            if (node) {
                owner_file = node->file();
                if (!owner_file) owner_file = dynamic_cast<FileNode *>(node);
            }

            FunctionNode *raw = fn.get();
            if (owner_file) {
                owner_file->add_child(std::move(fn)); // parent = owner_file, ownership -> file
                fn.reset();                           // fn now empty; raw is non-owning
            }

            // ---- 3. Fill (build the body) ----
            //
            // raw->file() is valid here (raw is a child of owner_file, or raw
            // is unattached if owner_file is nullptr — the old behavior).
            std::vector<std::unique_ptr<ExpressionNode>> body_exprs;
            body_exprs.reserve(4);

            auto current_body = body_forms;
            while (current_body.is_pair()) {
                body_exprs.push_back(builder.build_expression(current_body.as_pair()->car, raw));
                current_body = current_body.as_pair()->cdr;
            }

            if (!body_exprs.empty()) {
                if (body_exprs.size() == 1) {
                    auto &last_expr = body_exprs[0];
                    Type *return_type = last_expr->get_type();
                    if (return_type) { raw->set_return_type(return_type); }

                    auto ret = std::make_unique<ReturnNode>(std::move(last_expr));
                    raw->set_body(std::move(ret));
                } else {
                    auto seq = std::make_unique<SequenceNode>();
                    for (auto &expr : body_exprs) { seq->add(std::move(expr)); }

                    Type *return_type = seq->get_type();
                    if (return_type) { raw->set_return_type(return_type); }

                    auto ret = std::make_unique<ReturnNode>(std::move(seq));
                    raw->set_body(std::move(ret));
                }
            }

            // ---- 4. Detach (give ownership back to the caller) ----
            if (owner_file) {
                auto back = owner_file->release_child(raw);
                if (!back) {
                    throw builder.m_compiler->make_error(form, "FunctionCompiler::compile_function")
                        .expected("raw FunctionNode to be a child of owner_file")
                        .got("not found after attach");
                }
                fn = std::unique_ptr<FunctionNode>(static_cast<FunctionNode *>(back.release()));
                fn->set_parent(nullptr);
            } else {
                fn = std::unique_ptr<FunctionNode>(raw);
            }

            return fn; // caller takes ownership; see header note
        }

        // Parse a parameter list into the given FunctionNode.
        //
        // Each element of `args_form` is one of:
        //   (name type)  — typed parameter; the type must resolve to a Type*
        //   name         — untyped parameter; treated as `object`
        //
        // Anything else is a hard error and is reported with the exact
        // form that was rejected, so that macro-expanded code is debuggable.
        void parse_arguments(const script::Object &args_form, FunctionNode *func_node, Node *node,
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