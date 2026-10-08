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

        // Declarations (compiler-only)
        m_form_table["defmacro"] = &NodeBuilder::build_defmacro_wrap;
        m_form_table["deftype"] = &NodeBuilder::build_deftype_wrap;
        m_form_table["defenum"] = &NodeBuilder::build_defenum_wrap;
        m_form_table["defmethod"] = &NodeBuilder::build_defmethod_wrap;
        m_form_table["seval"] = &NodeBuilder::build_seval_wrap;
    }

    // ============================================================================
    // build — single dispatch
    // ============================================================================
    std::unique_ptr<Node> NodeBuilder::build(const soot::Object &form, Node *node) {
        if (form.is_symbol()) return build_variable(form, node);
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
            return std::make_unique<MethodCallNode>(std::move(expr), full_name, std::move(args),
                                                    return_type);
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
        if (!first.is_symbol()) {
            throw m_compiler->make_error(form, "NodeBuilder::build_new")
                .expected("symbol as type name (or allocation)")
                .got(first.print());
        }

        std::string first_str = first.as_symbol();

        if (first_str == "static" || first_str == "global" || first_str == "heap" ||
            first_str == "stack") {
            allocation = first_str;
            rest = rest.as_pair()->cdr;

            if (!rest.is_pair()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_new")
                    .expected("type name after allocation")
                    .got("end of form");
            }

            const auto &type_obj = rest.as_pair()->car;
            if (!type_obj.is_symbol()) {
                throw m_compiler->make_error(form, "NodeBuilder::build_new")
                    .expected("symbol as type name")
                    .got(type_obj.print());
            }
            type_name = type_obj.to_std_string();
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

        auto fields = rest.as_pair()->cdr;

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
                auto arg = build_expression(fields.as_pair()->car, node);
                new_node->add_argument(std::move(arg));
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

        auto call = std::make_unique<CallNode>(func_name, nullptr);
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
            if (auto *file = context->file()) { file->bind(name, fn); }
            return value_node;
        }

        // ---- Value is (new Type ...) ----
        if (auto *new_node = dynamic_cast<NewNode *>(value_node.get())) {
            value_node.release();
            return std::make_unique<DataDeclarationNode>(name, std::unique_ptr<NewNode>(new_node),
                                                         exported);
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

        fn->set_name(fmt::format("{}-{}", type_name, method_name));
        fn->set_method_of_type(type_name);

        // ---- 5. Parse the body ----
        auto                            current = body_forms;
        std::unique_ptr<ExpressionNode> last_expr;
        while (current.is_pair()) {
            last_expr = build_expression(current.as_pair()->car, fn.get());
            current = current.as_pair()->cdr;
        }
        if (last_expr) {
            fn->set_body(std::move(last_expr));
        } else {
            throw m_compiler->make_error(form, "NodeBuilder::build_defmethod")
                .expected("non-empty body")
                .got("empty body");
        }

        // ---- 6. Register in TypeSystem ----
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

} // namespace sootc