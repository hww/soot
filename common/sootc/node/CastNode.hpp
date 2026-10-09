#pragma once

#include "sootc/node/ExpressionNode.hpp"
#include <memory>
#include <string>

namespace sootc {
    

    /// @brief Implicit numeric coercion: int -> float, float -> int, etc.
    ///
    /// @details Inserted automatically by NodeBuilder::build_call when an
    ///          argument's type doesn't exactly match the callee's expected
    ///          parameter type but a numeric conversion is allowed.
    ///
    ///          Emits a single conversion instruction:
    ///            target float : CastFloat
    ///            target int   : CastInteger
    ///            same type    : Move
    class CastNode : public ExpressionNode {
        std::unique_ptr<ExpressionNode> m_value;

    public:
        CastNode(std::unique_ptr<ExpressionNode> value, Type *target_type)
            : ExpressionNode(NodeType::CastNode, target_type), m_value(std::move(value)) {}

        void emit(FunctionNode &fn) override;

        const char *node_type() const override { return "CastNode"; }

        std::string to_string() const override {
            const std::string dst = m_type ? m_type->get_name() : "?";
            const std::string src =
                (m_value && m_value->get_type()) ? m_value->get_type()->get_name() : "?";
            return "CastNode(" + src + " -> " + dst + ")";
        }
    };

} // namespace sootc