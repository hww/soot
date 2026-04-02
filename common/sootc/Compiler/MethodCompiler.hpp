#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/files/Definition.hpp"
#include "sootc/Env/Export.hpp"

namespace sootc {

class Compiler;
class IR_MethodValue;
class MethodEnv;

class MethodCompiler {
public:
    MethodCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Единый метод компиляции метода
    IR_Value* compile(const script::Object& form, 
                      const script::Object& rest, 
                      TypeEnv* type_env,
                      int method_id);
    
    // BUILD Phase
    carbon::files::RelocatableBuffer build(MethodEnv* m_env);

private:
    std::string extract_method_name(const script::Object& form);
    void parse_arguments(const script::Object& args_form, MethodEnv* m_env);
    void compile_body(const script::Object& body_forms, MethodEnv* m_env);
    
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc