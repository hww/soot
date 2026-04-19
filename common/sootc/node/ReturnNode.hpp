#pragma once

#include "ExpressionNode.hpp"

namespace sootc {

class ReturnNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> m_value;
    
public:
    explicit ReturnNode(std::unique_ptr<ExpressionNode> value = nullptr)
        : m_value(std::move(value)) 
    {
        if (m_value && m_value->get_type()) {
            m_type = m_value->get_type();
        }
    }
    
    void emit(FunctionNode& fn) override {
        if (m_value) {
            m_value->emit(fn);
            u8 reg = fn.get_temp_reg(m_value.get());
            fn.add_instruction(Opcode::Return, reg, 0, 0);
        } else {
            fn.add_instruction(Opcode::Return, 0, 0, 0);
        }
    }
    
    std::string to_string() const override {
        return m_value ? "return " + m_value->to_string() : "return";
    }
};

} // namespace sootc