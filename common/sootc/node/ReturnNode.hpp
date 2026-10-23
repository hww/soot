#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"

namespace sootc {

class ReturnNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> m_value;
    
public:
    explicit ReturnNode(std::unique_ptr<ExpressionNode> value = nullptr)
        : ExpressionNode(NodeType::ReturnNode), m_value(std::move(value)) 
    {
        adopt(m_value);
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

    void for_each_child(const std::function<void(Node &)> &f) const override {
        if (m_value) f(*m_value);
    }
};

} // namespace sootc