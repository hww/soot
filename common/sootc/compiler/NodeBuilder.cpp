#include "sootc/compiler/NodeBuilder.hpp"
#include "fmt/format.h"
#include "sootc/compiler/FunctionCompiler.hpp"
#include "common/sootc/node/FileNode.hpp"      
#include "common/sootc/node/FunctionNode.hpp"  
#include "sootc/node/Node.hpp"
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
    
    if (keyword == "+" || keyword == "-" || keyword == "*" || keyword == "/") {
        return build_binary(form, node);
    }
    
    if (keyword == ">" || keyword == "<" || keyword == ">=" || 
        keyword == "<=" || keyword == "==" || keyword == "!=") {
        return build_compare(form, node);
    }
    
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

std::unique_ptr<WhileNode> NodeBuilder::build_while(const soot::Object& form, Node* node) {
    auto rest = form.as_pair()->cdr;
    auto cond_form = rest.as_pair()->car;
    auto body_form = rest.as_pair()->cdr.as_pair()->car;
    
    auto cond = build_expression(cond_form, node);
    auto body = build_expression(body_form, node);
    
    return std::make_unique<WhileNode>(std::move(cond), std::move(body));
}

std::unique_ptr<BinaryNode> NodeBuilder::build_binary(const soot::Object& form, Node* node) {
    auto head = form.as_pair()->car.as_symbol();
    auto rest = form.as_pair()->cdr;
    
    BinaryNode::Op op;
    if (head == "+") op = BinaryNode::Op::ADD;
    else if (head == "-") op = BinaryNode::Op::SUB;
    else if (head == "*") op = BinaryNode::Op::MUL;
    else op = BinaryNode::Op::DIV;
    
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

std::unique_ptr<VariableNode> NodeBuilder::build_variable(const soot::Object& form, Node* context) {
    std::string name = form.as_symbol();
    
    Node* current = context;
    while (current) {
        if (auto* fn = dynamic_cast<FunctionNode*>(current)) {
            if (auto* info = fn->lookup_variable(name)) {
                // Создаем VariableNode с фиксированным регистром
                return std::make_unique<VariableNode>(name, info->type(), info->reg());
            }
        }
        current = current->parent();
    }
    
    // Глобальная переменная - регистр будет выделен при использовании
    // TODO: поддержка глобальных переменных
    throw std::runtime_error(fmt::format("Undefined symbol {}",form.to_std_string()));
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

Type* NodeBuilder::parse_type(const soot::Object& type_form, Node* node) {
    (void)node;
    if (type_form.is_symbol()) {
        return m_ts.lookup_type(type_form.as_symbol());
    }
    // TODO: сложные типы
    return m_ts.lookup_type("object");
}

std::unique_ptr<Node> NodeBuilder::build_define(const soot::Object& form, Node* context) {
    auto rest = form.as_pair()->cdr;
    auto def_form = rest.as_pair()->car;
    auto value_form = rest.as_pair()->cdr.as_pair()->car;
    
    if (!def_form.is_symbol()) {
        throw std::runtime_error("define: first argument must be a symbol");
    }
    
    std::string name = def_form.to_std_string();
    auto value = build(value_form, context);
    
    if (!value) {
        throw std::runtime_error(fmt::format("define: cannot compile value: {}", value_form.print()));
    }
    
    // Биндим значение напрямую (без обертки в VariableNode)
    auto file_node = context->file();
    file_node->bind(name, value.get());
    
    return value;
}

} // namespace sootc