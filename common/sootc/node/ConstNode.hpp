// ExpressionNode.hpp
#pragma once

#include "CommonTypes.hpp"
#include "Node.hpp"
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/sootc/libs/StringsTable.hpp"
#include <stdexcept>

namespace sootc {

class FunctionNode;

class ConstNode : public ExpressionNode {

    enum class EType { UNDEFINED, INT, FLOAT, STRING };

    i64 m_int_value;
    f64 m_float_value;
    std::string m_string_value;

    EType m_etype = EType::UNDEFINED;

    ConstNode(i64 val)
        : ExpressionNode(NodeType::ConstNode), m_int_value(val), m_etype(EType::INT) {
        m_type = TypeSystem::instance().lookup_type("int");
    }

    ConstNode(f64 val)
        : ExpressionNode(NodeType::ConstNode), m_float_value(val), m_etype(EType::FLOAT) {
        m_type = TypeSystem::instance().lookup_type("float");
    }

    ConstNode(const std::string &str)
        : ExpressionNode(NodeType::ConstNode), m_etype(EType::STRING) {
        m_string_value = str;
        m_type = TypeSystem::instance().lookup_type("string");
    }
    
public:
    ConstNode() : ExpressionNode(NodeType::ConstNode) {} 

    static std::unique_ptr<ConstNode> make_int(i64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_float(f64 val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    static std::unique_ptr<ConstNode> make_string(const std::string& val) {
        return std::unique_ptr<ConstNode>(new ConstNode(val));
    }
    
    // ---- Value accessors ----
    [[nodiscard]] bool is_int() const noexcept { return m_etype == EType::INT; }
    [[nodiscard]] bool is_float() const noexcept { return m_etype == EType::FLOAT; }
    [[nodiscard]] bool is_string() const noexcept { return m_etype == EType::STRING; }

    [[nodiscard]] i64 int_value() const noexcept {
        // If the literal was an integer, m_float_value is uninitialized.
        // Fall back to converting m_int_value so callers always get a
        // sensible number.
        switch (m_etype) {
        case sootc::ConstNode::EType::INT: return m_int_value;
        case sootc::ConstNode::EType::FLOAT: return static_cast<i64>(m_float_value);
        default: throw std ::runtime_error("unexpected");
        }
    }
    [[nodiscard]] f64 float_value() const noexcept {
        // If the literal was an integer, m_float_value is uninitialized.
        // Fall back to converting m_int_value so callers always get a
        // sensible number.
        switch (m_etype) {
        case sootc::ConstNode::EType::FLOAT: return m_float_value;
        case sootc::ConstNode::EType::INT: return static_cast<f64>(m_int_value); 
        default: throw std ::runtime_error("unexpected");
        }
    }
    [[nodiscard]] const std::string &string_value() const noexcept { return m_string_value; }

    void emit(FunctionNode& fn) override {
        if (m_etype == sootc::ConstNode::EType::STRING) {
            // Кладём в m_constants НЕ указатель на c_str(), а индекс строки
            // в глобальной string table. При финальной сборке бинарника
            // (FileNode::make_binary) этот индекс превратится в относительное
            // смещение в секции строк.
            if (!fn.global_state()) {
                throw std::runtime_error(
                    "ConstNode::emit: FunctionNode has no GlobalState; "
                    "call FunctionNode::set_global_state() before emit_body().");
            }
            u32 str_offset = fn.global_state()->lookup_or_add(m_string_value);
            u16 idx =
                fn.add_constant(static_cast<u64>(str_offset), FunctionNode::ConstKind::STRING);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticPointer, reg, idx);
            fn.set_temp_reg(this, reg);
        } else if (m_etype == sootc::ConstNode::EType::FLOAT) {
            u16 idx = fn.add_constant(*reinterpret_cast<u64*>(&m_float_value), 
                                       FunctionNode::ConstKind::FLOAT);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticFloatImm, reg, idx);
            fn.set_temp_reg(this, reg);
        } else if (m_etype == sootc::ConstNode::EType::INT) {
            u16 idx = fn.add_constant(static_cast<u64>(m_int_value), 
                                       FunctionNode::ConstKind::INT);
            u8 reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, idx);
            fn.set_temp_reg(this, reg);
        }
    }
    
    std::string to_string() const override {
        if (is_float()) return std::to_string(m_float_value);
        if (is_int()) return std::to_string(m_int_value);
        if (is_string()) return m_string_value;
        return "UNDEFINED";
    }
};



} // namespace sootc