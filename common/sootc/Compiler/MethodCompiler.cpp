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
// compile — один проход
// ============================================================================

IR_Value* MethodCompiler::compile(const script::Object& form, 
                                   const script::Object& rest, 
                                   TypeEnv* type_env,
                                   int method_id) {
    std::string method_name = extract_method_name(form);
    Type* type_info = type_env->get_type();
    
    // rest = (args... body...)
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    // Создаем MethodEnv
    auto* m_env = new MethodEnv(method_name, type_env, type_info, type_env);
    m_env->method_id = method_id;
    
    // Парсим аргументы (this уже добавлен в конструкторе)
    parse_arguments(args_list, m_env);
    
    // Компилируем тело метода
    compile_body(body_forms, m_env);
    
    // Регистрируем метод в типе
    type_env->bind(method_name, new IR_MethodValue(m_env));
    
    return new IR_MethodValue(m_env);
}

// ============================================================================
// compile_body — создает IR_Node для тела метода
// ============================================================================

void MethodCompiler::compile_body(const script::Object& body_forms, MethodEnv* m_env) {
    auto current = body_forms;
    IR_Value* last_val = nullptr;
    
    while (current.is_pair()) {
        last_val = compiler_->compile(current.as_pair()->car, m_env);
        current = current.as_pair()->cdr;
    }
    
    if (last_val) {
        m_env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    } else {
        Type* obj_type = ts_.lookup_type("object");
        m_env->emit(script::Object(), std::make_unique<IR_Return>(new IR_Const(obj_type, static_cast<s64>(0))));
    }
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
    // form = (defmethod name ...)
    auto pair = form.as_pair();
    if (pair && pair->cdr.is_pair()) {
        auto second = pair->cdr.as_pair()->car;
        if (second.is_symbol()) {
            return second.as_symbol().c_str();
        }
    }
    return "unknown-method";
}

void MethodCompiler::parse_arguments(const script::Object& args_form, MethodEnv* m_env) {
    auto current = args_form;
    int arg_idx = m_env->params().size(); // 0 для this
    
    while (current.is_pair()) {
        auto arg_decl = current.as_pair()->car;
        if (arg_decl.is_pair()) {
            // (name type)
            std::string arg_name = arg_decl.as_pair()->car.as_symbol().c_str();
            std::string type_name = arg_decl.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type(type_name);
            m_env->define_argument(arg_name, arg_type, arg_idx++);
        } else if (arg_decl.is_symbol()) {
            // name (тип object по умолчанию)
            std::string arg_name = arg_decl.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type("object");
            m_env->define_argument(arg_name, arg_type, arg_idx++);
        }
        current = current.as_pair()->cdr;
    }
}

} // namespace sootc