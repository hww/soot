// ExpressionNode.hpp
#pragma once

#include "CommonTypes.hpp"
#include "Node.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"
#include "common/type_system/TypeSystem.hpp"

namespace sootc {

class FunctionNode;

class ConstNode : public ExpressionNode {
    i64 m_int_value;
    f64 m_float_value;
    std::string m_string_value;

    bool m_is_float;
    bool m_is_string = false;

    ConstNode(i64 val) : m_int_value(val), m_is_float(false) {
        m_type = TypeSystem::instance().lookup_type("int");
    }
    
    ConstNode(f64 val) : m_float_value(val), m_is_float(true) {
        m_type = TypeSystem::instance().lookup_type("float");
    }
    
    ConstNode(const std::string& str) : m_is_float(false) {
        (void)str;
        m_type = TypeSystem::instance().lookup_type("string");
    }
    
public:
    static std::unique_ptr<ConstNode> make_int(i64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_float(f64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_string(const std::string& val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    void emit(FunctionNode& fn) override {
        if (m_is_string) {
            // Для строк - особый тип константы
            u16 idx = fn.add_constant(reinterpret_cast<u64>(m_string_value.c_str()), 
                                       FunctionNode::ConstKind::STRING);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticPointer, reg, idx);
            fn.set_temp_reg(this, reg);
        } else if (m_is_float) {
            u16 idx = fn.add_constant(*reinterpret_cast<u64*>(&m_float_value), 
                                       FunctionNode::ConstKind::FLOAT);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticFloatImm, reg, idx);
            fn.set_temp_reg(this, reg);
        } else {
            u16 idx = fn.add_constant(static_cast<u64>(m_int_value), 
                                       FunctionNode::ConstKind::INT);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, idx);
            fn.set_temp_reg(this, reg);
        }
    }
    
    std::string to_string() const override {
        if (m_is_float) return std::to_string(m_float_value);
        return std::to_string(m_int_value);
    }
};



} // namespace sootc