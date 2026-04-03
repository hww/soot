#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Env/MethodEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "common/util/Log.hpp"
#include "type_system/Type.hpp"

namespace sootc {

MethodCompiler::MethodCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// extract_method_name
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

// ============================================================================
// parse_form
// ============================================================================

void MethodCompiler::parse_form(const script::Object& rest,
                                 std::string& out_type_name,
                                 script::Object& out_args_list,
                                 script::Object& out_body_forms) {
    // rest = (type-or-args ...)
    auto first = rest.as_pair()->car;
    
    if (first.is_symbol()) {
        // Форма 1: (type args... body...)
        out_type_name = first.as_symbol().c_str();
        out_args_list = rest.as_pair()->cdr.as_pair()->car;
        out_body_forms = rest.as_pair()->cdr.as_pair()->cdr;
    } 
    else if (first.is_pair()) {
        // Форма 2: (((this type) args...) body...)
        auto first_arg = first.as_pair()->car;
        if (first_arg.is_pair() && first_arg.as_pair()->car.is_symbol() &&
            first_arg.as_pair()->car.as_symbol().c_str() == std::string("this")) {
            out_type_name = first_arg.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
        } else {
            throw std::runtime_error("Invalid method signature: expected (this type) as first argument");
        }
        out_args_list = rest.as_pair()->car;
        out_body_forms = rest.as_pair()->cdr;
    }
    else {
        throw std::runtime_error("Invalid defmethod syntax");
    }
}

// ============================================================================
// compile — принимает form и rest
// ============================================================================

IR_Value* MethodCompiler::compile(const script::Object& form, 
                                   const script::Object& rest, 
                                   Env* env) {
    (void)rest; // rest не используется, парсим из form
    
    std::string method_name = extract_method_name(form);
    
    // rest из compile_defmethod — это всё после defmethod
    // Нужно передать правильный rest в parse_form
    // form = (defmethod name ...)
    // rest_from_compiler = (name ...) ? или сразу аргументы?
    
    // В Compiler::compile_defmethod: rest = всё после defmethod
    // Значит form = (defmethod ...), rest = (name ...)
    // Поэтому для parse_form нужен rest, а не form
    
    auto rest_after_name = form.as_pair()->cdr.as_pair()->cdr;
    
    std::string type_name;
    script::Object args_list;
    script::Object body_forms;
    
    parse_form(rest_after_name, type_name, args_list, body_forms);
    
    // Находим TypeEnv
    IR_Value* type_val = env->lookup(type_name);
    if (!type_val) {
        lg::error("Type '{}' not found for method '{}'", type_name, method_name);
        return nullptr;
    }
    
    auto* ir_type = dynamic_cast<IR_Type*>(type_val);
    if (!ir_type) {
        lg::error("'{}' is not a type", type_name);
        return nullptr;
    }
    // Окрухение типа, в котором объявлен метод
    TypeEnv* type_env = ir_type->get_env();
    Type* type_info = type_env->get_type();
    MethodInfo method_info;
    if (!ts_.try_lookup_method(type_info, method_name, &method_info)) {
        lg::error("Method '{}' not found in type '{}'", method_name, type_name);
        return nullptr;
    }

    // Ищем MethodEnv для данного method_name в TypeEnv
    auto* m_env = new MethodEnv(method_info.id, method_name, type_env, type_info);
    
    parse_arguments(args_list, m_env);
    compile_body(body_forms, m_env);
    
    type_env->bind(method_name, new IR_MethodValue(m_env));
    return new IR_MethodValue(m_env);
}

// ============================================================================
// parse_arguments
// ============================================================================

void MethodCompiler::parse_arguments(const script::Object& args_form, MethodEnv* m_env) {
    auto current = args_form;
    int arg_idx = m_env->params().size(); // 0 для this
    
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

// ============================================================================
// compile_body
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
// build
// ============================================================================

carbon::files::RelocatableBuffer MethodCompiler::build(MethodEnv* m_env) {
    FunctionCompiler func_compiler(ts_, compiler_);
    return func_compiler.build_method(m_env);
}

} // namespace sootc