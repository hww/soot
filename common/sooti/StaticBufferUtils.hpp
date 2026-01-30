#pragma once

/*!
 * @file StaticBufferUtils.hpp
 * Утилиты для работы со статическими буферами и сериализации данных.
 * Обеспечивает запись данных в буферы с учетом системы типов GOAL.
 */

#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <fmt/format.h>

#include "common/type_system/TypeSystem.hpp"
#include "StaticBuffer.hpp"
#include "common/sooti/Object.hpp"

namespace script {

class StaticBufferUtils {
public:
    // ========================================================================
    // Основной публичный интерфейс
    // ========================================================================
    
    /**
     * Рекурсивная запись данных в буфер.
     * @param ts Система типов
     * @param dest Целевой буфер
     * @param offset Смещение в целевом буфере
     * @param type Тип данных для записи
     * @param source Источник данных (null, StaticBuffer, или Lisp данные)
     */
    static void write_recursive(TypeSystem* ts, StaticBuffer* dest,
                               size_t offset, Type* type, const Object& source);
    
    /**
     * Инициализация памяти под указанный тип (зануление + тип-теги).
     */
    static void write_from_type(TypeSystem* ts, StaticBuffer* dest,
                               size_t offset, Type* type);
    
    /**
     * Копирование данных из другого буфера.
     */
    static void write_from_buffer(TypeSystem* ts, StaticBuffer* dest,
                                 size_t offset, Type* type, StaticBuffer* src);
    
    /**
     * Запись данных из Lisp объекта.
     */
    static void write_from_data(TypeSystem* ts, StaticBuffer* dest,
                               size_t offset, Type* type, const Object& source);
    
private:
    // ========================================================================
    // Приватные вспомогательные методы для write_from_type
    // ========================================================================
    
    static void initialize_basic_type(TypeSystem* ts, StaticBuffer* dest,
                                     size_t offset, Type* type);
    
    static void initialize_structure_type(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, StructureType* type);
    
    static void process_structure_field(TypeSystem* ts, StaticBuffer* dest,
                                       size_t base_offset, const Field& field);
    
    static void initialize_array_field(TypeSystem* ts, StaticBuffer* dest,
                                      size_t base_offset, const Field& field);
    
    // ========================================================================
    // Приватные вспомогательные методы для write_from_buffer
    // ========================================================================
    
    static void copy_relocations(StaticBuffer* dest, StaticBuffer* src,
                                size_t dest_offset);
    
    static void fix_type_tag(TypeSystem* ts, StaticBuffer* dest,
                            size_t offset, Type* type);
    
    // ========================================================================
    // Приватные вспомогательные методы для write_from_data
    // ========================================================================
    
    // Обработка разных категорий типов
    static void write_value_data(TypeSystem* ts, StaticBuffer* dest,
                                size_t offset, ValueType* type, const Object& source);
    
    static void write_structure_data(TypeSystem* ts, StaticBuffer* dest,
                                    size_t offset, StructureType* type, const Object& source);
    
    static void write_enum_data(TypeSystem* ts, StaticBuffer* dest,
                               size_t offset, EnumType* type, const Object& source);
    
    static void write_bitfield_data(TypeSystem* ts, StaticBuffer* dest,
                                   size_t offset, BitFieldType* type, const Object& source);
    
    static void write_string_data(TypeSystem* ts, StaticBuffer* dest,
                                 size_t offset, Type* type, const Object& source);
    
    // Обработка структур из разных источников
    static void write_from_alist(TypeSystem* ts, StaticBuffer* dest,
                                size_t offset, StructureType* type, const Object& alist);
    
    static void write_from_hash_table(TypeSystem* ts, StaticBuffer* dest,
                                     size_t offset, StructureType* type, const Object& hash_table);
    
    // Утилиты для битовых полей
    static uint32_t parse_bitfield_value(BitFieldType* type, const Object& source);
    static uint32_t get_bitfield_flag_mask(BitFieldType* type, const std::string& flag_name);
    
    // ========================================================================
    // Общие утилиты
    // ========================================================================
    
    static void check_bounds(StaticBuffer* dest, size_t offset, size_t size,
                            const std::string& operation, const std::string& type_name);
    
    static Type* get_field_type(TypeSystem* ts, const Field& field);
    
    template<typename T>
    static T* safe_dynamic_cast(Type* type, const std::string& expected_class);

    // ========================================================================
    // Доступ к полям структуры
    // ========================================================================

    Object read_field(TypeSystem* ts, StaticBuffer* dest, size_t base_offset, Type* type, const std::string& field_name);
    Object read_primitive(TypeSystem* ts, StaticBuffer* dest, size_t offset, Type* type);
};

// ========================================================================
// Реализация шаблонных методов
// ========================================================================

template<typename T>
T* StaticBufferUtils::safe_dynamic_cast(Type* type, const std::string& expected_class) {
    if (!type) {
        throw std::runtime_error("[StaticBuffer] Type is null");
    }
    
    if (type->get_class_name() != expected_class) {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Expected type class '{}', got '{}'",
            expected_class, type->get_class_name()
        ));
    }
    
    T* result = dynamic_cast<T*>(type);
    if (!result) {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Failed to cast type '{}' to {}",
            type->get_name(), typeid(T).name()
        ));
    }
    
    return result;
}

class BufferWriter {
    StaticBuffer* m_buf;
    size_t m_cursor = 0;

public:
    // Возвращает адрес начала записи
    size_t write(TypeSystem* ts, Type* type, const Object& data) {
        // 1. Выравниваем
        m_cursor = align_up(m_cursor, type->get_in_memory_alignment());
        size_t start = m_cursor;
        
        // 2. Пишем
        StaticBufferUtils::write_recursive(ts, m_buf, m_cursor, type, data);
        
        // 3. Двигаем курсор
        m_cursor += type->get_size_in_memory();
        return start; 
    }
    
private:
    template <typename T>
    T align_up(T value, size_t alignment) {
        // Проверка: alignment должен быть степенью двойки (2, 4, 8, 16...)
        return (value + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace script