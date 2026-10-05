#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>
#include <string>

namespace sootc {

    // (set! name value)
    class SetNode : public ExpressionNode {
        std::string                     m_name;
        std::unique_ptr<ExpressionNode> m_value;

    public:
        SetNode(const std::string &name, std::unique_ptr<ExpressionNode> value)
            : ExpressionNode(NodeType::SetNode), m_name(name), m_value(std::move(value)) {}

        void emit(FunctionNode &fn) override {
            if (!m_value) { throw std::runtime_error("SetNode::emit: no value"); }

            // 1. Вычислить значение
            m_value->emit(fn);
            u8 value_reg = fn.get_temp_reg(m_value.get());

            // 2. Найти регистр переменной
            auto *info = fn.lookup_variable(m_name);
            if (!info) {
                throw std::runtime_error("SetNode::emit: undefined variable '" + m_name + "'");
            }
            u8 local_reg = info->reg();

            // 3. Move local_reg, value_reg
            if (local_reg != value_reg) {
                fn.add_instruction(Opcode::Move, local_reg, value_reg, 0);
            }

            // 4. Результат set! = значение
            fn.set_temp_reg(this, value_reg);
            m_type = m_value->get_type();
        }

        std::string to_string() const override {
            return "(set! " + m_name + " " + m_value->to_string() + ")";
        }
    };

} // namespace sootc