// TypeCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "Env.hpp"

using namespace carbon::files;

namespace sootc {

class TypeCompiler {
public:
    TypeCompiler(TypeSystem& ts);
    
    // Компилирует deftype форму в RelocatableBuffer
    // Вход: (deftype name (parent) fields... options...)
    RelocatableBuffer compile(const script::Object& form, Env* env);
    
private:
    TypeSystem& ts_;
    
    // Построение буфера из TypeDesc
    RelocatableBuffer build_type_buffer(const TypeDesc& type_desc,
                                         const std::vector<MethodDef>& methods,
                                         const std::vector<StateDef>& states);
    
    // Конвертация TypeFlags из парсера в бинарный формат
    u32 convert_flags(const TypeFlags& flags);
    
    // Конвертация RegClass
    RegClass convert_reg_class(const std::string& reg_class);
};

} // namespace sootc