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
    
    void parse_arguments(const script::Object& args_form, FunctionEnv* env);
    void compile_body(const script::Object& body_forms, FunctionEnv* env);

    // BUILD Phase
    RelocatableBuffer build(FunctionEnv* fe);   
    RelocatableBuffer build(FunctionEnv* fe, const std::string& name);

private:
    
    TypeSystem& ts_;
    Compiler* compiler_;

    static int s_lambda_index;
};

} // namespace sootc