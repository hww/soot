#pragma once

/*!
 * @file StaticBufferUtils.hpp
 * Полная система сериализации/десериализации для статических буферов.
 * Поддержка чтения и записи с учетом системы типов GOAL.
 */

#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <fmt/format.h>

#include "common/type_system/TypeSystem.hpp"
#include "StaticBuffer.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/ListBuilder.hpp"

namespace script {

class StaticBufferReader {
public:

    // Главная точка входа Универсальный диспетчер чтения по указателю
    static Object read_value_at_ptr(TypeSystem* ts, void* ptr, Type* type) ;

    // Чтение примитива
    static Object read_primitive_at_ptr(void* ptr, ValueType* type) ;

    // Чтение структуры в alist (для полной деривации объекта)
    static Object read_structure_to_alist(TypeSystem* ts, void* ptr, StructureType* type) ;

    static Object read_enum_at_ptr(TypeSystem* ts, void* ptr, EnumType* type);

    static Object read_bitfield_at_ptr(void* ptr, BitFieldType* type);

    static Object read_string_at_ptr(void* ptr);
};

} // script