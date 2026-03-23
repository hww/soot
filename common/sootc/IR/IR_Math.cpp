#include "common/sootc/IR/IR_Node.hpp"

namespace sootc {

// ============================================================================
// IR_Binary Implementation
// ============================================================================

IR_Binary::IR_Binary(Op op, IR_Reg* dest, IR_Value* left, IR_Value* right)
    : op_(op), dest_(dest), left_(left), right_(right) {}

std::string IR_Binary::to_string() const {
    const char* op_str = "";
    switch (op_) {
        case Op::ADD: op_str = "add"; break;
        case Op::SUB: op_str = "sub"; break;
        case Op::MUL: op_str = "mul"; break;
        case Op::DIV: op_str = "div"; break;
        case Op::MOD: op_str = "mod"; break;
        case Op::AND: op_str = "and"; break;
        case Op::OR:  op_str = "or"; break;
        case Op::XOR: op_str = "xor"; break;
    }
    return fmt::format("{} {}, {}, {}", op_str, dest_->to_string(), 
                       left_->to_string(), right_->to_string());
}

void IR_Binary::generate(FunctionDescBuilder& builder,
                         const std::unordered_map<IR_Value*, u32>& reg_map) {
    u32 dest_reg = reg_map.at(dest_);
    u32 left_reg = reg_map.at(left_);
    u32 right_reg = reg_map.at(right_);
    
    Opcode opcode;
    switch (op_) {
        case Op::ADD: opcode = Opcode::ADD_INT; break;
        case Op::SUB: opcode = Opcode::SUB_INT; break;
        case Op::MUL: opcode = Opcode::MUL_INT; break;
        case Op::DIV: opcode = Opcode::DIV_INT; break;
        case Op::MOD: opcode = Opcode::MOD_INT; break;
        case Op::AND: opcode = Opcode::BIT_AND; break;
        case Op::OR:  opcode = Opcode::BIT_OR; break;
        case Op::XOR: opcode = Opcode::BIT_XOR; break;
        default: opcode = Opcode::ADD_INT; break;
    }
    
    builder.add_instruction(opcode, dest_reg, left_reg, right_reg);
}

// ============================================================================
// IR_Compare Implementation
// ============================================================================

IR_Compare::IR_Compare(Cond cond, IR_Reg* dest, IR_Value* left, IR_Value* right)
    : cond_(cond), dest_(dest), left_(left), right_(right) {}

std::string IR_Compare::to_string() const {
    const char* cond_str = "";
    switch (cond_) {
        case Cond::EQ: cond_str = "eq"; break;
        case Cond::NE: cond_str = "ne"; break;
        case Cond::LT: cond_str = "lt"; break;
        case Cond::LE: cond_str = "le"; break;
        case Cond::GT: cond_str = "gt"; break;
        case Cond::GE: cond_str = "ge"; break;
    }
    return fmt::format("cmp {} {}, {}, {}", cond_str, dest_->to_string(),
                       left_->to_string(), right_->to_string());
}

void IR_Compare::generate(FunctionDescBuilder& builder,
                          const std::unordered_map<IR_Value*, u32>& reg_map) {
    u32 dest_reg = reg_map.at(dest_);
    u32 left_reg = reg_map.at(left_);
    u32 right_reg = reg_map.at(right_);
    
    Opcode opcode;
    switch (cond_) {
        case Cond::EQ: opcode = Opcode::CMP_EQUAL; break;
        case Cond::NE: opcode = Opcode::CMP_NOT_EQUAL; break;
        case Cond::LT: opcode = Opcode::CMP_LT; break;
        case Cond::LE: opcode = Opcode::CMP_LT_EQUAL; break;
        case Cond::GT: opcode = Opcode::CMP_GT; break;
        case Cond::GE: opcode = Opcode::CMP_GT_EQUAL; break;
        default: opcode = Opcode::CMP_EQUAL; break;
    }
    
    builder.add_instruction(opcode, dest_reg, left_reg, right_reg);
}

} // namespace sootc