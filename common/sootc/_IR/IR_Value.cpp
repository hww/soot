// IR_Value.cpp
#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/Env/FunctionEnv.hpp" 
#include "common/sootc/Env/TypeEnv.hpp"
#include "common/sootc/Env/Env.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/StateCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include <stdexcept>

namespace sootc {

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

IR_Const* IR_Const::create_int(Type* type, i64 val) {
    return new IR_Const(type, val);
}

IR_Const* IR_Const::create_float(Type* type, f64 val) {
    return new IR_Const(type, val);
}

std::string IR_Const::to_string() const {
    if (is_float_) {
        return std::to_string(float_val_);
    }
    return std::to_string(int_val_);
}

// ===========================================================
// IR_MethodValue
// ===========================================================

IR_MethodValue::IR_MethodValue(MethodEnv* env) 
        : IR_Value(env->type()), m_env(env) {
}


void IR_MethodValue::resolve(Compiler* c) {
    if (m_env) {
        for (auto& [name, value] : m_env->symbols_map()) {
            if (value) value->resolve(c);
        }
    }
}

ProgramBinaryElement IR_MethodValue::serialize(Compiler* c) {
    MethodCompiler m_c(c->ts(), c);
    return m_c.build(m_env);
}

// ===========================================================
// IR_StateValue
// ===========================================================

IR_StateValue::IR_StateValue(StateEnv* env) 
        : IR_Value(env->type()), m_env(env) {}

void IR_StateValue::resolve(Compiler* c) {
    /* FIX ME 
    if (m_env) {
        for (auto& [name, value] : m_env->symbols_map()) {
            if (value) value->resolve(c);
        }
    }
        */
}

ProgramBinaryElement IR_StateValue::serialize(Compiler* c) {
    StateCompiler s_c(c->ts(), c);
    return s_c.build(m_env);
}

// ===========================================================
// IR_Type
// ===========================================================

IR_Type::IR_Type(TypeEnv* env) 
        : IR_Value(env->get_type()), m_env(env) {}

std::string IR_Type::to_string() const { 
    return "type:" + (m_env ? m_env->name() : "unknown"); 
}

void IR_Type::resolve(Compiler* c) {
    (void)c;
    // Типы не требуют разрешения имен
}

ProgramBinaryElement IR_Type::serialize(Compiler* c) {
    TypeEnv* t_env = get_env();
    Type* type = t_env ? t_env->get_type() : nullptr;

    // Builtin типы не компилируем
    if (type && c->ts().fully_defined_type_exists(type->name())) {
        if (type->name() == "type" || type->allow_in_runtime()) {
            return ProgramBinaryElement{0};
        }
    }

    TypeCompiler t_c(c->ts(), c);
    return t_c.build(this->get_env());
}

// ===========================================================
// IR_ExternValue
// ===========================================================

void IR_ExternValue::resolve(Compiler* c) {
    // Проверяем, что extern символ действительно существует
    //if (!c->ts().has_symbol(m_name)) {
        // Можно добавить warning, но не ошибка - extern может быть из другого модуля
        // lg::warn("Extern symbol '{}' not found in current module", m_name);
    //}
    throw std::runtime_error("FIX ME");
}

// ===========================================================
// IR_StaticValue
// ===========================================================

void IR_StaticValue::resolve(Compiler* c) {
    (void)c;
    // Статические значения не требуют разрешения
}

// ===========================================================
// IR_LiteralValue
// ===========================================================

// ===========================================================
// IR_SymbolReference
// ===========================================================

void IR_SymbolReference::resolve(Compiler* c) {
    // Ищем в окружении, используя сам объект символа
    IR_Value* found = env_->lookup(symbol_.to_std_string());
    if (found) {
        // Подменяем...
    }
}
} // namespace sootc