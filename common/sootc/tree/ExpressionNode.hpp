// ExpressionNode.hpp
#pragma once

#include "Node.hpp"
#include "FunctionNode.hpp"

namespace sootc {

class FunctionNode;

class ExpressionNode : public Node {
public:
    const char* node_type() const override { return "ExpressionNode"; }
    
    // Главный метод - генерация кода в контексте функции
    virtual void emit(FunctionNode& fn) = 0;
};


} // namespace sootc