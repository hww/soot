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

class StaticBufferFactory {
    public:
    static size_t write_from_type(TypeSystem* ts, StaticBuffer* dest,
                                            size_t offset, Type* type);
    static void write_array_field(TypeSystem* ts, StaticBuffer* dest, 
                                            size_t field_offset, const Field& field, Type* element_type);
    static inline size_t align_up(size_t value, size_t alignment);

};

}