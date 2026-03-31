#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/Compiler/Env.hpp"
#include "sootc/IR/IR_Value.hpp"

namespace sootc {

class Compiler;

class FunctionCompiler {
public:
    FunctionCompiler(TypeSystem& ts, Compiler* compiler);

    // Возвращаем IR_Value*, чтобы совпадало с Compiler::compile
    IR_Value* declare(const script::Object& form, 
                     const script::Object& rest, 
                     Env* env);

    void compile_body(IR_FunctionValue* f_val);

    carbon::files::RelocatableBuffer build(FunctionEnv* fe);

private:
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc