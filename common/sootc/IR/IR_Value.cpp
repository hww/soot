// common/sootc/src/IR/IR_Value.cpp
#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/Compiler/Env.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/IR/IR_Node.hpp"

namespace sootc {

// ===========================================================
// Reg
// ===========================================================
    
std::optional<std::pair<std::string, RelocatableBuffer>> IR_Value::build(Compiler* c) {
    return std::nullopt; 
}

// ===========================================================
// Reg
// ===========================================================

IR_Reg::IR_Reg(Type *type, u32 index, bool is_arg)
    : IR_Value(type), index_(index), is_arg_(is_arg) {}

std::string IR_Reg::to_string() const {
    if (is_arg_) {
        return "arg" + std::to_string(index_);
    }
    return "r" + std::to_string(index_);
}

// ===========================================================
// Const
// ===========================================================

IR_Const::IR_Const(Type *type, s64 val) : IR_Value(type), int_val_(val), is_float_(false) {}

IR_Const::IR_Const(Type *type, float val) : IR_Value(type), float_val_(val), is_float_(true) {}

IR_Reg* IR_Const::to_reg(Env& env) {
    // В твоем Env.hpp метод называется alloc_reg и он в FunctionEnv
    auto* f_env = env.function_env();
    if (!f_env) return nullptr; // На всякий случай

    auto* r = f_env->alloc_reg(type_);
    env.emit(new IR_LoadConst(r, this));
    return r;
}


std::string IR_Const::to_string() const {
    if (is_float_) {
        return std::to_string(float_val_);
    }
    return std::to_string(int_val_);
}

// ===========================================================
// Field
// ===========================================================

IR_Field::IR_Field(IR_Value *base, const Field &field)
    : IR_Value(field.type().get()), base_(base), field_(field) {}

IR_Reg* IR_Field::to_reg(Env& env) {
    auto* f_env = env.function_env();
    if (!f_env) return nullptr;

    auto* r = f_env->alloc_reg(type_);
    env.emit(new IR_LoadField(r, this));
    return r;
}

std::string IR_Field::to_string() const {
    return base_->to_string() + "." + field_.name();
}

// ===========================================================
// Function
// ===========================================================

void IR_FunctionValue::resolve(Compiler* c) {
    FunctionCompiler fn_c(c->ts(), c);
    fn_c.compile_body(this);
}

std::optional<std::pair<std::string, RelocatableBuffer>> IR_FunctionValue::build(Compiler* c) {
    FunctionCompiler fn_c(c->ts(), c);
    return {{"function", fn_c.build(this->get_env())}};
}

// ===========================================================
// Method
// ===========================================================

std::string IR_MethodValue::name() const { return m_env->name(); }
std::string IR_MethodValue::type_name() const { return m_env->type()->get_name(); }

// ===========================================================
// State
// ===========================================================

std::string IR_StateValue::name() const { return m_env->name(); }
std::string IR_StateValue::type_name() const { return m_env->type()->get_name(); }

// ===========================================================
// Type
// ===========================================================

std::string IR_Type::to_string() const { 
    return "type:" + (m_env ? m_env->get_name() : "unknown"); 
}

void IR_Type::resolve(Compiler* c) {
    // Типам обычно не нужно resolve тел, но если у них есть 
    // инициализаторы статических полей — это делается здесь.
}

std::optional<std::pair<std::string, RelocatableBuffer>> IR_Type::build(Compiler* c) {
    TypeCompiler t_c(c->ts(), c);
    return {{"type", t_c.build(this)}};
}

} // namespace sootc