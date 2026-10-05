#pragma once

#include "CommonTypes.hpp"
#include "ExpressionNode.hpp"
#include "FunctionNode.hpp"
#include "sootc/node/Node.hpp"
#include <stdexcept>
#include <string>

namespace sootc {

    class VariableNode : public ExpressionNode {
        std::string m_name;
        u8          m_reg = 0xFF; // 0xFF = «регистр ещё не разрешён»

    public:
        VariableNode(const std::string &name, Type *type)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name) {}

        VariableNode(const std::string &name, Type *type, u8 reg)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_reg(reg) {}

        void               set_reg(u8 reg) { m_reg = reg; }
        const std::string &name() const { return m_name; }

        void emit(FunctionNode &fn) override {
            if (m_reg == 0xFF) {
                auto *info = fn.lookup_variable(m_name);
                if (!info) {
                    throw std::runtime_error("VariableNode::emit: undefined variable '" + m_name +
                                             "'");
                }
                m_reg = info->reg();
            }
            fn.set_temp_reg(this, m_reg);
        }

        std::string to_string() const override { return m_name; }
    };

} // namespace sootc