#include "common/sootc/IR/IR_Node.hpp"
#include <fmt/format.h>

namespace sootc {

// --- IR_Move ---
IR_Move::IR_Move(IR_Reg *dest, IR_Value *src) : dest_(dest), src_(src) {}
std::string IR_Move::to_string() const { return fmt::format("mov {}, {}", dest_->to_string(), src_->to_string()); }
void IR_Move::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::MOVE, reg_map.at(dest_), reg_map.at(src_), 0);
}
std::vector<IR_Value*> IR_Move::get_used_values() const { 
    return { src_ }; 
}
// --- IR_LoadConst ---
IR_LoadConst::IR_LoadConst(IR_Reg *dest, IR_Const *value) : dest_(dest), value_(value) {}
std::string IR_LoadConst::to_string() const { return fmt::format("load {}, {}", dest_->to_string(), value_->to_string()); }
std::vector<IR_Value*> IR_LoadConst::get_used_values() const { 
    return { dest_ }; 
}
void IR_LoadConst::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    u32 d = reg_map.at(dest_);
    if (value_->is_float()) {
        builder.add_instruction(Opcode::LOAD_IMMEDIATE_FLOAT, d, 0, 0); // В твоем Opcode есть LOAD_IMMEDIATE_FLOAT
    } else {
        builder.add_instruction(Opcode::LOAD_IMMEDIATE_INT, d, static_cast<u16>(value_->get_int()), 0);
    }
}

// --- IR_LoadField ---
IR_LoadField::IR_LoadField(IR_Reg *dest, IR_Field *field) : dest_(dest), field_(field) {}
std::vector<IR_Value*> IR_LoadField::get_used_values() const {
    return { dest_, field_->get_base() };
}
std::string IR_LoadField::to_string() const { return fmt::format("get_field {}, {}", dest_->to_string(), field_->to_string()); }
void IR_LoadField::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::LOAD_IND_POINTER, reg_map.at(dest_), reg_map.at(field_->get_base()), static_cast<u16>(field_->get_offset()));
}

// --- IR_StoreField ---
IR_StoreField::IR_StoreField(IR_Field *field, IR_Value *value) : field_(field), value_(value) {}
std::vector<IR_Value*> IR_StoreField::get_used_values() const {
    return { field_->get_base(), value_ };
}
std::string IR_StoreField::to_string() const { return fmt::format("set_field {}, {}", field_->to_string(), value_->to_string()); }
void IR_StoreField::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::STORE_IND_POINTER, reg_map.at(field_->get_base()), reg_map.at(value_), static_cast<u16>(field_->get_offset()));
}

// --- IR_Binary ---
IR_Binary::IR_Binary(Op op, IR_Reg* dest, IR_Value* left, IR_Value* right) : op_(op), dest_(dest), left_(left), right_(right) {}
std::string IR_Binary::to_string() const {
    return fmt::format("binary_op {}, {}, {}", dest_->to_string(), left_->to_string(), right_->to_string());
}

void IR_Binary::generate(FunctionDescBuilder& builder, const std::unordered_map<IR_Value*, u32>& reg_map) {
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
        default:      opcode = Opcode::ADD_INT; break;
    }
    builder.add_instruction(opcode, reg_map.at(dest_), reg_map.at(left_), reg_map.at(right_));
}

// --- IR_Compare ---
IR_Compare::IR_Compare(Cond cond, IR_Reg* dest, IR_Value* left, IR_Value* right) : cond_(cond), dest_(dest), left_(left), right_(right) {}
std::vector<IR_Value*> IR_Compare::get_used_values() const {
    return { dest_, left_, right_ };
}
std::string IR_Compare::to_string() const { return "cmp"; }
void IR_Compare::generate(FunctionDescBuilder& builder, const std::unordered_map<IR_Value*, u32>& reg_map) {
    Opcode opcode;
    switch (cond_) {
        case Cond::EQ: opcode = Opcode::CMP_EQUAL; break;
        case Cond::NE: opcode = Opcode::CMP_NOT_EQUAL; break;
        case Cond::LT: opcode = Opcode::CMP_LT; break;
        case Cond::LE: opcode = Opcode::CMP_LT_EQUAL; break;
        case Cond::GT: opcode = Opcode::CMP_GT; break;
        case Cond::GE: opcode = Opcode::CMP_GT_EQUAL; break;
        default:       opcode = Opcode::CMP_EQUAL; break;
    }
    builder.add_instruction(opcode, reg_map.at(dest_), reg_map.at(left_), reg_map.at(right_));
}

// --- Flow Control ---
IR_BranchIf::IR_BranchIf(IR_Value *cond, int t, int f) : cond_(cond), true_label_(t), false_label_(f) {}
void IR_BranchIf::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::BRANCH_IF, reg_map.at(cond_), 0, (u16)true_label_);
}

IR_Branch::IR_Branch(int l) : label_(l) {}
void IR_Branch::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &) { builder.add_branch_label(label_); }

IR_Label::IR_Label(int l) : label_(l) {}
void IR_Label::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &) { builder.add_label(label_); }

IR_Return::IR_Return(IR_Value *v) : value_(v) {}
std::vector<IR_Value*> IR_Return::get_used_values() const {
    if (value_) return { value_ };
    return {};
}
void IR_Return::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::RETURN, value_ ? reg_map.at(value_) : 0, 0, 0);
}

// Пустые заглушки для to_string там, где они нужны для линковки
std::string IR_BranchIf::to_string() const { return "br_if"; }
std::string IR_Branch::to_string() const { return "jmp"; }
std::string IR_Label::to_string() const { return fmt::format("L{}:", label_); }
std::string IR_Return::to_string() const { return "ret"; }
std::string IR_Call::to_string() const { return "call"; }

IR_Call::IR_Call(IR_Reg *res, IR_Value *fn, IR_Value *tp, std::vector<IR_Value *> a) : result_(res), function_(fn), this_ptr_(tp), args_(a) {}
std::vector<IR_Value*> IR_Call::get_used_values() const {
    std::vector<IR_Value*> res;
    if (result_) res.push_back(result_);
    if (function_) res.push_back(function_);
    if (this_ptr_) res.push_back(this_ptr_);
    for (auto* a : args_) res.push_back(a);
    return res;
}
void IR_Call::generate(FunctionDescBuilder &builder, const std::unordered_map<IR_Value *, u32> &reg_map) {
    builder.add_instruction(Opcode::CALL, result_ ? reg_map.at(result_) : 0, reg_map.at(function_), (u16)args_.size());
}

} // namespace sootc