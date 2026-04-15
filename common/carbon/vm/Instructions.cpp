#include "common/carbon/vm/Instructions.hpp"


namespace carbon::vm {

    std::string Instruction::inspect() const {
        auto* info = InstructionTable::instance().get_info(opcode);
        std::string op_name = info ? info->name.to_cstring() : "???";
        
        if (has_immediate()) {
            return fmt::format("{:08X}: {:<8} r{}, #{}", 
                as_u32,
                op_name,
                a,
                imm16);
        } else {
            // Специальные случаи для некоторых инструкций
            if (opcode == Opcode::RETURN) {
                return fmt::format("{:08X}: {:<8} r{}", 
                    as_u32,
                    op_name,
                    a);
            }
            if (opcode == Opcode::BRANCH || opcode == Opcode::BRANCH_IF || opcode == Opcode::BRANCH_IF_NOT) {
                return fmt::format("{:08X}: {:<8} L{}", 
                    as_u32,
                    op_name,
                    imm16);
            }
            if (opcode == Opcode::MOVE) {
                return fmt::format("{:08X}: {:<8} r{}, r{}", 
                    as_u32,
                    op_name,
                    a,
                    b);
            }
            
            return fmt::format("{:08X}: {:<8} r{}, r{}, r{}", 
                as_u32,
                op_name,
                a,
                b,
                c);
        }
    }

    std::string InstructionTable::disassemble(const Instruction inst) const {
        auto* info = get_info(inst.opcode);

        if (!info) {
            return std::format("??? ({:02x})", static_cast<u32>(inst.opcode));
        }

        std::string mnemonic = info->name.to_string();

        // Helper for register names
        auto reg = [&](u8 r) -> std::string {
            return std::format("r{}", r);
        };

        // Handle based on instruction type
        switch (inst.opcode) {
            // Control flow with immediates (branch targets)
            case Opcode::BRANCH:
            case Opcode::BRANCH_IF:
            case Opcode::BRANCH_IF_NOT:
                return std::format("{} {:+d}", mnemonic, inst.imm16);

            // Move and simple operations
            case Opcode::MOVE:
                return std::format("{} {}, {}", mnemonic, reg(inst.a), reg(inst.b));

            // Return (no operands or one operand)
            case Opcode::RETURN:
                return std::format("{} {}", mnemonic, reg(inst.a));

            // Call instructions
            case Opcode::CALL:
            case Opcode::CALL_NATIVE:
                return std::format("{} {}, {}, {}", mnemonic, reg(inst.a), reg(inst.b), reg(inst.c));

            // Unary operations
            case Opcode::ABS_INT:
            case Opcode::NEG_INT:
            case Opcode::ABS_FLOAT:
            case Opcode::NEG_FLOAT:
            case Opcode::LOG_NOT:
            case Opcode::BIT_NOT:
            case Opcode::TO_INT:
            case Opcode::TO_FLOAT:
                return std::format("{} {}, {}", mnemonic, reg(inst.a), reg(inst.b));

            // Binary operations (3 operands)
            case Opcode::ADD_INT:
            case Opcode::SUB_INT:
            case Opcode::MUL_INT:
            case Opcode::DIV_INT:
            case Opcode::MOD_INT:
            case Opcode::ASH_INT:
            case Opcode::ADD_FLOAT:
            case Opcode::SUB_FLOAT:
            case Opcode::MUL_FLOAT:
            case Opcode::DIV_FLOAT:
            case Opcode::MOD_FLOAT:
            case Opcode::LOG_AND:
            case Opcode::LOG_OR:
            case Opcode::BIT_AND:
            case Opcode::BIT_OR:
            case Opcode::BIT_XOR:
            case Opcode::BIT_NOR:
                return std::format("{} {}, {}, {}", mnemonic, reg(inst.a), reg(inst.b), reg(inst.c));

            // Immediate operations
            case Opcode::LOAD_IMMEDIATE_INT:
            case Opcode::LOAD_IMMEDIATE_FLOAT:
                return std::format("{} {}, imm({})", mnemonic, reg(inst.a_imm), inst.imm16);

            case Opcode::ADD_IMM:
            case Opcode::SUB_IMM:
            case Opcode::MUL_IMM:
            case Opcode::DIV_IMM:
                return std::format("{} {}, {}, {}", mnemonic, reg(inst.a_imm), reg(inst.b), inst.imm16);

            // Static loads
            case Opcode::LOAD_STATIC_INT:
            case Opcode::LOAD_STATIC_FLOAT:
            case Opcode::LOAD_STATIC_POINTER:
                return std::format("{} {}, data[{}]", mnemonic, reg(inst.a), inst.imm16);

            // Lookup operations
            case Opcode::LOOKUP_INT:
            case Opcode::LOOKUP_FLOAT:
            case Opcode::LOOKUP_POINTER:
                return std::format("{} {}, lookup(data[{}])", mnemonic, reg(inst.a), inst.imm16);

            // Indirect loads/stores
            case Opcode::LOAD_IND_INT:
            case Opcode::LOAD_IND_FLOAT:
            case Opcode::LOAD_IND_POINTER:
            case Opcode::STORE_IND_INT:
            case Opcode::STORE_IND_FLOAT:
            case Opcode::STORE_IND_POINTER:
                return std::format("{} {}, {}", mnemonic, reg(inst.a), reg(inst.b));

            // Comparison operations
            case Opcode::CMP_EQUAL:
            case Opcode::CMP_NOT_EQUAL:
            case Opcode::CMP_GT:
            case Opcode::CMP_GT_EQUAL:
            case Opcode::CMP_LT:
            case Opcode::CMP_LT_EQUAL:
            case Opcode::CMP_FLOAT_EQUAL:
            case Opcode::CMP_FLOAT_NOT_EQUAL:
            case Opcode::CMP_FLOAT_GT:
            case Opcode::CMP_FLOAT_GT_EQUAL:
            case Opcode::CMP_FLOAT_LT:
            case Opcode::CMP_FLOAT_LT_EQUAL:
                return std::format("{} {}, {}, {}", mnemonic, reg(inst.a), reg(inst.b), reg(inst.c));

            // Special cases
            case Opcode::GET_SID_STRING:
                return std::format("{} {}, {}", mnemonic, reg(inst.a), reg(inst.b));

            case Opcode::LOAD_ARGC:
                return std::format("{} {}", mnemonic, reg(inst.a));

            default:
                // Fallback for unknown instructions
                if (inst.has_immediate()) {
                    return std::format("{} {}, {}", mnemonic, reg(inst.a_imm), inst.imm16);
                } else if (info->operand_count == 2) {
                    return std::format("{} {}, {}", mnemonic, reg(inst.a), reg( inst.b));
                } else if (info->operand_count == 3) {
                    return std::format("{} {}, {}, {}", mnemonic, reg(inst.a), reg(inst.b), reg(inst.c));
                }
                return mnemonic;
        }
    }
} // namespace vm