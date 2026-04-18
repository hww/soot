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
    ExpressionNode* m_left;
    ExpressionNode* m_right;
    
public:
    BinaryNode(Op op, ExpressionNode* left, ExpressionNode* right)
        : m_op(op), m_left(left), m_right(right) {}
    
    void emit(FunctionNode& fn) override {
        m_left->emit(fn);
        m_right->emit(fn);
        
        u8 left_reg = fn.get_reg(m_left);
        u8 right_reg = fn.get_reg(m_right);
        u8 dest_reg = fn.alloc_reg(nullptr);
        
        Opcode opcode;
        switch (m_op) {
            case Op::ADD: opcode = Opcode::IAdd; break;
            case Op::SUB: opcode = Opcode::ISub; break;
            case Op::MUL: opcode = Opcode::IMul; break;
            case Op::DIV: opcode = Opcode::IDiv; break;
        }
        
        fn.add_instruction(opcode, dest_reg, left_reg, right_reg);
        fn.set_reg(this, dest_reg);
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