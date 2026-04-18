// ExpressionNode.hpp
#pragma once

#include "CommonTypes.hpp"
#include "Node.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"

namespace sootc {

class FunctionNode;

class ConstNode : public ExpressionNode {
    u64 m_value;
    FunctionNode::ConstKind m_kind;
    
public:
    explicit ConstNode(i64 value) 
        : m_value(static_cast<u64>(value)), m_kind(FunctionNode::ConstKind::INT) {}
    
    explicit ConstNode(f64 value) {
        // Конвертируем double в u64 битовым представлением
        memcpy(&m_value, &value, sizeof(double));
        m_kind = FunctionNode::ConstKind::FLOAT;
    }
    
    explicit ConstNode(const std::string& str) {
        // Строка хранится как указатель на строку в static segment
        // Пока заглушка
        m_value = 0;
        m_kind = FunctionNode::ConstKind::STRING;
    }
    
    void emit(FunctionNode& fn) override {
        u16 idx = fn.add_constant(m_value, m_kind);
        u8 reg = fn.alloc_reg(nullptr);  // TODO: type
        fn.add_instruction_imm_u16(Opcode::LoadStaticI32Imm, reg, idx);
        fn.set_reg(this, reg);
    }
    
    std::string to_string() const override {
        if (m_kind == FunctionNode::ConstKind::INT) {
            return std::to_string(static_cast<i64>(m_value));
        }
        return "const";
    }

    static std::unique_ptr<ConstNode> make_int(i64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_float(f64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_string(const std::string& val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }    
};



} // namespace sootc