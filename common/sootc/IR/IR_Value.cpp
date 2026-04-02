// common/sootc/src/IR/IR_Value.cpp
#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/Env/FunctionEnv.hpp" 
#include "common/sootc/Env/Env.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/IR/IR_Node.hpp"

namespace sootc {

// ===========================================================
// IR_Value
// ===========================================================
    
std::optional<std::pair<std::string, RelocatableBuffer>> IR_Value::build(Compiler* c) {
    (void)c;  // подавляем warning
    return std::nullopt; 
}

// ===========================================================
// IR_Reg
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
// IR_Const
// ===========================================================

IR_Const::IR_Const(Type *type, s64 val) : IR_Value(type), int_val_(val), is_float_(false) {}

IR_Const::IR_Const(Type *type, float val) : IR_Value(type), float_val_(val), is_float_(true) {}

IR_Reg* IR_Const::to_reg(Env& env) {
    auto* f_env = env.function_env();  // ← исправлено: function_env() вместо lookup_parent_function_env
    if (!f_env) return nullptr;

    auto* r = f_env->alloc_reg(type_);
    env.emit(script::Object(), std::unique_ptr<IR_Node>(new IR_LoadConst(r, this)));  // ← исправлено: добавить form
    return r;
}

std::string IR_Const::to_string() const {
    if (is_float_) {
        return std::to_string(float_val_);
    }
    return std::to_string(int_val_);
}

// ===========================================================
// IR_Field
// ===========================================================

IR_Field::IR_Field(IR_Value *base, const Field &field)
    : IR_Value(field.type().get()), base_(base), field_(field) {}

IR_Reg* IR_Field::to_reg(Env& env) {
    auto* f_env = env.function_env();  // ← исправлено
    if (!f_env) return nullptr;

    auto* r = f_env->alloc_reg(type_);
    env.emit(script::Object(), std::unique_ptr<IR_Node>(new IR_LoadField(r, this)));  // ← исправлено
    return r;
}

std::string IR_Field::to_string() const {
    return base_->to_string() + "." + field_.name();
}

// ===========================================================
// IR_FunctionValue
// ===========================================================

void IR_FunctionValue::resolve(Compiler* c) {
    FunctionCompiler fn_c(c->ts(), c);
    fn_c.resolve_body(this->get_env());  // ← исправлено: resolve_body вместо compile_body
}

std::optional<std::pair<std::string, RelocatableBuffer>> IR_FunctionValue::build(Compiler* c) {
    FunctionCompiler fn_c(c->ts(), c);
    return {{"function", fn_c.build(this->get_env())}};
}

// ===========================================================
// IR_MethodValue
// ===========================================================

std::string IR_MethodValue::name() const { return m_env->name(); }
std::string IR_MethodValue::type_name() const { return m_env->type()->get_name(); }

// ===========================================================
// IR_StateValue
// ===========================================================

std::string IR_StateValue::name() const { return m_env->name(); }
std::string IR_StateValue::type_name() const { return m_env->type()->get_name(); }

// ===========================================================
// IR_Type
// ===========================================================

std::string IR_Type::to_string() const { 
    return "type:" + (m_env ? m_env->name() : "unknown");  // ← исправлено: name() вместо get_name()
}

void IR_Type::resolve(Compiler* c) {
    (void)c;  // подавляем warning
    // Типам обычно не нужно resolve тел, но если у них есть 
    // инициализаторы статических полей — это делается здесь.
}

std::optional<std::pair<std::string, RelocatableBuffer>> IR_Type::build(Compiler* c) {
    TypeCompiler t_c(c->ts(), c);
    return {{"type", t_c.build(this->get_env())}};  // ← исправлено: get_env() возвращает TypeEnv*
}

} // namespace sootc