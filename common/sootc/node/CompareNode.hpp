#pragma once

#include "ExpressionNode.hpp"
#include "type_system/TypeSystem.hpp"
#include <memory>

namespace sootc {


class CompareNode : public ExpressionNode {
public:
    enum class Op { EQ, NE, LT, LE, GT, GE };
    
private:
    Op m_op;
    std::unique_ptr<ExpressionNode> m_left;
    std::unique_ptr<ExpressionNode> m_right;
    
public:
    CompareNode(Op op, 
                std::unique_ptr<ExpressionNode> left, 
                std::unique_ptr<ExpressionNode> right)
        : ExpressionNode(NodeType::CompareNode) 
        , m_op(op)
        , m_left(std::move(left))
        , m_right(std::move(right)) 
    {
        // Результат сравнения - булево значение (int)
        m_type = TypeSystem::instance().lookup_type("int");
    }
    
    void emit(FunctionNode &fn) override {
        // 1. Emit both operands.
        m_left->emit(fn);
        m_right->emit(fn);

        u8 left_reg = fn.get_temp_reg(m_left.get());
        u8 right_reg = fn.get_temp_reg(m_right.get());

        // 2. Decide whether this is a float or int comparison.
        //
        //    Like BinaryNode, we look at the FIRST operand's type.
        //    The VM will coerce the second operand automatically via
        //    Variant::to_int / to_float.
        Type      *left_type = m_left->get_type();
        const bool is_float = (left_type && left_type->get_name() == "float");

        // 3. Pick the opcode.
        Opcode opcode;
        switch (m_op) {
        case Op::EQ: opcode = is_float ? Opcode::FEqual : Opcode::IEqual; break;
        case Op::NE: opcode = is_float ? Opcode::FNotEqual : Opcode::INotEqual; break;
        case Op::LT: opcode = is_float ? Opcode::FLessThan : Opcode::ILessThan; break;
        case Op::LE: opcode = is_float ? Opcode::FLessThanEqual : Opcode::ILessThanEqual; break;
        case Op::GT: opcode = is_float ? Opcode::FGreaterThan : Opcode::IGreaterThan; break;
        case Op::GE:
            opcode = is_float ? Opcode::FGreaterThanEqual : Opcode::IGreaterThanEqual;
            break;
        default: opcode = Opcode::IEqual; break;
        }

        // 4. Result of a comparison is always int (boolean).
        m_type = TypeSystem::instance().lookup_type("int");

        u8 dest_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction(opcode, dest_reg, left_reg, right_reg);
        fn.set_temp_reg(this, dest_reg);
    }
    
    std::string to_string() const override {
        std::string op_str;
        switch (m_op) {
            case Op::EQ: op_str = "=="; break;
            case Op::NE: op_str = "!="; break;
            case Op::LT: op_str = "<"; break;
            case Op::LE: op_str = "<="; break;
            case Op::GT: op_str = ">"; break;
            case Op::GE: op_str = ">="; break;
        }
        return "(" + m_left->to_string() + " " + op_str + " " + m_right->to_string() + ")";
    }
};

} // namespace sootc