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
    
    // DECLARE Phase: создает MethodEnv
    IR_Value* declare(const script::Object& form, const script::Object& rest, Env* env);
    
    // RESOLVE Phase: компилирует тело метода
    void resolve(MethodEnv* m_env);
    
    // BUILD Phase: генерирует байткод
    carbon::files::RelocatableBuffer build(MethodEnv* m_env);

private:
    std::string extract_method_name(const script::Object& form);
    std::string extract_owner_name(const script::Object& rest);
    void parse_arguments(const script::Object& args_form, MethodEnv* m_env);
    
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc