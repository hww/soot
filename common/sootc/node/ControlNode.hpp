#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"

namespace sootc {

// Базовый класс для узлов, которые влияют на поток выполнения
class ControlNode : public ExpressionNode {
public:
    using ExpressionNode::ExpressionNode;
    
    // ControlNode не возвращает значение (тип void)
    ControlNode() : ExpressionNode(NodeType::ControlNode, nullptr) {}
    ControlNode(NodeType node_type) : ExpressionNode(node_type, nullptr) {}
    
    virtual ~ControlNode() = default;
};

} // namespace sootc