#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"


namespace sootc {

class Compiler;
class IR_Type;
class TypeEnv;
class MethodEnv;
class IR_Value;

class TypeCompiler {
public:
    TypeCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Единый метод компиляции типа
    IR_Value* compile(const script::Object& form, const script::Object& rest, Env* env);
    
    // BUILD Phase
    ProgramBinaryElement build(TypeEnv* t_env);

private:
    MethodEnv* find_method_in_hierarchy(TypeEnv* start_env, int method_id, TypeEnv*& out_defining_type);
    TypeSpec parse_state_type(const script::Object& form, Env* env);

    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc