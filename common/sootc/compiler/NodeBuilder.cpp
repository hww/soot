#include "fmt/format.h"
#include "common/sootc/node/FileNode.hpp"      
#include "common/sootc/node/FunctionNode.hpp"  
#include "common/sootc/node/StoreGlobalNode.hpp" 
#include "common/sootc/node/FileNode.hpp"      
#include "common/sootc/node/SequenceNode.hpp"      
#include "common/sootc/compiler/NodeBuilder.hpp"
#include "common/sootc/compiler/FunctionCompiler.hpp"
#include "common/sootc/compiler/CompilerError.hpp"

#include <stdexcept>

namespace sootc {

NodeBuilder::NodeBuilder(TypeSystem& ts, Compiler* compiler)
    : m_ts(ts), m_compiler(compiler) {}

std::unique_ptr<Node> NodeBuilder::build(const soot::Object& form, Node* node) {
    if (form.is_symbol()) {
        return build_variable(form, node);
    }
    
    if (!form.is_pair()) {
        return build_const(form, node);
    }
    
    auto head = form.as_pair()->car;
    auto rest = form.as_pair()->cdr;
    
    if (!head.is_symbol()) {
        return build_call(form, node);
    }
    
    std::string keyword = head.as_symbol();
    
    // === SOOT-макросы ===
    if (m_compiler && m_compiler->is_soot_macro(keyword)) {
        auto expanded = m_compiler->expand_soot_macro(form);
        return build(expanded, node); // рекурсивно компилируем результат
    }

    // === Встроенные формы компилятора ===

    if (keyword == "define") {
        return build_define(form, node);
    }
    
    if (keyword == "lambda" || keyword == "function") {
        return build_lambda(form, node);
    }
    
    if (keyword == "if") {
        return build_if(form, node);
    }
    
    if (keyword == "while") {
        return build_while(form, node);
    }
    
    if (keyword == "+" || keyword == "-" || keyword == "*" || keyword == "/" || keyword == "%") {
        return build_binary(form, node);
    }
    
    if (keyword == ">" || keyword == "<" || keyword == ">=" || 
        keyword == "<=" || keyword == "==" || keyword == "!=") {
        return build_compare(form, node);
    }
    
    if (keyword == "let") { return build_let(form, node); }

    if (keyword == "set!") { return build_set(form, node); }

    // Обычный вызов функции
    return build_call(form, node);
}

std::unique_ptr<FunctionNode> NodeBuilder::build_lambda(const soot::Object& form, Node* node) {
    return FunctionCompiler::compile_function(form, node, *this);
}

std::unique_ptr<IfNode> NodeBuilder::build_if(const soot::Object& form, Node* node) {
    auto rest = form.as_pair()->cdr;
    auto cond_form = rest.as_pair()->car;
    auto then_form = rest.as_pair()->cdr.as_pair()->car;
    auto else_form = rest.as_pair()->cdr.as_pair()->cdr.as_pair()->car;
    
    auto cond = build_expression(cond_form, node);
    auto then_branch = build_expression(then_form, node);
    auto else_branch = else_form.is_null() ? nullptr : build_expression(else_form, node);
    
    return std::make_unique<IfNode>(
        std::move(cond),
        std::move(then_branch),
        std::move(else_branch)
    );
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

std::unique_ptr<ExpressionNode> NodeBuilder::build_body_as_sequence(const soot::Object &body_forms,
                                                                    Node               *node) {
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

std::unique_ptr<BinaryNode> NodeBuilder::build_binary(const soot::Object& form, Node* node) {
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

std::unique_ptr<CompareNode> NodeBuilder::build_compare(const soot::Object& form, Node* node) {
    auto head = form.as_pair()->car.as_symbol();
    auto rest = form.as_pair()->cdr;
    
    CompareNode::Op op;
    if (head == ">") op = CompareNode::Op::GT;
    else if (head == "<") op = CompareNode::Op::LT;
    else if (head == ">=") op = CompareNode::Op::GE;
    else if (head == "<=") op = CompareNode::Op::LE;
    else if (head == "==") op = CompareNode::Op::EQ;
    else op = CompareNode::Op::NE;
    
    auto left = build_expression(rest.as_pair()->car, node);
    auto right = build_expression(rest.as_pair()->cdr.as_pair()->car, node);
    
    return std::make_unique<CompareNode>(op, std::move(left), std::move(right));
}

std::unique_ptr<CallNode> NodeBuilder::build_call(const soot::Object& form, Node* node) {
    auto head = form.as_pair()->car;
    auto rest = form.as_pair()->cdr;
    
    std::string func_name = head.to_std_string();
    auto args = parse_args(rest, node);
    
    // Пока не знаем возвращаемый тип - будет разрешен позже
    auto call = std::make_unique<CallNode>(func_name, nullptr);
    for (auto& arg : args) {
        call->add_argument(std::move(arg));
    }
    
    return call;
}

std::unique_ptr<VariableNode> NodeBuilder::build_variable(const soot::Object &form, Node *node) {
    (void)node; // контекст не нужен — VariableNode сам разрешит при emit

    if (!form.is_symbol()) { throw std::runtime_error("build_variable: form is not a symbol"); }

    std::string name = form.as_symbol();
    return std::make_unique<VariableNode>(name, /* type */ nullptr);
}

std::unique_ptr<ConstNode> NodeBuilder::build_const(const soot::Object& form, Node* node) {
    (void)form; (void)node;
    if (form.is_integer()) {
        return ConstNode::make_int(form.as_integer());
    }
    if (form.is_float()) {
        return ConstNode::make_float(form.as_float());
    }
    if (form.is_string()) {
        return ConstNode::make_string(form.to_std_string());
    }
    
    return nullptr;
}

std::unique_ptr<ExpressionNode> NodeBuilder::build_expression(const soot::Object& form, Node* node) {
    auto child_node = build(form, node);
    auto child_node_type = child_node->get_node_type_string();

    auto result = std::unique_ptr<ExpressionNode>(dynamic_cast<ExpressionNode*>(child_node.release()));
    if (result.get() == nullptr)
        throw std::runtime_error(fmt::format("build_expression can't cast {} to ExpressionNode", child_node_type));
    return result;
}

std::vector<std::unique_ptr<ExpressionNode>> NodeBuilder::parse_args(const soot::Object& args_form, Node* node) {
    std::vector<std::unique_ptr<ExpressionNode>> args;
    auto current = args_form;
    
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

std::unique_ptr<Node> NodeBuilder::build_define(const soot::Object &form, Node *context) {
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

    // Если значение — функция, регистрируем её под именем define и возвращаем её.
    if (auto *fn = dynamic_cast<FunctionNode *>(value_node.get())) {
        fn->set_name(name);
        if (auto *file = context->file()) { file->bind(name, fn); }
        return value_node; // ← FunctionNode как ребёнок FileNode
    }

    // Иначе пока не поддерживаем — отдельный патч 4b.
    throw CompilerError("NodeBuilder::build_define")
        .where(fmt::format("define '{}'", name))
        .expected("value form 'lambda' (function definition)")
        .got(fmt::format("value form of type '{}'", value_form.class_name()))
        .note("top-level 'define' of non-function values is not yet implemented");
}

} // namespace sootc