#pragma once


#include "CommonTypes.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"

namespace sootc {

class VariableNode : public ExpressionNode {
    std::string m_name;
    u8 m_reg;
    
public:
    VariableNode(const std::string& name, u8 reg) 
        : m_name(name), m_reg(reg) {}
    
    void emit(FunctionNode& fn) override {
        // Переменная уже в регистре, просто запоминаем результат
        fn.set_reg(this, m_reg);  // ← set_reg, а не set_result
    }
    
    std::string to_string() const override {
        return m_name;
    }
};
}
