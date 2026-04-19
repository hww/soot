#pragma once

#include "common/sooti/Object.hpp"
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
    std::unique_ptr<Node> build(const script::Object& form, Node* node);
    
    // Специализированные методы для разных типов форм
    std::unique_ptr<ExpressionNode> build_expression(const script::Object& form, Node* node);
    std::unique_ptr<FunctionNode> build_function(const script::Object& form, Node* node);
    std::unique_ptr<CompareNode> build_compare(const script::Object& form, Node* node);
    std::unique_ptr<BinaryNode> build_binary(const script::Object& form, Node* node);
    std::unique_ptr<IfNode> build_if(const script::Object& form, Node* node);
    std::unique_ptr<WhileNode> build_while(const script::Object& form, Node* node);
    std::unique_ptr<CallNode> build_call(const script::Object& form, Node* node);
    std::unique_ptr<VariableNode> build_variable(const script::Object& form, Node* node);
    std::unique_ptr<ConstNode> build_const(const script::Object& form, Node* node);
    
    // Вспомогательные методы
    Type* parse_type(const script::Object& type_form, Node* node);
    std::vector<std::unique_ptr<ExpressionNode>> parse_args(const script::Object& args_form, Node* node);
    
    TypeSystem& m_ts;
    Compiler* m_compiler;
};

} // namespace sootc