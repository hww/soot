#include "sootc/node/CastNode.hpp"
#include "carbon/vm/Instructions.hpp"
#include "sootc/node/FunctionNode.hpp"

namespace sootc {

    void CastNode::emit(FunctionNode &fn) {
        // 1. Emit the inner value.
        m_value->emit(fn);

        // 2. Read its source register and type.
        u8    src_reg = fn.get_temp_reg(m_value.get());
        Type *src_type = m_value->get_type();

        // 3. Allocate a destination register.
        u8 dst_reg = fn.alloc_temp_reg(m_type);

        // 4. Choose the conversion instruction.
        const std::string src_name = src_type ? src_type->get_name() : "object";
        const std::string dst_name = m_type ? m_type->get_name() : "object";

        if (src_name == dst_name) {
            fn.add_instruction(Opcode::Move, dst_reg, src_reg, 0);
        } else if (dst_name == "float") {
            fn.add_instruction(Opcode::CastFloat, dst_reg, src_reg, 0);
        } else if (dst_name == "int") {
            fn.add_instruction(Opcode::CastInteger, dst_reg, src_reg, 0);
        } else {
            fn.add_instruction(Opcode::Move, dst_reg, src_reg, 0);
        }

        fn.set_temp_reg(this, dst_reg);
    }

} // namespace sootc