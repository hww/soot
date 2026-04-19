// ExpressionNode.hpp
#pragma once

#include "Node.hpp"
#include "FunctionNode.hpp"

namespace sootc {

class FunctionNode;


class ExpressionNode : public Node {
protected:
    Type* m_type = nullptr;
    
public:
    ExpressionNode() = default;
    explicit ExpressionNode(Type* type) : m_type(type) {}
    
    const char* node_type() const override { return "ExpressionNode"; }
    
    virtual void emit(FunctionNode& fn) = 0;
    
    Type* get_type() const { return m_type; }
    void set_type(Type* type) { m_type = type; }
};


} // namespace sootc