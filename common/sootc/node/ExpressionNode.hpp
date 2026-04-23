// ExpressionNode.hpp
#pragma once

#include "Node.hpp"
#include "type_system/Type.hpp"

namespace sootc {

class FunctionNode;


class ExpressionNode : public Node {
protected:
    Type* m_type = nullptr;
    
public:
    ExpressionNode() = default;
    explicit ExpressionNode(NodeType node_type) : Node(node_type), m_type(nullptr) {}
    explicit ExpressionNode(Type* type) : Node(NodeType::ExpressionNode), m_type(type) {}
    explicit ExpressionNode(NodeType node_type, Type* type) : Node(node_type), m_type(type) {}
    
    const char* node_type() const override { return "ExpressionNode"; }
    
    virtual void emit(FunctionNode& fn) = 0;
    
     ProgramBinaryElement generate(GlobalState& state) override {
        (void)state;
        // ExpressionNode не генерирует самостоятельный бинарник
        // Он генерируется только как часть FunctionNode
        throw std::runtime_error("ExpressionNode::generate should not be called directly");
        return ProgramBinaryElement(0);
    }
    
    Type* get_type() const { return m_type; }
    void set_type(Type* type) { m_type = type; }
};


} // namespace sootc