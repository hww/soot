#pragma once

#include "DerefNode.hpp"
#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>
#include <string>

namespace sootc {

    /// @brief (set! target value)
    /// @details Two forms of target are supported:
    ///            - a variable name:  (set! x 42)
    ///            - a field access:   (set! (-> obj field) 42)
    ///
    ///          The result of set! is the value that was stored.
    class SetNode : public ExpressionNode {
        // Either m_name is set (variable) or m_lvalue is set (field), not both.
        std::string                     m_name;
        std::unique_ptr<DerefNode>      m_lvalue;
        std::unique_ptr<ExpressionNode> m_value;

    public:
        /// @brief (set! variable value)
        SetNode(const std::string &name, std::unique_ptr<ExpressionNode> value)
            : ExpressionNode(NodeType::SetNode), m_name(name), m_value(std::move(value)) {}

        /// @brief (set! (-> obj field) value)
        SetNode(std::unique_ptr<DerefNode> lvalue, std::unique_ptr<ExpressionNode> value)
            : ExpressionNode(NodeType::SetNode), m_lvalue(std::move(lvalue)),
              m_value(std::move(value)) {}

        void emit(FunctionNode &fn) override;

        std::string to_string() const override;
    };

} // namespace sootc