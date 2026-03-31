#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "sootc/Compiler/Env.hpp"

namespace sootc {

class Compiler;

class TypeCompiler {
public:
    TypeCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Первый проход: Регистрируем тип в TypeSystem и создаем IR_Type
    IR_Value* declare(const script::Object& form, const script::Object& rest, Env* env);
    
    // Второй проход: Собираем TypeDesc и вложенные определения в буфер
    carbon::files::RelocatableBuffer build(IR_Type* ir_type);

private:
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc