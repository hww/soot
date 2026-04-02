#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Env/MethodEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "common/util/Log.hpp"

namespace sootc {

MethodCompiler::MethodCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// DECLARE Phase
// ============================================================================

IR_Value* MethodCompiler::declare(const script::Object& form, const script::Object& rest, Env* env) {
    std::string method_name = extract_method_name(form);
    std::string owner_name = extract_owner_name(rest);
    
    IR_Value* owner_val = env->lookup(owner_name);
    if (!owner_val) {
        lg::error("Type '{}' not found for method '{}'", owner_name, method_name);
        return nullptr;
    }
    
    auto* owner_type = dynamic_cast<IR_Type*>(owner_val);
    if (!owner_type) {
        lg::error("'{}' is not a type", owner_name);
        return nullptr;
    }
    
    TypeEnv* type_env = owner_type->get_env();
    Type* type_info = type_env->get_type();
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    auto* m_env = new MethodEnv(method_name, type_env, type_info, type_env);
    m_env->set_source_form(body_forms);
    
    parse_arguments(args_list, m_env);
    
    type_env->bind(method_name, new IR_MethodValue(m_env));
    
    return new IR_MethodValue(m_env);
}

// ============================================================================
// RESOLVE Phase
// ============================================================================

void MethodCompiler::resolve(MethodEnv* m_env) {
    FunctionCompiler func_compiler(ts_, compiler_);
    func_compiler.resolve_method_body(m_env);
}

// ============================================================================
// BUILD Phase
// ============================================================================

carbon::files::RelocatableBuffer MethodCompiler::build(MethodEnv* m_env) {
    FunctionCompiler func_compiler(ts_, compiler_);
    return func_compiler.build_method(m_env);
}

// ============================================================================
// Helpers
// ============================================================================

std::string MethodCompiler::extract_method_name(const script::Object& form) {
    auto pair = form.as_pair();
    if (pair && pair->cdr.is_pair()) {
        auto second = pair->cdr.as_pair()->car;
        if (second.is_symbol()) {
            return second.as_symbol().c_str();
        }
    }
    return "unknown-method";
}

std::string MethodCompiler::extract_owner_name(const script::Object& rest) {
    auto pair = rest.as_pair();
    if (pair && pair->car.is_symbol()) {
        return pair->car.as_symbol().c_str();
    }
    return "unknown-owner";
}

void MethodCompiler::parse_arguments(const script::Object& args_form, MethodEnv* m_env) {
    auto current = args_form;
    int arg_idx = m_env->params().size();
    
    while (current.is_pair()) {
        auto arg_decl = current.as_pair()->car;
        if (arg_decl.is_pair()) {
            std::string arg_name = arg_decl.as_pair()->car.as_symbol().c_str();
            std::string type_name = arg_decl.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type(type_name);
            m_env->define_argument(arg_name, arg_type, arg_idx++);
        } else if (arg_decl.is_symbol()) {
            std::string arg_name = arg_decl.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type("object");
            m_env->define_argument(arg_name, arg_type, arg_idx++);
        }
        current = current.as_pair()->cdr;
    }
}

} // namespace sootc