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
    
    void emit(FunctionNode& fn) override {
        // 1. Генерируем левый и правый операнды
        m_left->emit(fn);
        m_right->emit(fn);
        
        u8 left_reg = fn.get_temp_reg(m_left.get());
        u8 right_reg = fn.get_temp_reg(m_right.get());
        
        // 2. Выделяем регистр для результата
        u8 dest_reg = fn.alloc_temp_reg(m_type);
        
        // 3. Генерируем инструкцию сравнения
        Opcode opcode;
        switch (m_op) {
            case Op::EQ: opcode = Opcode::IEqual; break;
            case Op::NE: opcode = Opcode::INotEqual; break;
            case Op::LT: opcode = Opcode::ILessThan; break;
            case Op::LE: opcode = Opcode::ILessThanEqual; break;
            case Op::GT: opcode = Opcode::IGreaterThan; break;
            case Op::GE: opcode = Opcode::IGreaterThanEqual; break;
        }
        
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