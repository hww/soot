#include "StoreGlobalNode.hpp"
#include "FunctionNode.hpp"
#include "NewNode.hpp"
#include "vm/Instructions.hpp"

namespace sootc {

    void StoreGlobalNode::emit(FunctionNode &fn) {
        if (!m_new_node) { return; }

        // 1. Build the object. NewNode::emit for a heap allocation
        //    emits the constructor call and leaves a pointer in a
        //    temp register assigned to m_new_node.
        m_new_node->emit(fn);
        u8 value_reg = fn.get_temp_reg(m_new_node.get());

        // 2. Resolve the named slot via LookupPointer. The name is a
        //    placeholder entry registered at module-load time, so
        //    Globals has it and LookupPointer returns the slot address
        //    inside the module's bytes.
        const u16 sid_idx =
            fn.add_constant(static_cast<u64>(StringId(m_name).value), FunctionNode::ConstKind::SID);

        u8 slot_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, slot_reg, sid_idx);

        // 3. Write the value into the slot.
        //    StorePointer convention: a = address, b = value.
        fn.add_instruction(Opcode::StorePointer, slot_reg, value_reg, 0);

        fn.set_temp_reg(this, value_reg);
        m_type = m_new_node->get_type();
    }

} // namespace sootc