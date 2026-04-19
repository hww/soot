#pragma once

#include "ExpressionNode.hpp"

namespace sootc {

// Базовый класс для узлов, которые влияют на поток выполнения
class ControlNode : public ExpressionNode {
public:
    using ExpressionNode::ExpressionNode;
    
    // ControlNode не возвращает значение (тип void)
    ControlNode() : ExpressionNode(nullptr) {}
    
    virtual ~ControlNode() = default;
};

} // namespace sootc