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

#include <stdexcept>
#include <sootc/node/MethodCallNode.hpp>

namespace sootc {

    NodeBuilder::NodeBuilder(TypeSystem &ts, Compiler *compiler) : m_ts(ts), m_compiler(compiler) {}

    std::unique_ptr<Node> NodeBuilder::build(const soot::Object &form, Node *node) {
        if (form.is_symbol()) { return build_variable(form, node); }

        if (!form.is_pair()) { return build_const(form, node); }

        auto head = form.as_pair()->car;
        auto rest = form.as_pair()->cdr;

        if (!head.is_symbol()) { return build_call(form, node); }

        std::string keyword = head.as_symbol();

        // === SOOT-макросы ===
        if (m_compiler && m_compiler->is_soot_macro(keyword)) {
            auto expanded = m_compiler->expand_soot_macro(form);
            return build(expanded, node); // рекурсивно компилируем результат
        }

        // === Встроенные формы компилятора ===

        if (keyword == "define") { return build_define(form, node, /*exported=*/false); }

        if (keyword == "define-export") { return build_define(form, node, /*exported=*/true); }

        if (keyword == "lambda" || keyword == "function") { return build_lambda(form, node); }

        if (keyword == "if") { return build_if(form, node); }

        if (keyword == "while") { return build_while(form, node); }

        if (keyword == "+" || keyword == "-" || keyword == "*" || keyword == "/" ||
            keyword == "%") {
            return build_binary(form, node);
        }

        if (keyword == ">" || keyword == "<" || keyword == ">=" || keyword == "<=" ||
            keyword == "==" || keyword == "!=") {
            return build_compare(form, node);
        }

        if (keyword == "let") { return build_let(form, node); }

        if (keyword == "set!") { return build_set(form, node); }
        if (keyword == "new") { return build_new(form, node); }

        if (keyword == "deftype") return build_deftype(form, node);
        if (keyword == "defenum") return build_defenum(form, node);
        if (keyword == "defmethod") return build_defmethod(form, node);
        if (keyword == "->") { return build_deref(form, node); }
        if (keyword == "begin") {
            auto body_forms = form.as_pair()->cdr;

            auto seq = std::make_unique<SequenceNode>();
            auto cur = body_forms;
            while (cur.is_pair()) {
                auto &child = cur.as_pair()->car;

                if (child.is_pair() && child.as_pair()->car.is_symbol()) {
                    const std::string kw = child.as_pair()->car.as_symbol();

                    // Top-level declarations inside (begin ...) are hoisted onto
                    // the FileNode. They are not expressions, so they cannot go
                    // into the sequence.
                    if (kw == "deftype" || kw == "defenum") {
                        auto decl = build(child, node);
                        if (auto *file = node->file()) { file->add_child(std::move(decl)); }
                        cur = cur.as_pair()->cdr;
                        continue;
                    }
                    if (kw == "defmethod") {
                        auto fn = build_defmethod(child, node);
                        if (auto *file = node->file()) {
                            FunctionNode *raw = fn.get();
                            file->add_child(std::move(fn));
                            file->bind(raw->name(), raw);
                        }
                        cur = cur.as_pair()->cdr;
                        continue;
                    }
                    // define / define-export at any level produce a FunctionNode
                    // or DataDeclarationNode. Both are hoisted onto the FileNode:
                    // they are named declarations, not expressions.
                    if (kw == "define" || kw == "define-export") {
                        const bool exported = (kw == "define-export");
                        auto       decl = build_define(child, node, exported);
                        if (auto *file = node->file()) {
                            if (auto *fn = dynamic_cast<FunctionNode *>(decl.get())) {
                                FunctionNode *raw = fn;
                                file->add_child(std::move(decl));
                                file->bind(raw->name(), raw);
                            } else if (dynamic_cast<DataDeclarationNode *>(decl.get()) != nullptr) {
                                file->add_child(std::move(decl));
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

        // Обычный вызов функции
        return build_call(form, node);
    }

    std::unique_ptr<FunctionNode> NodeBuilder::build_lambda(const soot::Object &form, Node *node) {
        return FunctionCompiler::compile_function(form, node, *this);
    }

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

    std::unique_ptr<LetNode> NodeBuilder::build_let(const soot::Object &form, Node *node) {
        // (let ((a 1) (b 2)) body...)
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) { throw std::runtime_error("let: missing bindings list"); }

        auto bindings_form = rest.as_pair()->car;
        auto body_forms = rest.as_pair()->cdr;

        if (!bindings_form.is_null() && !bindings_form.is_pair()) {
            throw std::runtime_error("let: bindings must be a list or null");
        }

        auto let_node = std::make_unique<LetNode>();

        // Парсим bindings
        auto cur = bindings_form;
        while (cur.is_pair()) {
            auto binding = cur.as_pair()->car;
            if (!binding.is_pair()) {
                throw std::runtime_error("let: each binding must be (name value)");
            }

            auto name_obj = binding.as_pair()->car;
            auto value_obj = binding.as_pair()->cdr;

            if (!name_obj.is_symbol()) {
                throw std::runtime_error("let: binding name must be a symbol");
            }
            if (!value_obj.is_pair()) { throw std::runtime_error("let: binding must have value"); }

            std::string name = name_obj.as_symbol();
            auto        value = build_expression(value_obj.as_pair()->car, node);

            let_node->add_binding(name, std::move(value));
            cur = cur.as_pair()->cdr;
        }

        // Парсим тело — SequenceNode, если форм несколько
        auto body = build_body_as_sequence(body_forms, node);
        if (!body) { throw std::runtime_error("let: missing body"); }
        let_node->set_body(std::move(body));

        return let_node;
    }

    std::unique_ptr<SetNode> NodeBuilder::build_set(const soot::Object &form, Node *node) {
        // (set! name value)
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) { throw std::runtime_error("set!: missing name"); }
        auto name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol()) { throw std::runtime_error("set!: name must be a symbol"); }

        if (!rest.as_pair()->cdr.is_pair()) { throw std::runtime_error("set!: missing value"); }
        auto value_obj = rest.as_pair()->cdr.as_pair()->car;

        std::string name = name_obj.as_symbol();
        auto        value = build_expression(value_obj, node);

        return std::make_unique<SetNode>(name, std::move(value));
    }
    /// @brief Parse (-> expr field-or-method [args...]).
    /// @details First tries to resolve the name as a field of the base expression's
    ///          static type. If found, this is a plain DerefNode. Otherwise, tries
    ///          to resolve it as a method and produces a MethodCallNode.
    std::unique_ptr<ExpressionNode> NodeBuilder::build_deref(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw CompilerError("NodeBuilder::build_deref")
                .where("->")
                .expected("expression and field/method name")
                .got("empty form");
        }

        auto expr_form = rest.as_pair()->car;
        auto field_form = rest.as_pair()->cdr.as_pair()->car;

        if (!field_form.is_symbol()) {
            throw CompilerError("NodeBuilder::build_deref")
                .where("->")
                .expected("field or method name as a symbol")
                .got(field_form.print());
        }

        std::string name = field_form.as_symbol();

        // Build the base expression.
        auto expr = build_expression(expr_form, node);

        // Resolve the static type of the base expression.
        Type *expr_type = expr->get_type();
        if (!expr_type) {
            throw CompilerError("NodeBuilder::build_deref")
                .where(fmt::format("(-> ... {})", name))
                .expected("base expression with a known type")
                .got("unknown");
        }

        // If the base is a pointer, dereference to get the structure type.
        Type *struct_type = expr_type;
        {
            DerefInfo di = m_ts.get_deref_info(TypeSpec(expr_type->get_name()));
            if (di.can_deref && di.result_type.get()) { struct_type = di.result_type.get(); }
        }

        auto *st = dynamic_cast<StructureType *>(struct_type);
        if (!st) {
            throw CompilerError("NodeBuilder::build_deref")
                .where(fmt::format("(-> ... {})", name))
                .expected("structure type")
                .got(struct_type ? struct_type->get_name() : "unknown");
        }

        // --- Try field first. ---
        Field field;
        if (st->lookup_field(name, &field)) {
            Type *field_type = m_ts.lookup_type_no_throw(field.type().base_type());
            if (!field_type) {
                throw CompilerError("NodeBuilder::build_deref")
                    .where(fmt::format("(-> ... {})", name))
                    .expected("known field type")
                    .got(field.type().print());
            }
            return std::make_unique<DerefNode>(std::move(expr), name,
                                               static_cast<u32>(field.offset()), field_type);
        }

        // --- Try method. ---
        MethodInfo method_info;
        if (m_ts.try_lookup_method(struct_type->get_name(), name, &method_info)) {
            // Collect any remaining forms as arguments.
            std::vector<std::unique_ptr<ExpressionNode>> args;
            auto arg_forms = rest.as_pair()->cdr.as_pair()->cdr;
            while (arg_forms.is_pair()) {
                args.push_back(build_expression(arg_forms.as_pair()->car, node));
                arg_forms = arg_forms.as_pair()->cdr;
            }

            // The return type is the last argument of the method's typespec.
            Type *return_type = nullptr;
            if (!method_info.type.empty()) {
                return_type = m_ts.lookup_type_no_throw(method_info.type.last_arg().base_type());
            }

            const std::string full_name = fmt::format("{}-{}", struct_type->get_name(), name);
            return std::make_unique<MethodCallNode>(std::move(expr), full_name, std::move(args),
                                                    return_type);
        }

        throw CompilerError("NodeBuilder::build_deref")
            .where(fmt::format("(-> {} {})", struct_type->get_name(), name))
            .expected("a known field or method")
            .got("unknown");
    }

    std::unique_ptr<NewNode> NodeBuilder::build_new(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;

        if (!rest.is_pair()) {
            throw CompilerError("NodeBuilder::build_new")
                .where("new")
                .expected("type name (optionally preceded by an allocation symbol)")
                .got("empty form");
        }

        // ---- Allocation and type name ----
        //
        // Two forms are accepted:
        //    (new Type ...)              — allocation defaults to 'static'
        //    (new alloc Type ...)        — alloc is one of static/global/heap/stack
        //
        // The first symbol is examined: if it is a known allocation keyword, it
        // is consumed and the next symbol is the type. Otherwise it is the type
        // and allocation stays at its default.
        std::string allocation = "static";
        std::string type_name;

        const auto &first = rest.as_pair()->car;
        if (!first.is_symbol()) {
            throw CompilerError("NodeBuilder::build_new")
                .where("new")
                .expected("symbol as type name (or allocation)")
                .got(first.print());
        }

        std::string first_str = first.as_symbol();

        if (first_str == "static" || first_str == "global" || first_str == "heap" ||
            first_str == "stack") {
            allocation = first_str;
            rest = rest.as_pair()->cdr;

            if (!rest.is_pair()) {
                throw CompilerError("NodeBuilder::build_new")
                    .where(fmt::format("new {}", allocation))
                    .expected("type name after allocation")
                    .got("end of form");
            }

            const auto &type_obj = rest.as_pair()->car;
            if (!type_obj.is_symbol()) {
                throw CompilerError("NodeBuilder::build_new")
                    .where(fmt::format("new {}", allocation))
                    .expected("symbol as type name")
                    .got(type_obj.print());
            }
            type_name = type_obj.to_std_string();
        } else {
            type_name = first_str;
        }

        // ---- Look up the type ----
        Type *type = m_ts.lookup_type_no_throw(type_name);
        if (!type) {
            throw CompilerError("NodeBuilder::build_new")
                .where(fmt::format("new {} {}", allocation, type_name))
                .expected("a known type")
                .got("unknown type");
        }

        auto new_node = std::make_unique<NewNode>(TypeSpec(type_name));
        new_node->set_allocation(allocation);

        // ---- Parse the rest ----
        auto fields = rest.as_pair()->cdr;

        if (allocation == "static") {
            // Static initialization: keyword-args are field names.
            while (fields.is_pair()) {
                const auto &field_name_obj = fields.as_pair()->car;

                if (!field_name_obj.is_keyword()) {
                    throw CompilerError("NodeBuilder::build_new")
                        .where(fmt::format("new static {} ...", type_name))
                        .expected(":field-name as a keyword")
                        .got(field_name_obj.print());
                }

                std::string field_name = field_name_obj.as_symbol().name_ptr;
                if (!field_name.empty() && field_name[0] == ':') {
                    field_name = field_name.substr(1);
                }

                fields = fields.as_pair()->cdr;
                if (!fields.is_pair()) {
                    throw CompilerError("NodeBuilder::build_new")
                        .where(fmt::format("new static {} :{}", type_name, field_name))
                        .expected("value after the field name")
                        .got("end of form");
                }

                auto value_node = build_expression(fields.as_pair()->car, node);
                new_node->add_field(field_name, std::move(value_node));

                fields = fields.as_pair()->cdr;
            }
        } else {
            // Constructor call: positional arguments.
            while (fields.is_pair()) {
                auto arg = build_expression(fields.as_pair()->car, node);
                new_node->add_argument(std::move(arg));
                fields = fields.as_pair()->cdr;
            }
        }

        return new_node;
    }

    std::unique_ptr<ExpressionNode>
    NodeBuilder::build_body_as_sequence(const soot::Object &body_forms, Node *node) {
        // Одна форма — вернуть её напрямую
        if (body_forms.is_pair() && body_forms.as_pair()->cdr.is_null()) {
            return build_expression(body_forms.as_pair()->car, node);
        }

        // Несколько форм — SequenceNode
        auto seq = std::make_unique<SequenceNode>();
        auto cur = body_forms;
        while (cur.is_pair()) {
            auto expr = build_expression(cur.as_pair()->car, node);
            seq->add(std::move(expr));
            cur = cur.as_pair()->cdr;
        }
        return seq;
    }

    std::unique_ptr<WhileNode> NodeBuilder::build_while(const soot::Object &form, Node *node) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) { throw std::runtime_error("while: missing condition"); }

        auto cond_form = rest.as_pair()->car;
        auto body_forms = rest.as_pair()->cdr; // ← ВСЁ тело, а не только первая форма

        if (!body_forms.is_pair()) { throw std::runtime_error("while: missing body"); }

        auto cond = build_expression(cond_form, node);
        auto body = build_body_as_sequence(body_forms, node); // ← SequenceNode для всех форм

        return std::make_unique<WhileNode>(std::move(cond), std::move(body));
    }

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
        else
            throw CompilerError("NodeBuilder::build_binary")
                .where(fmt::format("op '{}'", std::string(head)))
                .expected("one of: +, -, *, /, %")
                .got(fmt::format("'{}'", std::string(head)));

        auto left = build_expression(rest.as_pair()->car, node);
        auto right = build_expression(rest.as_pair()->cdr.as_pair()->car, node);

        return std::make_unique<BinaryNode>(op, std::move(left), std::move(right));
    }

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

    std::unique_ptr<CallNode> NodeBuilder::build_call(const soot::Object &form, Node *node) {
        auto head = form.as_pair()->car;
        auto rest = form.as_pair()->cdr;

        std::string func_name = head.to_std_string();
        auto        args = parse_args(rest, node);

        // Пока не знаем возвращаемый тип - будет разрешен позже
        auto call = std::make_unique<CallNode>(func_name, nullptr);
        for (auto &arg : args) { call->add_argument(std::move(arg)); }

        return call;
    }

    std::unique_ptr<VariableNode> NodeBuilder::build_variable(const soot::Object &form,
                                                              Node               *node) {
        if (!form.is_symbol()) { throw std::runtime_error("build_variable: form is not a symbol"); }

        std::string name = form.as_symbol();

        // Try to resolve the type right away from the enclosing function, so that
        // forms like (-> v len) can look up the field/method at compile time.
        // The actual register is still resolved lazily in VariableNode::emit.
        Type *type = nullptr;
        if (auto *fn = node->function()) {
            if (auto *info = fn->lookup_variable(name)) { type = info->type(); }
        }

        return std::make_unique<VariableNode>(name, type);
    }

    std::unique_ptr<ConstNode> NodeBuilder::build_const(const soot::Object &form, Node *node) {
        (void)form;
        (void)node;
        if (form.is_integer()) { return ConstNode::make_int(form.as_integer()); }
        if (form.is_float()) { return ConstNode::make_float(form.as_float()); }
        if (form.is_string()) { return ConstNode::make_string(form.to_std_string()); }

        return nullptr;
    }

    std::unique_ptr<ExpressionNode> NodeBuilder::build_expression(const soot::Object &form,
                                                                  Node               *node) {
        auto child_node = build(form, node);
        auto child_node_type = child_node->get_node_type_string();

        auto result =
            std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode *>(child_node.release()));
        if (result.get() == nullptr)
            throw std::runtime_error(
                fmt::format("build_expression can't cast {} to ExpressionNode", child_node_type));
        return result;
    }

    std::vector<std::unique_ptr<ExpressionNode>>
    NodeBuilder::parse_args(const soot::Object &args_form, Node *node) {
        std::vector<std::unique_ptr<ExpressionNode>> args;
        auto                                         current = args_form;

        while (current.is_pair()) {
            args.push_back(build_expression(current.as_pair()->car, node));
            current = current.as_pair()->cdr;
        }

        return args;
    }

    Type *NodeBuilder::parse_type(const soot::Object &type_form, Node *node) {
        (void)node;
        if (type_form.is_symbol()) {
            Type *t = m_ts.lookup_type(type_form.as_symbol());
            if (t) return t;
            // Если типа нет — ошибка с понятным сообщением
            throw CompilerError("NodeBuilder::parse_type")
                .where(fmt::format("type '{}'", type_form.as_symbol().c_str()))
                .expected("known type (int, float, ...)")
                .got("unknown type");
        }
        // Сложные типы: пока object
        return m_ts.lookup_type("object");
    }

    std::unique_ptr<Node> NodeBuilder::build_define(const soot::Object &form, Node *context,
                                                    bool exported) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) { throw std::runtime_error("define: missing name"); }
        auto def_form = rest.as_pair()->car;
        auto value_form = rest.as_pair()->cdr.as_pair()->car;

        if (!def_form.is_symbol()) {
            throw std::runtime_error("define: first argument must be a symbol");
        }

        std::string name = def_form.to_std_string();
        auto        value_node = build(value_form, context);
        if (!value_node) {
            throw std::runtime_error(
                fmt::format("define: cannot compile value: {}", value_form.print()));
        }

        // Value is a function — register it under `name` and return it.
        if (auto *fn = dynamic_cast<FunctionNode *>(value_node.get())) {
            fn->set_name(name);
            fn->set_exported(exported);
            if (auto *file = context->file()) { file->bind(name, fn); }
            return value_node;
        }

        // Value is (new Type ...) — a data instance.
        if (auto *new_node = dynamic_cast<NewNode *>(value_node.get())) {
            value_node.release(); // ownership transfers to DataDeclarationNode
            auto data_decl = std::make_unique<DataDeclarationNode>(
                name, std::unique_ptr<NewNode>(new_node), exported);
            return data_decl;
        }

        // Anything else — not supported yet.
        throw CompilerError("NodeBuilder::build_define")
            .where(fmt::format("define '{}'", name))
            .expected("value form 'lambda' (function) or 'new' (data instance)")
            .got(fmt::format("value form of type '{}'", value_form.class_name()))
            .note("top-level 'define' of arbitrary expressions is not yet implemented");
    }

    std::unique_ptr<Node> NodeBuilder::build_deftype(const soot::Object &form, Node *node) {
        (void)node;
        auto rest = form.as_pair()->cdr;
        try {
            DeftypeResult result = parse_deftype(rest, &m_ts);
            // Register the type name so that sid_str(SID("vec4")) resolves to "vec4".
            StringIdManager::instance().register_string(result.type.base_type());
            lg::info("Registered type: {}", result.type.print());
            return std::make_unique<TypeDeclarationNode>(result.type);
        } catch (const std::exception &e) {
            throw CompilerError("NodeBuilder::build_deftype")
                .where("deftype")
                .expected("valid deftype form")
                .got(e.what());
        }
    }

    std::unique_ptr<Node> NodeBuilder::build_defenum(const soot::Object &form, Node *node) {
        (void)node;

        auto rest = form.as_pair()->cdr;

        try {
            EnumType *enum_type = parse_defenum(rest, &m_ts);
            lg::info("Registered enum: {}", enum_type->get_name());
            return std::make_unique<EnumDeclarationNode>(enum_type->get_name());
        } catch (const std::exception &e) {
            throw CompilerError("NodeBuilder::build_defenum")
                .where("defenum")
                .expected("valid defenum form")
                .got(e.what());
        }
    }

    // ============================================================================
    // defmethod
    // ============================================================================
    /// @brief Compile a (defmethod ...) form.
    /// @details Syntax (same as GOAL):
    ///            (defmethod <method-name> [<type-name>] <args> <body>...)
    ///
    ///          If <type-name> is omitted, it is inferred from the first
    ///          argument's type. The first argument is conventionally named
    ///          "this" for non-new methods.
    ///
    ///          The result is a FunctionNode named "<type>-<method>". Its
    ///          method_of_type() is set to <type-name>, so FileNode can emit it as
    ///          a ScriptLambda and later tools can find the method in TypeSystem.
    ///
    ///          The signature MUST already be declared in the corresponding
    ///          deftype's :methods section. define_method verifies compatibility.
    std::unique_ptr<FunctionNode> NodeBuilder::build_defmethod(const soot::Object &form,
                                                               Node               *node) {
        auto rest = form.as_pair()->cdr;

        // ---- 1. Method name ----
        if (!rest.is_pair()) {
            throw CompilerError("NodeBuilder::build_defmethod")
                .where("defmethod")
                .expected("method name as the first argument")
                .got("empty form");
        }
        const auto &method_name_obj = rest.as_pair()->car;
        if (!method_name_obj.is_symbol()) {
            throw CompilerError("NodeBuilder::build_defmethod")
                .where("defmethod")
                .expected("symbol as method name")
                .got(method_name_obj.print());
        }
        const std::string method_name = method_name_obj.as_symbol();
        rest = rest.as_pair()->cdr;

        // ---- 2. Optional explicit type name ----
        // If the next form is a symbol (not a list), it is the type name. Otherwise
        // the type is inferred from the first argument.
        std::string type_name;
        if (rest.is_pair() && rest.as_pair()->car.is_symbol()) {
            type_name = rest.as_pair()->car.to_std_string();
            rest = rest.as_pair()->cdr;
        }

        // ---- 3. Argument list ----
        if (!rest.is_pair()) {
            throw CompilerError("NodeBuilder::build_defmethod")
                .where(fmt::format("defmethod {}", method_name))
                .expected("argument list")
                .got("end of form");
        }
        const auto &arg_list = rest.as_pair()->car;
        auto        body_forms = rest.as_pair()->cdr;

        // ---- 4. Build a FunctionNode for the method body ----
        // The name is set after we know the type.
        auto fn = std::make_unique<FunctionNode>("<pending>");

        // Parse arguments. The first argument's type determines the method's type
        // if type_name is empty.
        FunctionCompiler::parse_arguments(arg_list, fn.get(), node, *this);

        if (type_name.empty()) {
            // Infer from the first parameter's type.
            if (!arg_list.is_pair()) {
                throw CompilerError("NodeBuilder::build_defmethod")
                    .where(fmt::format("defmethod {}", method_name))
                    .expected("at least one argument to infer the type from")
                    .got("empty argument list");
            }
            const auto &first_arg = arg_list.as_pair()->car;
            if (!first_arg.is_pair()) {
                throw CompilerError("NodeBuilder::build_defmethod")
                    .where(fmt::format("defmethod {}", method_name))
                    .expected("(name type) pair as the first argument")
                    .got(first_arg.print());
            }
            // (name type) — take the type (cdr of the arg pair, first element)
            const auto &type_obj = first_arg.as_pair()->cdr;
            if (!type_obj.is_pair() || !type_obj.as_pair()->car.is_symbol()) {
                throw CompilerError("NodeBuilder::build_defmethod")
                    .where(fmt::format("defmethod {}", method_name))
                    .expected("symbol as type in the first argument")
                    .got(type_obj.print());
            }
            type_name = type_obj.as_pair()->car.to_std_string();
        }

        // Set the composite name and mark the function as a method.
        fn->set_name(fmt::format("{}-{}", type_name, method_name));
        fn->set_method_of_type(type_name);

        // ---- 5. Parse the body ----
        // Reuse the same logic as compile_function: build each form; the last one
        // is the return value. We don't wrap in a ReturnNode here because the
        // FunctionNode::emit_body already appends a Return if the body doesn't end
        // with one.
        auto                            current = body_forms;
        std::unique_ptr<ExpressionNode> last_expr;
        while (current.is_pair()) {
            last_expr = build_expression(current.as_pair()->car, fn.get());
            current = current.as_pair()->cdr;
        }
        if (last_expr) {
            fn->set_body(std::move(last_expr));
        } else {
            throw CompilerError("NodeBuilder::build_defmethod")
                .where(fmt::format("defmethod {}-{}", type_name, method_name))
                .expected("non-empty body")
                .got("empty body");
        }

        // ---- 6. Register the method in TypeSystem ----
        // The signature was already declared in deftype's :methods section. We
        // only bind the implementation. define_method verifies compatibility and
        // throws if the method was never declared.
        try {
            MethodInfo info = m_ts.lookup_method(type_name, method_name);
            m_ts.define_method(type_name, method_name, info.type, std::nullopt);
            lg::info("defmethod {}-{}: registered (id {}, sig {})", type_name, method_name, info.id,
                     info.type.print());
        } catch (const std::exception &e) {
            throw CompilerError("NodeBuilder::build_defmethod")
                .where(fmt::format("defmethod {}-{}", type_name, method_name))
                .expected("method declared in deftype :methods")
                .got(e.what());
        }

        return fn;
    }

} // namespace sootc