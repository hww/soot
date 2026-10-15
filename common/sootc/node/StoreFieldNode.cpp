#include "sootc/node/StoreFieldNode.hpp"
#include "sootc/node/FunctionNode.hpp"

#include "carbon/lib/StringId.hpp"
#include "carbon/vm/Instructions.hpp"
#include "util/Log.hpp"

#include <stdexcept>

namespace sootc {

    void StoreFieldNode::emit(FunctionNode &fn) {
        if (!m_value) {
            throw std::runtime_error("StoreFieldNode::emit: no value (owner '" + m_owner_name +
                                     "')");
        }

        // 1. Address of the owner's payload.
        //
        //    LookupPointer returns the address of the owner's slot
        //    inside its module's bytes. For a static declaration that
        //    IS the address of the payload (where the fields live).
        //
        //    If the owner is heap-declared (a later case), LookupPointer
        //    returns the address of the 8-byte slot that holds the heap
        //    pointer; a single LoadPointer would then be needed to get
        //    the payload. For now static owners are the target; the
        //    distinction is handled by the owner's own declaration, not
        //    by this node.
        const u16 owner_sid_idx = fn.add_constant(static_cast<u64>(StringId(m_owner_name).value),
                                                  FunctionNode::ConstKind::SID);

        u8 owner_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, owner_reg, owner_sid_idx);

        // 2. Compute the field address: owner + field_offset.
        u8 addr_reg = fn.alloc_temp_reg(nullptr);
        if (m_field_offset <= 0xFF) {
            fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, owner_reg,
                                       static_cast<u8>(m_field_offset));
        } else {
            u8  off_reg = fn.alloc_temp_reg(nullptr);
            u16 st =
                fn.add_constant(static_cast<u64>(m_field_offset), FunctionNode::ConstKind::INT);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, off_reg, st);
            fn.add_instruction(Opcode::IAdd, addr_reg, owner_reg, off_reg);
        }

        // 3. Evaluate the value. Any ExpressionNode works.
        m_value->emit(fn);
        u8 value_reg = fn.get_temp_reg(m_value.get());

        // 4. Store the value into the field.
        //
        //    StorePointer convention: a = address, b = value.
        fn.add_instruction(Opcode::StorePointer, addr_reg, value_reg, 0);

        // 5. The result of the whole store is the stored value.
        fn.set_temp_reg(this, value_reg);
        m_type = m_value->get_type();
    }

} // namespace sootc