#include "common/sootc/compiler/NodeBuilder.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/sootc/compiler/CompilerError.hpp"
#include "common/sootc/compiler/FunctionCompiler.hpp"
#include "common/sootc/node/DataDeclarationNode.hpp"
#include "common/sootc/node/EnumDeclarationNode.hpp"
#include "common/sootc/node/FileNode.hpp"
#include "common/sootc/node/FunctionNode.hpp"
#include "common/sootc/node/SequenceNode.hpp"
#include "common/sootc/node/StoreGlobalNode.hpp"
#include "common/sootc/node/TypeDeclarationNode.hpp"
#include "common/sootc/node/CastNode.hpp"
#include "common/type_system/Defenum.hpp"
#include "common/type_system/Deftype.hpp"
#include "fmt/format.h"

#include <sootc/node/MethodCallNode.hpp>
#include <stdexcept>

namespace sootc {

    // ============================================================================
    // Constructor / form table
    // ============================================================================
    NodeBuilder::NodeBuilder(TypeSystem &ts, Compiler *compiler) : m_ts(ts), m_compiler(compiler) {
        init_form_table();
    }

    void NodeBuilder::init_form_table() {
        // Basic / declarations
        m_form_table["define"] = &NodeBuilder::build_define_wrap;
        m_form_table["define-export"] = &NodeBuilder::build_define_wrap;
        m_form_table["lambda"] = &NodeBuilder::build_lambda_wrap;
        m_form_table["function"] = &NodeBuilder::build_lambda_wrap;
        m_form_table["begin"] = &NodeBuilder::build_begin_wrap;

        // Control flow
        m_form_table["if"] = &NodeBuilder::build_if_wrap;
        m_form_table["cond"] = &NodeBuilder::build_cond_wrap;
        m_form_table["while"] = &NodeBuilder::build_while_wrap;
        m_form_table["let"] = &NodeBuilder::build_let_wrap;
        m_form_table["set!"] = &NodeBuilder::build_set_wrap;
        m_form_table["new"] = &NodeBuilder::build_new_wrap;
        m_form_table["->"] = &NodeBuilder::build_deref_wrap;

        // Arithmetic
        m_form_table["+"] = &NodeBuilder::build_binary_wrap;
        m_form_table["-"] = &NodeBuilder::build_binary_wrap;
        m_form_table["*"] = &NodeBuilder::build_binary_wrap;
        m_form_table["/"] = &NodeBuilder::build_binary_wrap;
        m_form_table["%"] = &NodeBuilder::build_binary_wrap;

        m_form_table[">"] = &NodeBuilder::build_compare_wrap;
        m_form_table["<"] = &NodeBuilder::build_compare_wrap;
        m_form_table[">="] = &NodeBuilder::build_compare_wrap;
        m_form_table["<="] = &NodeBuilder::build_compare_wrap;
        m_form_table["=="] = &NodeBuilder::build_compare_wrap;
        m_form_table["!="] = &NodeBuilder::build_compare_wrap;

        // Unary
        m_form_table["abs"] = &NodeBuilder::build_abs_wrap;
        m_form_table["neg"] = &NodeBuilder::build_neg_wrap;
        m_form_table["not"] = &NodeBuilder::build_not_wrap;
        m_form_table["lognot"] = &NodeBuilder::build_lognot_wrap;

        // Declarations (compiler-only)
        m_form_table["defmacro"] = &NodeBuilder::build_defmacro_wrap;
        m_form_table["deftype"] = &NodeBuilder::build_deftype_wrap;
        m_form_table["defenum"] = &NodeBuilder::build_defenum_wrap;
        m_form_table["defmethod"] = &NodeBuilder::build_defmethod_wrap;
        m_form_table["seval"] = &NodeBuilder::build_seval_wrap;
        m_form_table["update-macro-metadata"] = &NodeBuilder::build_update_macro_metadata;
        m_form_table["defconstant"] = &NodeBuilder::build_defconstant;
        m_form_table["define-extern"] = &NodeBuilder::build_define_extern;
        m_form_table["define-native"] = &NodeBuilder::build_define_native;
    }

    // ============================================================================
    // build — single dispatch
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build(const soot::Object &form, Node *node) {
        if (form.is_symbol()) {
            // ---- Soot literals: #t / #f ----
            //
            // The reader parses `#t` and `#f` as ordinary symbols, but
            // semantically they are boolean constants. GOAL's VM treats any
            // non-zero int as true, so we map them to 1 and 0.
            const std::string name = form.as_symbol();
            if (name == "#t") return ConstNode::make_int(1);
            if (name == "#f") return ConstNode::make_int(0);

            // ---- Compile-time constants ----
            if (m_compiler) {
                if (auto constant = m_compiler->lookup_constant(name)) {
                    return build(*constant, node);
                }
            }

            return build_variable(form, node);
        }

        if (!form.is_pair()) return build_const(form, node);

        auto head = form.as_pair()->car;
        auto rest = form.as_pair()->cdr;

        if (!head.is_symbol()) return build_call(form, node);

        std::string keyword = head.as_symbol();

        // 1. SOOT macros — expand first.
        if (m_compiler && m_compiler->is_soot_macro(keyword)) {
            auto expanded = m_compiler->expand_soot_macro(form);
            return build(expanded, node);
        }

        // 2. Built-in forms — single lookup.
        auto it = m_form_table.find(keyword);
        if (it != m_form_table.end()) { return (this->*(it->second))(form, node); }

        // 3. Regular function call.
        return build_call(form, node);
    }

// ============================================================================
// Wrap forwarders — uniform signature for the dispatch table.
// ============================================================================
#define WRAP_FORWARD(wrap_name, target)                                                            \
    std::unique_ptr<Node> NodeBuilder::wrap_name(const soot::Object &form, Node *node) {           \
        return target(form, node);                                                                 \
    }

    WRAP_FORWARD(build_begin_wrap, build_begin)
    WRAP_FORWARD(build_define_wrap, build_define)
    WRAP_FORWARD(build_defmacro_wrap, build_defmacro)
    WRAP_FORWARD(build_seval_wrap, build_seval)
    WRAP_FORWARD(build_deftype_wrap, build_deftype)
    WRAP_FORWARD(build_defenum_wrap, build_defenum)
    WRAP_FORWARD(build_defmethod_wrap, build_defmethod)
    WRAP_FORWARD(build_lambda_wrap, build_lambda)

    WRAP_FORWARD(build_if_wrap, build_if)
    WRAP_FORWARD(build_cond_wrap, build_cond)
    WRAP_FORWARD(build_while_wrap, build_while)
    WRAP_FORWARD(build_let_wrap, build_let)
    WRAP_FORWARD(build_set_wrap, build_set)
    WRAP_FORWARD(build_deref_wrap, build_deref)
    WRAP_FORWARD(build_new_wrap, build_new)
    WRAP_FORWARD(build_binary_wrap, build_binary)
    WRAP_FORWARD(build_compare_wrap, build_compare)
    
    WRAP_FORWARD(build_abs_wrap, build_abs)
    WRAP_FORWARD(build_neg_wrap, build_neg)
    WRAP_FORWARD(build_not_wrap, build_not)
    WRAP_FORWARD(build_lognot_wrap, build_lognot)

#undef WRAP_FORWARD

    // ============================================================================
    // begin
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_begin(const soot::Object &form, Node *node) {
        auto body_forms = form.as_pair()->cdr;
        auto seq = std::make_unique<SequenceNode>();
        auto cur = body_forms;
        while (cur.is_pair()) {
            auto &child = cur.as_pair()->car;

            // Top-level declarations inside begin are hoisted onto the FileNode.
            if (child.is_pair() && child.as_pair()->car.is_symbol()) {
                const std::string kw = child.as_pair()->car.as_symbol();

                if (kw == "deftype" || kw == "defenum") {
                    auto decl = build(child, node);
                    if (auto *file = node->file()) file->add_child(std::move(decl));
                    cur = cur.as_pair()->cdr;
                    continue;
                }
                if (kw == "defmethod") {
                    auto fn = build_defmethod(child, node);
                    if (auto *file = node->file()) {
                        FunctionNode *raw = dynamic_cast<FunctionNode *>(fn.get());
                        if (!raw) throw std::runtime_error("defmethod didn't return FunctionNode");
                        file->add_child(std::move(fn));
                        file->bind(raw->name(), raw);
                    }
                    cur = cur.as_pair()->cdr;
                    continue;
                }
                if (kw == "defun") {
                    auto fn = build(child, node);
                    if (auto *file = node->file()) {
                        auto *raw = dynamic_cast<FunctionNode *>(fn.get());
                        if (!raw) throw std::runtime_error("defun didn't return FunctionNode");
                        file->add_child(std::move(fn));
                        file->bind(raw->name(), raw);
                    }
                    cur = cur.as_pair()->cdr;
                    continue;
                }
                if (kw == "define" || kw == "define-export") {
                    auto decl = build_define(child, node);
                    if (auto *file = node->file()) {
                        if (auto *fn = dynamic_cast<FunctionNode *>(decl.get())) {
                            FunctionNode *raw = fn;
                            file->add_child(std::move(decl));
                            file->bind(raw->name(), raw);
                        } else {
                            file->add_child(std::move(decl));
                        }
                    }
                    cur = cur.as_pair()->cdr;
                    continue;
                }
            }

            seq->add(build_expression(child, node));
            cur = cur.as_pair()->cdr;
        }
        return seq;
    }

    // ============================================================================
    // lambda
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_lambda(const soot::Object &form, Node *node) {
        return FunctionCompiler::compile_function(form, node, *this);
    }

    // ============================================================================
    // update-macro-metadata
    // ============================================================================

    std::unique_ptr<Node> NodeBuilder::build_update_macro_metadata(const soot::Object &form,
                                                                   Node               *node) {
        (void)form;
        (void)node;
        // No-op: metadata for tooling, not runtime code.
        return std::make_unique<SequenceNode>();
    }

    // ============================================================================
    // defconstant
    // ============================================================================

    std::unique_ptr<Node> NodeBuilder::build_defconstant(const soot::Object &form, Node *node) {
        (void)node;

        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defconstant")
                .expected("(defconstant name value)")
                .got("empty form");
        }

        auto name_form = rest.as_pair()->car;
        if (!name_form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defconstant")
                .expected("symbol as constant name")
                .got(name_form.print());
        }

        auto after_name = rest.as_pair()->cdr;
        if (!after_name.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defconstant")
                .expected("value after name")
                .got("end of form");
        }

        auto value_form = after_name.as_pair()->car;

        // Parse the value (must be a literal — number, string, symbol).
        soot::Object value = value_form;

        // Register the constant in the compiler's constant pool.
        //
        // Mirrors GOAL's compile_defconstant:
        //   m_global_constants.set(sym, value);
        m_compiler->define_constant(name_form.to_std_string(), value);
        lg::info("Registered constant: {} = {}", name_form.to_std_string(), value.print());

        return std::make_unique<SequenceNode>();
    }

    // ============================================================================
    // if
    // ============================================================================
    std::unique_ptr<IfNode> NodeBuilder::build_if(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;
        auto cond_form = rest.as_pair()->car;
        auto then_form = rest.as_pair()->cdr.as_pair()->car;
        auto else_form = rest.as_pair()->cdr.as_pair()->cdr.as_pair()->car;

        auto cond = build_expression(cond_form, node);
        auto then_branch = build_expression(then_form, node);
        auto else_branch = else_form.is_null() ? nullptr : build_expression(else_form, node);

        return std::make_unique<IfNode>(std::move(cond), std::move(then_branch),
                                        std::move(else_branch));
    }

    // ============================================================================
    // cond
    // ============================================================================
    std::unique_ptr<ExpressionNode> NodeBuilder::build_cond(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_cond")
                .expected("at least one cond clause")
                .got("empty cond");
        }

        struct Clause {
            soot::Object test;
            soot::Object body;
        };
        std::vector<Clause> clauses;

        auto current = rest;
        while (current.is_pair()) {
            auto clause = current.as_pair()->car;
            if (!clause.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_cond")
                    .expected("each cond clause to be (test body...)")
                    .got(clause.print());
            }
            clauses.push_back({clause.as_pair()->car, clause.as_pair()->cdr});
            current = current.as_pair()->cdr;
        }

        std::unique_ptr<ExpressionNode> result;

        for (auto it = clauses.rbegin(); it != clauses.rend(); ++it) {
            bool is_else = it->test.is_symbol() && it->test.as_symbol() == "else";
            auto body_expr = build_body_as_sequence(it->body, node);

            if (is_else) {
                if (result) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_cond")
                        .expected("'else' clause to be the last one")
                        .got("'else' before other clauses");
                }
                result = std::unique_ptr<ExpressionNode>(
                    dynamic_cast<ExpressionNode *>(body_expr.release()));
            } else {
                auto cond_expr = build_expression(it->test, node);
                result = std::make_unique<IfNode>(
                    std::move(cond_expr),
                    std::unique_ptr<ExpressionNode>(
                        dynamic_cast<ExpressionNode *>(body_expr.release())),
                    std::move(result));
            }
        }

        return result;
    }

    // ============================================================================
    // let
    // ============================================================================
    std::unique_ptr<LetNode> NodeBuilder::build_let(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_let")
                .expected("(let ((name value) ...) body...)")
                .got("missing bindings list");
        }

        auto bindings_form = rest.as_pair()->car;
        auto body_forms = rest.as_pair()->cdr;

        if (!bindings_form.is_null() && !bindings_form.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_let")
                .expected("bindings must be a list or null")
                .got(bindings_form.print());
        }

        auto let_node = std::make_unique<LetNode>();

        auto cur = bindings_form;
        while (cur.is_pair()) {
            auto binding = cur.as_pair()->car;
            if (!binding.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_let")
                    .expected("(name value) binding")
                    .got(binding.print());
            }

            auto name_obj = binding.as_pair()->car;
            auto value_obj = binding.as_pair()->cdr;

            if (!name_obj.is_symbol()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_let")
                    .expected("symbol as binding name")
                    .got(name_obj.print());
            }
            if (!value_obj.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_let")
                    .expected("value after binding name")
                    .got("end of form");
            }

            std::string name = name_obj.as_symbol();
            auto        value = build_expression(value_obj.as_pair()->car, node);

            let_node->add_binding(name, std::move(value));
            cur = cur.as_pair()->cdr;
        }

        auto body = build_body_as_sequence(body_forms, node);
        if (!body) {
            throw m_compiler->make_error(form, "NodeBuilder::build_let")
                .expected("non-empty body")
                .got("empty body");
        }
        let_node->set_body(
            std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode *>(body.release())));

        return let_node;
    }

    // ============================================================================
    // set!
    // ============================================================================
    std::unique_ptr<SetNode> NodeBuilder::build_set(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_set")
                .expected("(set! target value)")
                .got("empty form");
        }

        auto target_form = rest.as_pair()->car;

        if (!rest.as_pair()->cdr.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_set")
                .expected("value after target")
                .got("end of form");
        }
        auto value_form = rest.as_pair()->cdr.as_pair()->car;

        auto value = build_expression(value_form, node);

        if (target_form.is_symbol()) {
            std::string name = target_form.as_symbol();
            return std::make_unique<SetNode>(name, std::move(value));
        }

        if (target_form.is_pair() && target_form.as_pair()->car.is_symbol() &&
            target_form.as_pair()->car.as_symbol() == "->") {
            auto deref_expr = build_deref(target_form, node);

            auto *deref_raw = dynamic_cast<DerefNode *>(deref_expr.release());
            if (!deref_raw) {
                throw m_compiler->make_error(form, "NodeBuilder::build_set")
                    .expected("DerefNode as lvalue")
                    .got("non-DerefNode");
            }

            return std::make_unique<SetNode>(std::unique_ptr<DerefNode>(deref_raw),
                                             std::move(value));
        }

        throw m_compiler->make_error(form, "NodeBuilder::build_set")
            .expected("variable name or (-> obj field) as target")
            .got(target_form.print());
    }

    // ============================================================================
    // ->
    // ============================================================================
    std::unique_ptr<ExpressionNode> NodeBuilder::build_deref(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_deref")
                .expected("(-> expr field/method [args...])")
                .got("empty form");
        }

        auto expr_form = rest.as_pair()->car;
        auto field_form = rest.as_pair()->cdr.as_pair()->car;

        if (!field_form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_deref")
                .expected("field or method name as a symbol")
                .got(field_form.print());
        }

        std::string name = field_form.as_symbol();

        auto expr = build_expression(expr_form, node);

        Type *expr_type = expr->get_type();
        if (!expr_type) {
            throw m_compiler->make_error(form, "NodeBuilder::build_deref")
                .expected("base expression with a known type")
                .got("unknown");
        }

        Type *struct_type = expr_type;
        {
            DerefInfo di = m_ts.get_deref_info(TypeSpec(expr_type->get_name()));
            if (di.can_deref && di.result_type.get()) struct_type = di.result_type.get();
        }

        auto *st = dynamic_cast<StructureType *>(struct_type);
        if (!st) {
            throw m_compiler->make_error(form, "NodeBuilder::build_deref")
                .expected("structure type")
                .got(struct_type ? struct_type->get_name() : "unknown");
        }

        // Field?
        Field field;
        if (st->lookup_field(name, &field)) {
            Type *field_type = m_ts.lookup_type_no_throw(field.type().base_type());
            if (!field_type) {
                throw m_compiler->make_error(form, "NodeBuilder::build_deref")
                    .expected("known field type")
                    .got(field.type().print());
            }
            return std::make_unique<DerefNode>(std::move(expr), name,
                                               static_cast<u32>(field.offset()), field_type);
        }

        // Method?
        MethodInfo method_info;
        if (m_ts.try_lookup_method(struct_type->get_name(), name, &method_info)) {
            std::vector<std::unique_ptr<ExpressionNode>> args;
            auto arg_forms = rest.as_pair()->cdr.as_pair()->cdr;
            while (arg_forms.is_pair()) {
                args.push_back(build_expression(arg_forms.as_pair()->car, node));
                arg_forms = arg_forms.as_pair()->cdr;
            }

            Type *return_type = nullptr;
            if (!method_info.type.empty()) {
                return_type = m_ts.lookup_type_no_throw(method_info.type.last_arg().base_type());
            }

            const std::string full_name = fmt::format("{}-{}", struct_type->get_name(), name);

            auto method_call = std::make_unique<MethodCallNode>(std::move(expr), full_name,
                                                                std::move(args), return_type);
            method_call->set_is_native(m_compiler->is_native_function(full_name));
            return method_call;
        }

        throw m_compiler->make_error(form, "NodeBuilder::build_deref")
            .expected("a known field or method")
            .got(fmt::format("(-> {} {})", struct_type->get_name(), name));
    }

    // ============================================================================
    // new
    // ============================================================================
    std::unique_ptr<NewNode> NodeBuilder::build_new(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_new")
                .expected("(new [alloc] Type ...)")
                .got("empty form");
        }

        std::string allocation = "static";
        std::string type_name;

        const auto &first = rest.as_pair()->car;

        // Unwrap (quote x) -> x. In Lisp, 'x is (quote x), and the reader
        // does not collapse it, so we must handle it here.
        const soot::Object *first_ptr = &first;
        if (first.is_pair() && first.as_pair()->car.is_symbol() &&
            first.as_pair()->car.as_symbol() == "quote") {
            const auto &quoted = first.as_pair()->cdr;
            if (!quoted.is_pair() || !quoted.as_pair()->car.is_symbol()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_new")
                    .expected("symbol after quote")
                    .got(first.print());
            }
            first_ptr = &quoted.as_pair()->car;
        }

        if (!first_ptr->is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_new")
                .expected("symbol as type name (or allocation)")
                .got(first_ptr->print());
        }

        std::string first_str = first_ptr->as_symbol();

        if (first_str == "static" || first_str == "global" || first_str == "heap" ||
            first_str == "stack" || first_str == "stack-no-clear" || first_str == "process" ||
            first_str == "debug") 
            {
            allocation = first_str;
            rest = rest.as_pair()->cdr;

            if (!rest.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_new")
                    .expected("type name after allocation")
                    .got("end of form");
            }

            const auto         &type_obj = rest.as_pair()->car;
            const soot::Object *type_ptr = &type_obj;
            if (type_obj.is_pair() && type_obj.as_pair()->car.is_symbol() &&
                type_obj.as_pair()->car.as_symbol() == "quote") {
                const auto &quoted = type_obj.as_pair()->cdr;
                if (!quoted.is_pair() || !quoted.as_pair()->car.is_symbol()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_new")
                        .expected("symbol after quote")
                        .got(type_obj.print());
                }
                type_ptr = &quoted.as_pair()->car;
            }
            if (!type_ptr->is_symbol()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_new")
                    .expected("symbol as type name")
                    .got(type_ptr->print());
            }
            type_name = type_ptr->to_std_string();
        } else {
            type_name = first_str;
        }

        Type *type = m_ts.lookup_type_no_throw(type_name);
        if (!type) {
            throw m_compiler->make_error(form, "NodeBuilder::build_new")
                .expected("a known type")
                .got(fmt::format("new {} {}", allocation, type_name));
        }

        auto new_node = std::make_unique<NewNode>(TypeSpec(type_name));
        new_node->set_allocation(allocation);

        // Set the expression type to the constructed type. This lets `->`
        // (build_deref) resolve fields and methods on the result of `(new ...)`.
        {
            Type *constructed_type = m_ts.lookup_type_no_throw(type_name);
            if (constructed_type) { new_node->set_type(constructed_type); }
        }

        auto fields = rest.as_pair()->cdr;

        // Two syntaxes:
        //
        //   (new 'static 'vec3 :x 1 :y 2)     — :field value pairs
        //   (new 'process 'vec3 1 2 3 4)      — positional args
        //
        // For 'static we always use :field value. For non-static we use
        // positional args because they map 1:1 to the constructor's
        // parameters.
        if (allocation == "static") {
            while (fields.is_pair()) {
                const auto &field_name_obj = fields.as_pair()->car;

                if (!field_name_obj.is_keyword()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_new")
                        .expected(":field-name as a keyword")
                        .got(field_name_obj.print());
                }

                std::string field_name = field_name_obj.as_symbol().name_ptr;
                if (!field_name.empty() && field_name[0] == ':') {
                    field_name = field_name.substr(1);
                }

                fields = fields.as_pair()->cdr;
                if (!fields.is_pair()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_new")
                        .expected("value after field name")
                        .got("end of form");
                }

                auto value_node = build_expression(fields.as_pair()->car, node);
                new_node->add_field(field_name, std::move(value_node));

                fields = fields.as_pair()->cdr;
            }
        } else {
            while (fields.is_pair()) {
                auto arg_node = build_expression(fields.as_pair()->car, node);
                new_node->add_argument(std::move(arg_node));
                fields = fields.as_pair()->cdr;
            }
        }

        return new_node;
    }

    // ============================================================================
    // body-as-sequence
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_body_as_sequence(const soot::Object &body_forms,
                                                              Node               *node) {
        if (body_forms.is_pair() && body_forms.as_pair()->cdr.is_null()) {
            return build_expression(body_forms.as_pair()->car, node);
        }

        auto seq = std::make_unique<SequenceNode>();
        auto cur = body_forms;
        while (cur.is_pair()) {
            auto expr = build_expression(cur.as_pair()->car, node);
            seq->add(std::move(expr));
            cur = cur.as_pair()->cdr;
        }
        return seq;
    }

    // ============================================================================
    // function_signature
    // ============================================================================
    TypeSpec NodeBuilder::build_function_signature(FunctionNode *fn, const std::string &name) {
        (void)name;
        std::vector<TypeSpec> args;

        // Параметры функции
        for (const auto *var_info : fn->parameters()) {
            Type *type = var_info->type();
            args.push_back(type ? TypeSpec(type->get_name()) : TypeSpec("object"));
        }

        // Возвращаемый тип
        Type *return_type = fn->get_return_type();
        args.push_back(return_type ? TypeSpec(return_type->get_name()) : TypeSpec("object"));

        return TypeSpec("function", std::move(args));
    }

    // ============================================================================
    // while
    // ============================================================================
    std::unique_ptr<WhileNode> NodeBuilder::build_while(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_while")
                .expected("(while test body...)")
                .got("missing condition");
        }

        auto cond_form = rest.as_pair()->car;
        auto body_forms = rest.as_pair()->cdr;

        if (!body_forms.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_while")
                .expected("non-empty body")
                .got("missing body");
        }

        auto cond = build_expression(cond_form, node);
        auto body = build_body_as_sequence(body_forms, node);

        return std::make_unique<WhileNode>(
            std::move(cond),
            std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode *>(body.release())));
    }

    // ============================================================================
    // arithmetic
    // ============================================================================
    std::unique_ptr<BinaryNode> NodeBuilder::build_binary(const soot::Object &form, Node *node) {
        auto head = form.as_pair()->car.as_symbol();
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_binary")
                .expected("at least one argument")
                .got("empty form");
        }

        BinaryNode::Op op;
        if (head == "+") op = BinaryNode::Op::ADD;
        else if (head == "-")
            op = BinaryNode::Op::SUB;
        else if (head == "*")
            op = BinaryNode::Op::MUL;
        else if (head == "/")
            op = BinaryNode::Op::DIV;
        else if (head == "%")
            op = BinaryNode::Op::MOD;
        else {
            throw m_compiler->make_error(form, "NodeBuilder::build_binary")
                .expected("one of: +, -, *, /, %")
                .got(fmt::format("'{}'", std::string(head)));
        }

        // ---- Unary minus: (- x) → (0 - x) ----
        //
        // Only `-` is allowed as unary. Other ops require two operands.
        if (rest.as_pair()->cdr.is_null()) {
            if (op != BinaryNode::Op::SUB) {
                throw m_compiler->make_error(form, "NodeBuilder::build_binary")
                    .expected(fmt::format("two arguments for '{}'", std::string(head)))
                    .got("one argument");
            }
            auto zero = ConstNode::make_int(0);
            auto operand = build_expression(rest.as_pair()->car, node);
            return std::make_unique<BinaryNode>(op, std::move(zero), std::move(operand));
        }

        // ---- Binary operator ----
        auto left = build_expression(rest.as_pair()->car, node);
        auto right = build_expression(rest.as_pair()->cdr.as_pair()->car, node);

        return std::make_unique<BinaryNode>(op, std::move(left), std::move(right));
    }

    // ============================================================================
    // comparison
    // ============================================================================
    std::unique_ptr<CompareNode> NodeBuilder::build_compare(const soot::Object &form, Node *node) {
        auto head = form.as_pair()->car.as_symbol();
        auto rest = form.as_pair()->cdr;

        CompareNode::Op op;
        if (head == ">") op = CompareNode::Op::GT;
        else if (head == "<")
            op = CompareNode::Op::LT;
        else if (head == ">=")
            op = CompareNode::Op::GE;
        else if (head == "<=")
            op = CompareNode::Op::LE;
        else if (head == "==")
            op = CompareNode::Op::EQ;
        else
            op = CompareNode::Op::NE;

        auto left = build_expression(rest.as_pair()->car, node);
        auto right = build_expression(rest.as_pair()->cdr.as_pair()->car, node);

        return std::make_unique<CompareNode>(op, std::move(left), std::move(right));
    }

    // ============================================================================
    // call
    // ============================================================================
    std::unique_ptr<CallNode> NodeBuilder::build_call(const soot::Object &form, Node *node) {
        auto head = form.as_pair()->car;
        auto rest = form.as_pair()->cdr;

        if (!head.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_call")
                .expected("symbol as the function name")
                .got(fmt::format("'{}' (type: {})", head.print(), head.class_name()))
                .note("The first element of a function call must be a symbol.");
        }

        std::string func_name = head.to_std_string();
        auto        args = parse_args(rest, node);

         // ---- Typecheck against known signature (if any) ----
        if (auto sig = m_compiler->lookup_function_signature(func_name)) {
            const size_t declared_args = sig->get_args_count() - 1;

            // ---- Найти _varargs_ ----
            bool   has_varargs = false;
            size_t required = declared_args;
            for (size_t i = 0; i < declared_args; ++i) {
                const auto &arg = sig->get_arg(i);
                if (arg.base_type() == "_varargs_" || arg.print() == "_varargs_") {
                    has_varargs = true;
                    required = i;
                    break;
                }
            }

            // ---- Проверка количества ----
            if (has_varargs) {
                if (args.size() < required) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_call")
                        .expected(
                            fmt::format("at least {} arguments for '{}'", required, func_name))
                        .got(fmt::format("{} arguments", args.size()))
                        .note(fmt::format("Signature: {}", sig->print()));
                }
            } else if (args.size() != declared_args) {
                throw m_compiler->make_error(form, "NodeBuilder::build_call")
                    .expected(fmt::format("{} arguments for '{}'", declared_args, func_name))
                    .got(fmt::format("{} arguments", args.size()))
                    .note(fmt::format("Signature: {}", sig->print()));
            }

            // ---- Типчек только обязательных (до _varargs_) ----
            const size_t check_count = has_varargs ? required : declared_args;
            for (size_t i = 0; i < check_count; ++i) {
                // ... ваш существующий код типчека ...
            }
        }

        auto call = std::make_unique<CallNode>(func_name, nullptr);
        call->set_is_native(m_compiler->is_native_function(func_name));
        for (auto &arg : args) {
            call->add_argument(
                std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode *>(arg.release())));
        }
        return call;
    }

    // ============================================================================
    // variable
    // ============================================================================
    std::unique_ptr<VariableNode> NodeBuilder::build_variable(const soot::Object &form,
                                                              Node               *node) {
        if (!form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_variable")
                .expected("symbol")
                .got(form.print());
        }

        std::string name = form.as_symbol();

        Type *type = nullptr;
        if (auto *fn = node->function()) {
            if (auto *info = fn->lookup_variable(name)) type = info->type();
        }

        return std::make_unique<VariableNode>(name, type);
    }

    // ============================================================================
    // const
    // ============================================================================
    std::unique_ptr<ConstNode> NodeBuilder::build_const(const soot::Object &form, Node *node) {
        (void)node;
        if (form.is_integer()) return ConstNode::make_int(form.as_integer());
        if (form.is_float()) return ConstNode::make_float(form.as_float());
        if (form.is_string()) return ConstNode::make_string(form.to_std_string());
        return nullptr;
    }

    // ============================================================================
    // expression
    // ============================================================================
    std::unique_ptr<ExpressionNode> NodeBuilder::build_expression(const soot::Object &form,
                                                                  Node               *node) {
        auto child_node = build(form, node);

        if (!child_node) {
            // Form produced no node (e.g. seval, defmacro).
            // Treat as a no-op.
            return std::make_unique<SequenceNode>();
        }

        auto child_node_type = child_node->get_node_type_string();
        auto result =
            std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode *>(child_node.release()));
        if (result.get() == nullptr) {
            throw m_compiler->make_error(form, "NodeBuilder::build_expression")
                .expected("an ExpressionNode")
                .got(child_node_type);
        }
        return result;
    }

    // ============================================================================
    // parse_args
    // ============================================================================
    std::vector<std::unique_ptr<Node>> NodeBuilder::parse_args(const soot::Object &args_form,
                                                               Node               *node) {
        std::vector<std::unique_ptr<Node>> args;
        auto                               current = args_form;

        while (current.is_pair()) {
            args.push_back(build_expression(current.as_pair()->car, node));
            current = current.as_pair()->cdr;
        }
        return args;
    }

    // ============================================================================
    // parse_type
    // ============================================================================
    Type *NodeBuilder::parse_type(const soot::Object &type_form, Node *node) {
        (void)node;
        if (type_form.is_symbol()) {
            Type *t = m_ts.lookup_type(type_form.as_symbol());
            if (t) return t;
            throw m_compiler->make_error(type_form, "NodeBuilder::parse_type")
                .expected("known type (int, float, ...)")
                .got(fmt::format("type '{}'", type_form.as_symbol().c_str()));
        }
        return m_ts.lookup_type("object");
    }

    // ============================================================================
    // seval
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_seval(const soot::Object &form, Node *node) {
        (void)node;

        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_seval")
                .expected("(seval <form> ...)")
                .got("no arguments");
        }

        // Mirrors GOAL's Compiler::compile_seval.
        auto &soot = m_compiler->get_soot_interpreter();
        auto  env = soot.get_global_environment();

        try {
            soot::Object current = rest;
            while (current.is_pair()) {
                soot.eval_form(current.as_pair()->car, env.as_env_ptr());
                current = current.as_pair()->cdr;
            }
            if (!current.is_null()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_seval")
                    .expected("proper list of forms")
                    .got(current.print());
            }
        } catch (const std::exception &e) {
            throw m_compiler->make_error(form, "NodeBuilder::build_seval")
                .expected("successful seval")
                .got(e.what());
        }

        return std::make_unique<SequenceNode>();
    }

    // ============================================================================
    // defmacro
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_defmacro(const soot::Object &form, Node *node) {
        (void)node;

        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmacro")
                .expected("(defmacro name args body...)")
                .got("empty form");
        }

        auto name_form = rest.as_pair()->car;
        if (!name_form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmacro")
                .expected("symbol as macro name")
                .got(name_form.print());
        }

        auto after_name = rest.as_pair()->cdr;
        if (!after_name.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmacro")
                .expected("args list after name")
                .got("end of form");
        }
        auto args_form = after_name.as_pair()->car;
        auto body_form = after_name.as_pair()->cdr;

        auto macro_form =
            soot::Object::make_pair(m_compiler->get_soot_interpreter().intern("macro"),
                                    soot::Object::make_pair(args_form, body_form));

        auto define_form = soot::Object::make_pair(
            m_compiler->get_soot_interpreter().intern("define"),
            soot::Object::make_pair(
                name_form, soot::Object::make_pair(macro_form, soot::Object::make_null())));

        auto env = m_compiler->get_soot_environment();
        m_compiler->get_soot_interpreter().eval_form(define_form, env.as_env_ptr());

        lg::info("Registered macro: {}", name_form.to_std_string());
        return std::make_unique<SequenceNode>();
    }

    // ============================================================================
    // define / define-export
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_define(const soot::Object &form, Node *context) {
        const bool exported = form.as_pair()->car.is_symbol("define-export");

        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define")
                .expected("(define name value) or (define :env env name value)")
                .got("empty form");
        }

        // ---- Optional :env keyword ----
        soot::Object env_form = soot::Object::make_none();
        {
            auto first = rest.as_pair()->car;
            if (first.is_symbol() && first.is_keyword() && first.is_symbol(":env")) {
                auto after_kw = rest.as_pair()->cdr;
                if (!after_kw.is_pair()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_define")
                        .expected("environment expression after :env")
                        .got("end of form");
                }
                env_form = after_kw.as_pair()->car;
                rest = after_kw.as_pair()->cdr;
                if (!rest.is_pair()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_define")
                        .expected("name after (define :env <env>")
                        .got("end of form");
                }
            }
        }

        // ---- Name ----
        auto def_form = rest.as_pair()->car;
        if (!def_form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define")
                .expected("symbol as name")
                .got(def_form.print());
        }

        // ---- Value (with optional docstring) ----
        auto after_name = rest.as_pair()->cdr;
        if (!after_name.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define")
                .expected("value after name")
                .got("end of form");
        }

        soot::Object value_form = after_name.as_pair()->car;

        if (value_form.is_string()) {
            auto after_doc = after_name.as_pair()->cdr;
            if (!after_doc.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_define")
                    .expected("value after docstring")
                    .got("end of form");
            }
            value_form = after_doc.as_pair()->car;
        }

        std::string name = def_form.to_std_string();

        // ---- Special case: (macro ...) ----
        if (value_form.is_pair() && value_form.as_pair()->car.is_symbol() &&
            value_form.as_pair()->car.as_symbol() == "macro") {
            auto env = m_compiler->get_soot_environment();
            m_compiler->get_soot_interpreter().eval_form(form, env.as_env_ptr());
            lg::info("Registered macro: {}", name);
            return std::make_unique<SequenceNode>();
        }

        // ---- Compile the value ----
        auto value_node = build(value_form, context);
        if (!value_node) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define")
                .expected("compilable value")
                .got(value_form.print());
        }

        // ---- Value is a function ----
        if (auto *fn = dynamic_cast<FunctionNode *>(value_node.get())) {
            fn->set_name(name);
            fn->set_exported(exported);

            // ---- Register the function's signature ----
            TypeSpec sig = build_function_signature(fn, name);
            m_compiler->define_function_signature(name, sig);
            lg::info("Registered function: {} : {}", name, sig.print());

            if (auto *file = context->file()) { file->bind(name, fn); }
            return value_node;
        }

        // ---- Value is (new Type ...) ----
        if (auto *new_node = dynamic_cast<NewNode *>(value_node.get())) {
            value_node.release();
            return std::make_unique<DataDeclarationNode>(name, std::unique_ptr<NewNode>(new_node),
                                                         exported);
        }

        // ---- Value is a compile-time constant ----
        //
        // (define x 1) — treat the value as a constant known to the
        // compiler. The name is registered in the constant pool and
        // resolved wherever it appears. Nothing is emitted to the binary.
        //
        // This covers literals (numbers, strings, symbols) and, more
        // generally, any ConstNode the builder produced. Complex
        // expressions are not folded here; if you want a runtime value,
        // wrap it in (new ...).
        if (dynamic_cast<ConstNode *>(value_node.get()) != nullptr) {
            m_compiler->define_constant(name, value_form);
            lg::info("Registered constant: {} = {}", name, value_form.print());
            return std::make_unique<SequenceNode>();
        }
        // ---- Anything else ----
        throw m_compiler->make_error(form, "NodeBuilder::build_define")
            .expected("(define name (lambda ...)), (define name (new Type ...)), "
                      "or (define name (macro ...))")
            .got(fmt::format("{} (type: {})", value_form.print(), value_form.class_name()))
            .note(fmt::format("Top-level 'define' creates a global symbol in the compiled binary.\n"
                              "  Only these forms are supported:\n"
                              "    (1) functions      — (define {} (lambda ...))\n"
                              "    (2) data instances — (define {} (new Type ...))\n"
                              "    (3) macros         — (define {} (macro ...))",
                              name, name, name));
    }

    std::unique_ptr<Node> NodeBuilder::build_define_extern(const soot::Object& form, Node* node) {
        return build_define_extern_or_native(form, node, false);

    }

    std::unique_ptr<Node> NodeBuilder::build_define_native(const soot::Object &form, Node *node) {
        return build_define_extern_or_native(form, node, true);
    }

    // (define-extern _format (function _varargs_ object))
    // (define-extern wait-animate (function string string object))
    std::unique_ptr<Node> NodeBuilder::build_define_extern_or_native(const soot::Object &form,
                                                               Node               *node, bool is_native) {
        (void)node;

        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                .expected("(define-extern name (function ...))")
                .got("empty form");
        }

        // ---- Name ----
        auto name_form = rest.as_pair()->car;
        if (!name_form.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                .expected("symbol as function name")
                .got(name_form.print());
        }
        std::string name = name_form.to_std_string();

        // ---- Type spec ----
        auto after_name = rest.as_pair()->cdr;
        if (!after_name.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                .expected("type spec after name")
                .got("end of form");
        }

        auto type_form = after_name.as_pair()->car;

        // ---- Two forms of type spec: ----
        //   1. (function arg1-type arg2-type ... return-type)   — GOAL-style
        //   2. (arg-name type) (arg-name type) ...              — verbose-style
        TypeSpec sig;

        if (type_form.is_pair() && type_form.as_pair()->car.is_symbol() &&
            type_form.as_pair()->car.as_symbol() == "function") {
            // ---- Form 1: (function ...) ----
            // Parse via parse_typespec — same as define-extern in GOAL.
            try {
                sig = m_compiler->parse_typespec(type_form);
            } catch (const std::exception &e) {
                throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                    .expected("valid (function ...) type spec")
                    .got(e.what());
            }
        } else {
            // ---- Form 2: (arg-name type) pairs ----
            std::vector<TypeSpec> args;
            auto                  current = after_name;
            while (current.is_pair()) {
                auto param_form = current.as_pair()->car;
                if (!param_form.is_pair()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                        .expected("(arg-name type) pairs")
                        .got(param_form.print());
                }

                auto arg_name = param_form.as_pair()->car;
                auto arg_type = param_form.as_pair()->cdr;
                if (!arg_name.is_symbol() || !arg_type.is_pair()) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                        .expected("(arg-name type)")
                        .got(param_form.print());
                }

                try {
                    args.push_back(m_compiler->parse_typespec(arg_type.as_pair()->car));
                } catch (const std::exception &e) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_define_c_function")
                        .expected(fmt::format("valid type for argument '{}'", arg_name.print()))
                        .got(e.what());
                }

                current = current.as_pair()->cdr;
            }

            args.push_back(TypeSpec("object")); // native return type
            sig = TypeSpec("function", std::move(args));
        }
        if (is_native)
            m_compiler->define_native_signature(name, sig);
        else
            m_compiler->define_function_signature(name, sig);
        lg::info("Registered native signature: {} : {}", name, sig.print());

        return std::make_unique<SequenceNode>();
    }
    // ============================================================================
    // deftype
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_deftype(const soot::Object &form, Node *node) {
        (void)node;
        auto rest = form.as_pair()->cdr;
        try {
            DeftypeResult result = parse_deftype(rest, &m_ts);
            StringIdManager::instance().register_string(result.type.base_type());
            lg::info("Registered type: {}", result.type.print());
            return std::make_unique<TypeDeclarationNode>(result.type);
        } catch (const std::exception &e) {
            throw m_compiler->make_error(form, "NodeBuilder::build_deftype")
                .expected("valid deftype form")
                .got(e.what());
        }
    }

    // ============================================================================
    // defenum
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_defenum(const soot::Object &form, Node *node) {
        (void)node;
        auto rest = form.as_pair()->cdr;
        try {
            EnumType *enum_type = parse_defenum(rest, &m_ts);
            lg::info("Registered enum: {}", enum_type->get_name());
            return std::make_unique<EnumDeclarationNode>(enum_type->get_name());
        } catch (const std::exception &e) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defenum")
                .expected("valid defenum form")
                .got(e.what());
        }
    }

    // ============================================================================
    // defmethod
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build_defmethod(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        // ---- 1. Method name ----
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("method name as the first argument")
                .got("empty form");
        }
        const auto &method_name_obj = rest.as_pair()->car;
        if (!method_name_obj.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("symbol as method name")
                .got(method_name_obj.print());
        }
        const std::string method_name = method_name_obj.as_symbol();
        rest = rest.as_pair()->cdr;

        // ---- 2. Optional explicit type name ----
        //
        // Only `new` uses this form:
        //     (defmethod new vector ((allocation symbol) ...) ...)
        // For all other methods the list of arguments comes right away:
        //     (defmethod len ((this vector)) ...)
        std::string type_name;
        if (rest.is_pair() && rest.as_pair()->car.is_symbol()) {
            type_name = rest.as_pair()->car.to_std_string();
            rest = rest.as_pair()->cdr;
        }

        // ---- 3. Argument list ----
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("argument list")
                .got("end of form");
        }
        const auto &arg_list = rest.as_pair()->car;
        auto        body_forms = rest.as_pair()->cdr;

        // ---- 4. Build a FunctionNode ----
        auto fn = std::make_unique<FunctionNode>("<pending>");
        FunctionCompiler::parse_arguments(arg_list, fn.get(), node, *this);

        // ---- 5. Infer type_name for non-new methods ----
        //
        // For ordinary methods the owning type comes from the first argument,
        // which is `(this-name type-name)`.
        if (type_name.empty()) {
            if (!arg_list.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                    .expected("at least one argument to infer the type from")
                    .got("empty argument list");
            }
            const auto &first_arg = arg_list.as_pair()->car;
            if (!first_arg.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                    .expected("(name type) pair as the first argument")
                    .got(first_arg.print());
            }
            const auto &type_obj = first_arg.as_pair()->cdr;
            if (!type_obj.is_pair() || !type_obj.as_pair()->car.is_symbol()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                    .expected("symbol as type in the first argument")
                    .got(type_obj.print());
            }
            type_name = type_obj.as_pair()->car.to_std_string();
        }

        // ---- 6. For `new`, insert `this` as the SECOND parameter ----
        //
        // The VM calling convention for constructors is:
        //
        //     <type>-new(allocation, this, ...user_args)
        //
        // The user writes only `(allocation symbol)` at the front; `this` is
        // inserted by the compiler right after it. This is why `new` has the
        // owning type written explicitly after the method name, while ordinary
        // methods get it from their first argument.
        //
        // If the user actually wrote `this` in the argument list (some code
        // does that for clarity), we skip the insertion.
        if (method_name == "new") {
            const bool user_wrote_this = (fn->lookup_variable("this") != nullptr);
            if (!user_wrote_this) {
                Type *this_type = m_ts.lookup_type_no_throw(type_name);
                if (!this_type) {
                    throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                        .expected("known type for 'new' method")
                        .got(type_name);
                }
                fn->insert_parameter_at(1, "this", this_type);
            }
        }

        fn->set_name(fmt::format("{}-{}", type_name, method_name));
        fn->set_method_of_type(type_name);

        // ---- 7. Parse the body ----
        //
        // A method body is a sequence of forms. The value of the method is the
        // value of the last form. We wrap everything in a SequenceNode so that
        // side-effecting statements (set!, calls, ...) are not dropped.
        if (!body_forms.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("non-empty body")
                .got("empty body");
        }

        auto body_seq = std::make_unique<SequenceNode>();
        auto current = body_forms;
        while (current.is_pair()) {
            auto expr = build_expression(current.as_pair()->car, fn.get());
            body_seq->add(std::move(expr));
            current = current.as_pair()->cdr;
        }
        fn->set_body(std::move(body_seq));

        // ---- 8. Register in TypeSystem ----
        try {
            MethodInfo info = m_ts.lookup_method(type_name, method_name);
            m_ts.define_method(type_name, method_name, info.type, std::nullopt);
            lg::info("defmethod {}-{}: registered (id {}, sig {})", type_name, method_name, info.id,
                     info.type.print());
        } catch (const std::exception &e) {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("method declared in deftype :methods")
                .got(e.what());
        }

        return fn;
    }

    // ============================================================================
    // unary
    // ============================================================================
    std::unique_ptr<ExpressionNode> NodeBuilder::build_unary_common(const soot::Object &form,
                                                                    Node *node, UnaryNode::Op op,
                                                                    const char *op_name) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_unary")
                .expected(fmt::format("({} x)", op_name))
                .got("empty form");
        }
        if (!rest.as_pair()->cdr.is_null()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_unary")
                .expected(fmt::format("({} x)", op_name))
                .got("more than one argument");
        }
        auto operand = build_expression(rest.as_pair()->car, node);
        return std::make_unique<UnaryNode>(op, std::move(operand));
    }

    std::unique_ptr<ExpressionNode> NodeBuilder::build_abs(const soot::Object &form, Node *node) {
        return build_unary_common(form, node, UnaryNode::Op::ABS, "abs");
    }
    std::unique_ptr<ExpressionNode> NodeBuilder::build_neg(const soot::Object &form, Node *node) {
        return build_unary_common(form, node, UnaryNode::Op::NEG, "neg");
    }
    std::unique_ptr<ExpressionNode> NodeBuilder::build_not(const soot::Object &form, Node *node) {
        return build_unary_common(form, node, UnaryNode::Op::NOT, "not");
    }
    std::unique_ptr<ExpressionNode> NodeBuilder::build_lognot(const soot::Object &form,
                                                              Node               *node) {
        return build_unary_common(form, node, UnaryNode::Op::BITNOT, "lognot");
    }
} // namespace sootc