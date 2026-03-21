// common/sootc/src/IR/IR_Move.cpp
#include "common/sootc/IR/IR_Node.hpp"

namespace sootc {

// IR_Move
IR_Move::IR_Move(IR_Reg *dest, IR_Value *src) : dest_(dest), src_(src) {}

std::string IR_Move::to_string() const {
    return "mov " + dest_->to_string() + ", " + src_->to_string();
}

void IR_Move::generate(ByteCodeBuilder                           &builder,
                       const std::unordered_map<IR_Value *, u32> &reg_map) {
    u32 dest_reg = reg_map.at(dest_);
    u32 src_reg = reg_map.at(src_);
    builder.add_instruction(Opcode::MOVE, dest_reg, src_reg, 0);
}

std::vector<IR_Value *> IR_Move::get_used_values() const {
    return {dest_, src_};
}

// IR_Return
IR_Return::IR_Return(IR_Value *value) : value_(value) {}

std::string IR_Return::to_string() const {
    if (value_) {
        return "ret " + value_->to_string();
    }
    return "ret";
}

void IR_Return::generate(ByteCodeBuilder                           &builder,
                         const std::unordered_map<IR_Value *, u32> &reg_map) {
    if (value_) {
        u32 val_reg = reg_map.at(value_);
        builder.add_instruction(Opcode::RETURN, val_reg, 0, 0);
    } else {
        builder.add_instruction(Opcode::RETURN, 0, 0, 0);
    }
}

// Добавить в конец файла IR_Move.cpp

// ============================================================================
// IR_LoadConst Implementation
// ============================================================================

IR_LoadConst::IR_LoadConst(IR_Reg* dest, IR_Const* value)
    : dest_(dest), value_(value) {}

std::string IR_LoadConst::to_string() const {
    return fmt::format("load {}, {}", dest_->to_string(), value_->to_string());
}

void IR_LoadConst::generate(ByteCodeBuilder& builder,
                            const std::unordered_map<IR_Value*, u32>& reg_map) {
    u32 dest_reg = reg_map.at(dest_);
    
    if (value_->is_float()) {
        // TODO: добавить поддержку float констант
        builder.add_instruction(Opcode::LOAD_IMMEDIATE_INT, dest_reg, 0, 0);
    } else {
        builder.add_instruction(Opcode::LOAD_IMMEDIATE_INT, dest_reg, 
                                static_cast<u16>(value_->get_int()), 0);
    }
}

} // namespace sootc