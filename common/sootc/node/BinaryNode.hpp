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
    enum class Op { ADD, SUB, MUL, DIV, MOD };
    
private:
    Op m_op;
    std::unique_ptr<ExpressionNode> m_left;
    std::unique_ptr<ExpressionNode> m_right;
    
public:
    BinaryNode(Op op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)
        : ExpressionNode(NodeType::BinaryNode),  m_op(op), m_left(std::move(left)), m_right(std::move(right)) 
    {
        if (m_left && m_left->get_type()) {
            m_type = m_left->get_type();
        }
    }
    
    void emit(FunctionNode &fn) override {
        m_left->emit(fn);
        m_right->emit(fn);

        u8 left_reg = fn.get_temp_reg(m_left.get());
        u8 right_reg = fn.get_temp_reg(m_right.get());

        // ---- Determine whether this is a float or int operation ----
        //
        // The result type follows the FIRST operand's type — mirrors the
        // principle of least surprise and matches how GOAL dispatches.
        // If the first operand is unknown, fall back to int.
        Type      *left_type = m_left->get_type();
        const bool is_float = (left_type && left_type->get_name() == "float");

        // ---- Pick the opcode ----
        Opcode opcode;
        switch (m_op) {
        case Op::ADD: opcode = is_float ? Opcode::FAdd : Opcode::IAdd; break;
        case Op::SUB: opcode = is_float ? Opcode::FSub : Opcode::ISub; break;
        case Op::MUL: opcode = is_float ? Opcode::FMul : Opcode::IMul; break;
        case Op::DIV: opcode = is_float ? Opcode::FDiv : Opcode::IDiv; break;
        case Op::MOD: opcode = is_float ? Opcode::FMod : Opcode::IMod; break;
        default: opcode = Opcode::IAdd; break;
        }

        // ---- Update our own type accordingly ----
        //
        // The VM casts operands to int/float on the fly (Variant::to_int
        // / Variant::to_float), so mixed int/float operands still work:
        // an int on the left of a float op is promoted automatically.
        set_type(is_float ? TypeSystem::instance().lookup_type("float")
                          : TypeSystem::instance().lookup_type("int"));

        u8 dest_reg = fn.alloc_temp_reg(m_type);
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
            case Op::MOD: op_str = "%"; break;
            }
        return "(" + m_left->to_string() + " " + op_str + " " + m_right->to_string() + ")";
    }
};


} // namespace sootc