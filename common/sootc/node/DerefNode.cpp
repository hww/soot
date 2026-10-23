#include "DerefNode.hpp"
#include "FunctionNode.hpp"

#include "carbon/vm/Instructions.hpp"
#include "util/Log.hpp"

namespace sootc {

    DerefNode::DerefNode(std::unique_ptr<ExpressionNode> expr, std::string field, u32 offset,
                         Type *field_type)
        : ExpressionNode(field_type), m_expr(std::move(expr)), m_field(std::move(field)),
          m_offset(offset) {
        adopt(m_expr);
    }

    DerefNode::DerefNode(std::unique_ptr<ExpressionNode> expr, std::string field, u32 offset,
                         Type *field_type,
                         bool is_inline, bool is_array)
        : ExpressionNode(field_type), m_expr(std::move(expr)), m_field(std::move(field)),
          m_offset(offset), m_is_inline(is_inline), m_is_array(is_array) {
        adopt(m_expr);
    }

    std::string DerefNode::to_string() const {
        return "(-> " + m_expr->to_string() + " " + m_field + ")";
    }

    /// @brief Emit code to read the field at m_offset from the pointer value
    ///        produced by m_expr.
    /// @details 1. Emit m_expr, obtaining a pointer in some register.
    ///          2. Add m_offset with intAddImm, producing an address register.
    ///          3. Load the field using the appropriate load opcode for the
    ///             field's type.
    ///          The result is written to a fresh temp register assigned to this.
    void DerefNode::emit(FunctionNode &fn) {
        m_expr->emit(fn);
        u8 base_reg = fn.get_temp_reg(m_expr.get());

        u8 addr_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, base_reg,
                                   static_cast<u8>(m_offset & 0xFF));

        // Classify the field's TYPE.
        auto *value_type = dynamic_cast<ValueType *>(m_type);
        auto *ref_type = dynamic_cast<ReferenceType *>(m_type);

        if (value_type) {
            // Scalar. Load always. m_is_inline is irrelevant.
            u8     dest_reg = fn.alloc_temp_reg(m_type);
            Opcode load_op;

            if (m_type->get_preferred_reg_class() == RegClass::FPR) {
                load_op = Opcode::LoadFloat;
            } else {
                const int  size = m_type->get_load_size();
                const bool sign = m_type->get_load_signed();
                switch (size) {
                case 1: load_op = sign ? Opcode::LoadI8 : Opcode::LoadU8; break;
                case 2: load_op = sign ? Opcode::LoadI16 : Opcode::LoadU16; break;
                case 4: load_op = sign ? Opcode::LoadI32 : Opcode::LoadU32; break;
                case 8: load_op = sign ? Opcode::LoadI64 : Opcode::LoadU64; break;
                default:
                    throw std::runtime_error(
                        fmt::format("DerefNode: unsupported field load size {}", size));
                }
            }
            fn.add_instruction(load_op, dest_reg, addr_reg, 0);
            fn.set_temp_reg(this, dest_reg);
            return;
        }

        if (ref_type) {
            if (m_is_inline) {
                // Inline structure/array: return address.
                fn.set_temp_reg(this, addr_reg);
                return;
            }
            // Pointer: read the pointer.
            u8 dest_reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction(Opcode::LoadPointer, dest_reg, addr_reg, 0);
            fn.set_temp_reg(this, dest_reg);
            return;
        }

        throw std::runtime_error(
            fmt::format("DerefNode: field type '{}' is neither ValueType nor ReferenceType",
                        m_type ? m_type->get_name() : "null"));
    }

    /// @brief Emit code that computes the field's address (an lvalue).
    /// @details Same as emit(), but without the load. The resulting address
    ///          register is assigned to this node so SetNode can use it.
    u8 DerefNode::emit_lvalue(FunctionNode &fn) {
        // 1. Evaluate the base pointer expression.
        m_expr->emit(fn);
        u8 base_reg = fn.get_temp_reg(m_expr.get());

        // 2. Compute the field address: r_addr = base_reg + offset.
        //
        // Same reasoning as in DerefNode::emit — use the three-operand
        // form so the base register actually reaches the VM.
        u8 addr_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, base_reg,
                                   static_cast<u8>(m_offset & 0xFF));

        fn.set_temp_reg(this, addr_reg);
        return addr_reg;
    }
} // namespace sootc