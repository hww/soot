#include "sootc/node/UnaryNode.hpp"
#include "carbon/vm/Instructions.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "type_system/TypeSystem.hpp"

namespace sootc {

    void UnaryNode::emit(FunctionNode &fn) {
        m_operand->emit(fn);

        u8                src_reg = fn.get_temp_reg(m_operand.get());
        Type             *src_type = m_operand->get_type();
        const std::string src_name = src_type ? src_type->get_name() : "int";
        const bool        is_float = (src_name == "float");

        // Result type follows the operand (except for NOT/BITNOT — always int).
        if (m_op == Op::NOT || m_op == Op::BITNOT) {
            m_type = TypeSystem::instance().lookup_type("int");
        } else {
            m_type = src_type;
        }

        u8 dst_reg = fn.alloc_temp_reg(m_type);

        Opcode opcode;
        switch (m_op) {
        case Op::ABS: opcode = is_float ? Opcode::FAbs : Opcode::IAbs; break;
        case Op::NEG: opcode = is_float ? Opcode::FNeg : Opcode::INeg; break;
        case Op::NOT: opcode = Opcode::OpLogNot; break;
        case Op::BITNOT: opcode = Opcode::OpBitNot; break;
        default: opcode = Opcode::INeg; break;
        }

        // All unary ops here use the two-register form (dest, src).
        fn.add_instruction(opcode, dst_reg, src_reg, 0);
        fn.set_temp_reg(this, dst_reg);
    }

} // namespace sootc