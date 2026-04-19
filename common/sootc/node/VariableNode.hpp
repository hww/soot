#pragma once


#include "CommonTypes.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"

namespace sootc {
    
class VariableNode : public ExpressionNode {
    std::string m_name;
    u8 m_reg;
public:
    VariableNode(const std::string& name, Type* type, u8 reg) 
        : ExpressionNode(type), m_name(name), m_reg(reg) {}
    VariableNode(const std::string& name, Type* type) 
        : ExpressionNode(type), m_name(name), m_reg(0) {}

    void set_reg(u8 reg) { m_reg = reg; }
            
    void emit(FunctionNode& fn) override {
        fn.set_temp_reg(this, m_reg);
    }
    
    std::string to_string() const override {
        return m_name;
    }
};

}
