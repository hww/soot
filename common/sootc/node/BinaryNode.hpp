// ExpressionNode.hpp
#pragma once

#include "CommonTypes.hpp"
#include "Node.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"

namespace sootc {

class FunctionNode;

// ========================================================================
// Бинарная операция
// ========================================================================


class BinaryNode : public ExpressionNode {
public:
    enum class Op { ADD, SUB, MUL, DIV };
    
private:
    Op m_op;
    std::unique_ptr<ExpressionNode> m_left;
    std::unique_ptr<ExpressionNode> m_right;
    
public:
    BinaryNode(Op op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)
        : m_op(op), m_left(std::move(left)), m_right(std::move(right)) 
    {
        if (m_left && m_left->get_type()) {
            m_type = m_left->get_type();
        }
    }
    
    void emit(FunctionNode& fn) override {
        m_left->emit(fn);
        m_right->emit(fn);
        
        u8 left_reg = fn.get_temp_reg(m_left.get());
        u8 right_reg = fn.get_temp_reg(m_right.get());
        u8 dest_reg = fn.alloc_temp_reg(m_type);
        
        Opcode opcode;
        switch (m_op) {
            case Op::ADD: opcode = Opcode::IAdd; break;
            case Op::SUB: opcode = Opcode::ISub; break;
            case Op::MUL: opcode = Opcode::IMul; break;
            case Op::DIV: opcode = Opcode::IDiv; break;
        }
        
        fn.add_instruction(opcode, dest_reg, left_reg, right_reg);
        fn.set_temp_reg(this, dest_reg);
    }
    
    std::string to_string() const override {
        std::string op_str;
        switch (m_op) {
            case Op::ADD: op_str = "+"; break;
            case Op::SUB: op_str = "-"; break;
            case Op::MUL: op_str = "*"; break;
            case Op::DIV: op_str = "/"; break;
        }
        return "(" + m_left->to_string() + " " + op_str + " " + m_right->to_string() + ")";
    }
};


} // namespace sootc