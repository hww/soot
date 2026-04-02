#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "files/RelocatableBuffer.hpp"
#include "sootc/Env/Export.hpp"
#include "sootc/IR/IR_Value.hpp"

using namespace carbon::files;

namespace sootc {

class Compiler;
class StaticObject;

class FunctionCompiler {
public:
    FunctionCompiler(TypeSystem& ts, Compiler* compiler);

    // ========================================================================
    // DECLARE Phase
    // ========================================================================
    
    // Для обычной функции
    IR_Value* declare_function(const script::Object& form, 
                               const script::Object& rest, 
                               Env* env);
    
    // Для метода (внутри типа)
    IR_Value* declare_method(const script::Object& form,
                             const script::Object& rest,
                             TypeEnv* type_env,
                             int method_id);
    
    // Для состояния
    IR_Value* declare_state(const script::Object& form,
                            const script::Object& rest,
                            TypeEnv* type_env);
    
    // Для статических данных
    StaticObject* declare_static(const script::Object& form, Env* env);

    // ========================================================================
    // RESOLVE Phase
    // ========================================================================
    
    void resolve_body(FunctionEnv* f_env);
    void resolve_method_body(MethodEnv* m_env);
    void resolve_state_body(StateEnv* s_env);

    // ========================================================================
    // BUILD Phase
    // ========================================================================
    
    RelocatableBuffer build(FunctionEnv* fe);
    RelocatableBuffer build_method(MethodEnv* me);
    RelocatableBuffer build_state(StateEnv* se);
    RelocatableBuffer build_static(StaticObject* so);

    // ========================================================================
    // Helpers
    // ========================================================================
    
    void parse_arguments(const script::Object& args_form, FunctionEnv* env);
    void compile_body_from_forms(const script::Object& body_forms, FunctionEnv* env);
    
private:
    TypeSystem& ts_;
    Compiler* compiler_;
    
    // Генерация имени для символов
    std::string make_function_symbol(const std::string& name);
    std::string make_method_symbol(const std::string& type_name, const std::string& method_name);
    std::string make_state_symbol(const std::string& type_name, const std::string& state_name);
};

} // namespace sootc