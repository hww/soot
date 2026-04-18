// IR_Expression.cpp
#include "common/sootc/IR/IR_Expression.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include "type_system/TypeSystem.hpp"
#include <fmt/format.h>

namespace sootc {

// ===================================================
// --- IR_Move ---
// ===================================================

IR_Move::IR_Move(IR_Reg *dest, IR_Value *src) : IR_Expression(src->get_type()), dest_(dest), src_(src) {}

std::string IR_Move::to_string() const { 
    return fmt::format("mov {}, {}", dest_->to_string(), src_->to_string()); 
}

void IR_Move::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    u8 dest_reg = fn_env->get_reg_index(dest_);
    u8 src_reg = fn_env->get_reg_index(src_);
    fn_env->add_instruction(Opcode::MoveInt, dest_reg, src_reg, 0);
}

std::vector<IR_Value*> IR_Move::get_used_values() const { 
    return { dest_, src_ }; 
}

void IR_Move::resolve(Compiler* c) {
    if (src_) src_->resolve(c);
    if (dest_) dest_->resolve(c);
}

// ===================================================
// --- IR_LoadConst ---
// ===================================================

IR_LoadConst::IR_LoadConst(IR_Reg *dest, IR_Const *value) : IR_Expression(value->get_type()), dest_(dest), value_(value) {}

std::string IR_LoadConst::to_string() const { 
    return fmt::format("load {}, {}", dest_->to_string(), value_->to_string()); 
}

void IR_LoadConst::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    auto* file_env = env.file_env();
    if (!file_env) return;
    
    u8 d = fn_env->get_reg_index(dest_);

    if (value_->is_float()) {
        float f_val = value_->get_float();
        u16 slot_idx = static_cast<u16>(fn_env->add_float(f_val));
        fn_env->add_instruction_imm_u16(Opcode::LoadStaticFloat, d, slot_idx);
    } else {
        int32_t i_val = static_cast<int32_t>(value_->get_int());
        if (i_val >= -32768 && i_val <= 32767) {
            fn_env->add_instruction_imm_u16(Opcode::LoadU16Imm, d, static_cast<u16>(i_val));
        } else {
            u16 slot_idx = static_cast<u16>(fn_env->add_int32(i_val));
            fn_env->add_instruction_imm_u16(Opcode::LoadStaticInt, d, slot_idx);
        }
    }
    fn_env->set_reg(this, d);
}

// ===================================================
// --- IR_LoadString ---
// ===================================================

IR_LoadString::IR_LoadString(IR_Reg* dest, const std::string& value) 
    : IR_Expression(TypeSystem::instance().lookup_type("string")), dest_(dest), value_(value) {}

std::string IR_LoadString::to_string() const {
    return fmt::format("{} = \"{}\"", dest_->to_string(), value_);
}

void IR_LoadString::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    auto* file_env = env.file_env();
    if (!file_env) return;
    
    u8 d = fn_env->get_reg_index(dest_);
    u16 slot_idx = file_env->add_string_pointer(value_);
    fn_env->add_instruction_imm_u16(Opcode::LoadStaticPointer, d, slot_idx);
    fn_env->set_reg(this, d);
}

// ===================================================
// --- IR_LoadField ---
// ===================================================

IR_LoadField::IR_LoadField(IR_Reg *dest, IR_Value *base, u32 offset, Type* field_type)
    : IR_Expression(field_type), dest_(dest), base_(base), offset_(offset), field_type_(field_type) {}

std::string IR_LoadField::to_string() const { 
    return fmt::format("load_field {}, offset:{}", dest_->to_string(), offset_); 
}

void IR_LoadField::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    base_->emit(env, compiler);
    
    u8 dest_reg = fn_env->get_reg_index(dest_);
    u8 base_reg = fn_env->get_reg_index(base_);
    fn_env->add_instruction(Opcode::LoadPointer, dest_reg, base_reg, static_cast<u16>(offset_));
    fn_env->set_reg(this, dest_reg);
}

// ===================================================
// --- IR_StoreField ---
// ===================================================

IR_StoreField::IR_StoreField(IR_Value *base, u32 offset, IR_Value *value)
    : IR_Expression(TypeSystem::instance().lookup_type("void")), base_(base), offset_(offset), value_(value) {}

std::string IR_StoreField::to_string() const { 
    return fmt::format("store_field offset:{}, {}", offset_, value_->to_string()); 
}

void IR_StoreField::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    base_->emit(env, compiler);
    value_->emit(env, compiler);
    
    u8 base_reg = fn_env->get_reg_index(base_);
    u8 value_reg = fn_env->get_reg_index(value_);
    fn_env->add_instruction(Opcode::StorePointer, base_reg, value_reg, static_cast<u16>(offset_));
}

// ===================================================
// --- IR_Binary ---
// ===================================================

IR_Binary::IR_Binary(Op op, IR_Reg* dest, IR_Value* left, IR_Value* right) 
    : IR_Expression(dest->get_type()), op_(op), dest_(dest), left_(left), right_(right) {}

void IR_Binary::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    Opcode opcode;
    switch (op_) {
        case Op::ADD: opcode = Opcode::IAdd; break;
        case Op::SUB: opcode = Opcode::ISub; break;
        case Op::MUL: opcode = Opcode::IMul; break;
        case Op::DIV: opcode = Opcode::IDiv; break;
        case Op::MOD: opcode = Opcode::IMod; break;
        case Op::AND: opcode = Opcode::OpBitAnd; break;
        case Op::OR:  opcode = Opcode::OpBitOr; break;
        case Op::XOR: opcode = Opcode::OpBitXor; break;
        default:      opcode = Opcode::IAdd; break;
    }
    
    left_->emit(env, compiler);
    right_->emit(env, compiler);
    
    u8 dest_reg = fn_env->get_reg_index(dest_);
    u8 left_reg = fn_env->get_reg_index(left_);
    u8 right_reg = fn_env->get_reg_index(right_);
    
    fn_env->add_instruction(opcode, dest_reg, left_reg, right_reg);
    fn_env->set_reg(this, dest_reg);
}

// ===================================================
// --- IR_Compare ---
// ===================================================

IR_Compare::IR_Compare(Cond cond, IR_Reg* dest, IR_Value* left, IR_Value* right) 
    : IR_Expression(dest->get_type()), cond_(cond), dest_(dest), left_(left), right_(right) {}

void IR_Compare::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    Opcode opcode;
    switch (cond_) {
        case Cond::EQ: opcode = Opcode::IEqual; break;
        case Cond::NE: opcode = Opcode::INotEqual; break;
        case Cond::LT: opcode = Opcode::ILessThan; break;
        case Cond::LE: opcode = Opcode::ILessThanEqual; break;
        case Cond::GT: opcode = Opcode::IGreaterThan; break;
        case Cond::GE: opcode = Opcode::IGreaterThanEqual; break;
        default:       opcode = Opcode::IEqual; break;
    }
    
    left_->emit(env, compiler);
    right_->emit(env, compiler);
    
    u8 dest_reg = fn_env->get_reg_index(dest_);
    u8 left_reg = fn_env->get_reg_index(left_);
    u8 right_reg = fn_env->get_reg_index(right_);
    
    fn_env->add_instruction(opcode, dest_reg, left_reg, right_reg);
    fn_env->set_reg(this, dest_reg);
}

// ===================================================
// --- Flow Control ---
// ===================================================

IR_BranchIf::IR_BranchIf(IR_Value *cond, Label true_label) 
    : IR_Expression(TypeSystem::instance().lookup_type("void")), cond_(cond), true_label_(true_label) {}

void IR_BranchIf::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    cond_->emit(env, compiler);
    u8 cond_reg = fn_env->get_reg_index(cond_);
    fn_env->add_branch_reference(true_label_.name); 
    fn_env->add_instruction_imm_s16(Opcode::BranchIf, cond_reg, 0);
}

IR_Branch::IR_Branch(Label label) 
    : IR_Expression(TypeSystem::instance().lookup_type("void")), label_(label) {}

void IR_Branch::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    fn_env->add_branch_reference(label_.name); 
    fn_env->add_instruction_imm_s16(Opcode::Branch, 0, 0);
}

IR_Label::IR_Label(Label label) 
    : IR_Expression(TypeSystem::instance().lookup_type("void")), label_(label) {}

void IR_Label::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    fn_env->add_label(label_.name); 
}

// ===================================================
// --- IR_Return ---
// ===================================================

IR_Return::IR_Return(IR_Value *value) 
    : IR_Expression(value ? value->get_type() : TypeSystem::instance().lookup_type("void")), value_(value) {}

void IR_Return::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    if (value_) {
        value_->emit(env, compiler);
        fn_env->add_instruction(Opcode::Return, fn_env->get_reg_index(value_), 0, 0);
    } else {
        fn_env->add_instruction(Opcode::Return, 0, 0, 0);
    }
}

// ===================================================
// --- IR_Call ---
// ===================================================

IR_Call::IR_Call(IR_Reg *result, IR_Value *function, IR_Value *this_ptr, std::vector<IR_Value *> args)
    : IR_Expression(result ? result->get_type() : TypeSystem::instance().lookup_type("void")), 
      result_(result), function_(function), this_ptr_(this_ptr), args_(args) {}

void IR_Call::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    // Генерируем аргументы
    for (auto* arg : args_) {
        arg->emit(env, compiler);
    }
    if (this_ptr_) {
        this_ptr_->emit(env, compiler);
    }
    function_->emit(env, compiler);
    
    u16 arg_count = static_cast<u16>(args_.size());
    u8 func_reg = fn_env->get_reg_index(function_);
    u8 result_reg = result_ ? fn_env->get_reg_index(result_) : 0;
    
    fn_env->add_instruction(Opcode::Call, result_reg, func_reg, arg_count);
    if (result_) {
        fn_env->set_reg(this, result_reg);
    }
}

// ===================================================
// --- IR_FieldAccess ---
// ===================================================

IR_FieldAccess::IR_FieldAccess(IR_Value *base, const Field &field)
    : IR_Expression(field.type().get()), base_(base), field_(field) {}

void IR_FieldAccess::emit(Env& env, Compiler* compiler) {
    auto* fn_env = env.function_env();
    if (!fn_env) return;
    
    base_->emit(env, compiler);
    u8 base_reg = fn_env->get_reg_index(base_);
    u8 dest_reg = fn_env->alloc_reg();
    
    fn_env->add_instruction(Opcode::LoadPointer, dest_reg, base_reg, static_cast<u16>(field_.offset()));
    fn_env->set_reg(this, dest_reg);
}

} // namespace sootc