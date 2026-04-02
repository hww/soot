#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp" 
#include "common/carbon/files/TypeDesc.hpp"
#include "files/RelocatableBuffer.hpp"
#include "sootc/Env/Export.hpp"

namespace sootc {

class Compiler;
class IR_Type;
class TypeEnv;

class TypeCompiler {
public:
    TypeCompiler(TypeSystem& ts, Compiler* compiler);
    
    // DECLARE Phase
    IR_Value* declare(const script::Object& form, const script::Object& rest, Env* env);
    
    // RESOLVE Phase
    void resolve(TypeEnv* t_env);
    
    // BUILD Phase
    RelocatableBuffer build(TypeEnv* t_env);  // ← RelocatableBuffer → SimpleBuffer

private:
    TypeSystem& ts_;
    Compiler* compiler_;
    
    MethodEnv* find_method_in_hierarchy(TypeEnv* start_env, int method_id, TypeEnv*& out_defining_type);
};

} // namespace sootc