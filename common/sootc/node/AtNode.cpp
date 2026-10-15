#include "sootc/node/AtNode.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/ConstNode.hpp"

#include "carbon/vm/Instructions.hpp"
#include "type_system/Type.hpp"
#include "util/Log.hpp"

#include <stdexcept>

namespace sootc {

       void AtNode::emit(FunctionNode &fn) {
        // Compute the element's address.
        u8 addr_reg = emit_lvalue(fn);

        // If the element is a reference (structure), the address IS
        // the value. Leave it in the register.
        if (m_elem_type->is_reference()) {
            fn.set_temp_reg(this, addr_reg);
            return;
        }

        // Scalar load.
        Opcode load_op;
        if (m_elem_type->get_preferred_reg_class() == RegClass::FPR) {
            load_op = Opcode::LoadFloat;
        } else {
            switch (m_elem_type->get_load_size()) {
            case 1:
                load_op = m_elem_type->get_load_signed() ? Opcode::LoadI8 : Opcode::LoadU8;
                break;
            case 2:
                load_op = m_elem_type->get_load_signed() ? Opcode::LoadI16 : Opcode::LoadU16;
                break;
            case 4:
                load_op = m_elem_type->get_load_signed() ? Opcode::LoadI32 : Opcode::LoadU32;
                break;
            case 8:
                load_op = m_elem_type->get_load_signed() ? Opcode::LoadI64 : Opcode::LoadU64;
                break;
            default:
                throw std::runtime_error(
                    fmt::format("AtNode::emit: unsupported element load size {}",
                                m_elem_type->get_load_size()));
            }
        }

        u8 result_reg = fn.alloc_temp_reg(m_elem_type);
        fn.add_instruction(load_op, result_reg, addr_reg, 0);
        fn.set_temp_reg(this, result_reg);
    }

        u8 AtNode::emit_lvalue(FunctionNode &fn) {
        if (!m_base || !m_index || !m_elem_type) {
            throw std::runtime_error("AtNode::emit_lvalue: missing base, index, or element type");
        }

        // 1. Base pointer.
        m_base->emit(fn);
        u8 base_reg = fn.get_temp_reg(m_base.get());

        // 2. Element size.
        const int elem_size = m_elem_type->get_size_in_memory();
        if (elem_size <= 0) {
            throw std::runtime_error(
                fmt::format("AtNode::emit_lvalue: element type '{}' has invalid size {}",
                            m_elem_type->get_name(), elem_size));
        }

        // 3. Fast path: constant index.
        //
        // (-> arr N) and (at arr N) with literal N fold to a single
        // IAddImm(base, N * elem_size). No IMul, no IAdd, no runtime
        // index register. This is what GOAL does for inline arrays.
        if (auto *const_index = dynamic_cast<const ConstNode *>(m_index.get())) {
            if (const_index->is_int()) {
                const i64 idx = const_index->int_value();
                if (idx < 0) {
                    throw std::runtime_error(
                        fmt::format("AtNode::emit_lvalue: negative index {}", idx));
                }
                const u64 offset = static_cast<u64>(idx) * elem_size;

                u8 addr_reg = fn.alloc_temp_reg(nullptr);
                if (offset <= 0xFF) {
                    fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, base_reg,
                                               static_cast<u8>(offset));
                } else {
                    u8  off_reg = fn.alloc_temp_reg(nullptr);
                    u16 st = fn.add_constant(offset, FunctionNode::ConstKind::INT);
                    fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, off_reg, st);
                    fn.add_instruction(Opcode::IAdd, addr_reg, base_reg, off_reg);
                }
                fn.set_temp_reg(this, addr_reg);
                return addr_reg;
            }
        }

        // 4. Runtime index.
        m_index->emit(fn);
        u8 index_reg = fn.get_temp_reg(m_index.get());

        u8 offset_reg = fn.alloc_temp_reg(nullptr);
        if (elem_size <= 0xFF) {
            fn.add_instruction_imm_reg(Opcode::IMulImm, offset_reg, index_reg,
                                       static_cast<u8>(elem_size));
        } else {
            u8  size_reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(static_cast<u64>(elem_size), FunctionNode::ConstKind::INT);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, size_reg, st);
            fn.add_instruction(Opcode::IMul, offset_reg, index_reg, size_reg);
        }

        u8 addr_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction(Opcode::IAdd, addr_reg, base_reg, offset_reg);

        fn.set_temp_reg(this, addr_reg);
        return addr_reg;
    }
} // namespace sootc