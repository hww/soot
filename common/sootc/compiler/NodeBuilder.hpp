#pragma once

#include "common/soot/Object.hpp"
#include "sootc/compiler/Compiler.hpp"
#include "sootc/node/BinaryNode.hpp"
#include "sootc/node/CompareNode.hpp"
#include "sootc/node/ConstNode.hpp"
#include "sootc/node/CallNode.hpp"
#include "sootc/node/IfNode.hpp"
#include "sootc/node/VariableNode.hpp"
#include "sootc/node/WhileNode.hpp"
#include "type_system/TypeSystem.hpp"
#include <memory>

namespace sootc {

    class Compiler;

class NodeBuilder {
public:
    NodeBuilder(TypeSystem& ts, Compiler* compiler);
    
    // Главный метод - строит узел из AST
    std::unique_ptr<Node> build(const soot::Object& form, Node* node);
    
    // Специализированные методы для разных типов форм
    std::unique_ptr<ExpressionNode> build_expression(const soot::Object& form, Node* node);
    std::unique_ptr<FunctionNode> build_lambda(const soot::Object& form, Node* node);
    std::unique_ptr<CompareNode> build_compare(const soot::Object& form, Node* node);
    std::unique_ptr<BinaryNode> build_binary(const soot::Object& form, Node* node);
    std::unique_ptr<IfNode> build_if(const soot::Object& form, Node* node);
    std::unique_ptr<WhileNode> build_while(const soot::Object& form, Node* node);
    std::unique_ptr<CallNode> build_call(const soot::Object& form, Node* node);
    std::unique_ptr<VariableNode> build_variable(const soot::Object& form, Node* node);
    std::unique_ptr<ConstNode> build_const(const soot::Object& form, Node* node);
    std::unique_ptr<Node> build_define(const soot::Object& form, Node* context);

    // Вспомогательные методы
    Type* parse_type(const soot::Object& type_form, Node* node);
    std::vector<std::unique_ptr<ExpressionNode>> parse_args(const soot::Object& args_form, Node* node);
    
    TypeSystem& m_ts;
    Compiler* m_compiler;
};

} // namespace sootc