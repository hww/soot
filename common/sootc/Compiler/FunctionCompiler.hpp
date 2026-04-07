#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/Env/Export.hpp"
#include "sootc/IR/IR_Value.hpp"

using namespace carbon::files;

namespace sootc {

class Compiler;
class StaticObject;

class FunctionCompiler {
public:
    FunctionCompiler(TypeSystem& ts, Compiler* compiler);

    // Единый метод компиляции функции
    IR_Value* compile_function(const script::Object& form, 
                                const script::Object& rest, 
                                Env* env);
    
    // Единый метод компиляции метода
    IR_Value* compile_method(const script::Object& form,
                              const script::Object& rest,
                              TypeEnv* type_env,
                              int method_id);
    
    // Единый метод компиляции состояния
    IR_Value* compile_state(const script::Object& form,
                             const script::Object& rest,
                             TypeEnv* type_env);
    
    // Для статических данных
    StaticObject* compile_static(const script::Object& form, Env* env);

    // BUILD Phase
    RelocatableBuffer build(FunctionEnv* fe);
    RelocatableBuffer build_method(MethodEnv* me);
    RelocatableBuffer build_state(StateEnv* se);
    RelocatableBuffer build_static(StaticObject* so);

private:
    void parse_arguments(const script::Object& args_form, FunctionEnv* env);
    void compile_body(const script::Object& body_forms, FunctionEnv* env);
    
    TypeSystem& ts_;
    Compiler* compiler_;
    
    std::string make_function_symbol(const std::string& name);
    std::string make_local_function_symbol(const std::string& name);
    std::string make_method_symbol(const std::string& type_name, const std::string& method_name);
    std::string make_state_symbol(const std::string& type_name, const std::string& state_name);

    static int s_lambda_index;
};

} // namespace sootc