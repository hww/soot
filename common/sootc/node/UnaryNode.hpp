#pragma once

#include "sootc/node/ExpressionNode.hpp"
#include "type_system/TypeSystem.hpp"

#include <memory>

namespace sootc {

    /// @brief Unary operation: ABS, NEG, NOT, BITNOT.
    class UnaryNode : public ExpressionNode {
    public:
        enum class Op { ABS, NEG, NOT, BITNOT };

    private:
        Op                              m_op;
        std::unique_ptr<ExpressionNode> m_operand;

    public:
        UnaryNode(Op op, std::unique_ptr<ExpressionNode> operand)
            : ExpressionNode(NodeType::ExpressionNode), m_op(op), m_operand(std::move(operand)) {
            // Type follows the operand's type (ABS/NEG preserve it; NOT always int).
            if (op == Op::NOT || op == Op::BITNOT) {
                m_type = TypeSystem::instance().lookup_type("int");
            } else if (m_operand && m_operand->get_type()) {
                m_type = m_operand->get_type();
            } else {
                m_type = TypeSystem::instance().lookup_type("int");
            }
        }

        void emit(FunctionNode &fn) override;

        const char *node_type() const override { return "UnaryNode"; }

        std::string to_string() const override {
            const char *op_str = "?";
            switch (m_op) {
            case Op::ABS: op_str = "abs"; break;
            case Op::NEG: op_str = "neg"; break;
            case Op::NOT: op_str = "not"; break;
            case Op::BITNOT: op_str = "lognot"; break;
            }
            return std::string("(") + op_str + " " + m_operand->to_string() + ")";
        }
    };

} // namespace sootc