#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/files/Definition.hpp"
#include "sootc/Compiler/Env.hpp"

namespace sootc {

class Compiler;

class MethodCompiler {
public:
    MethodCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Переименовали в declare для единообразия с FunctionCompiler
    IR_Value* declare(const script::Object& form, const script::Object& rest, Env* env);
    
    // Финализация
    carbon::files::RelocatableBuffer build(IR_MethodValue* m_val);

private:
    carbon::files::RelocatableBuffer build_method_buffer(const carbon::files::MethodDef& method_def,
                                                      const carbon::files::RelocatableBuffer& func_buffer);
    std::string extract_owner_name(const script::Object& rest);
    
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc