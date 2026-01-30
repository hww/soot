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

#pragma once

/*!
 * @file StaticBufferUtils.hpp
 * Полная система сериализации/десериализации для статических буферов.
 * Поддержка чтения и записи с учетом системы типов GOAL.
 */
class StaticBufferUtils {
public:
    // ========================================================================
    // ОСНОВНОЙ ИНТЕРФЕЙС (Запись)
    // ========================================================================
    
    /**
     * Рекурсивная запись данных в буфер.
     * @param ts Система типов
     * @param dest Целевой буфер
     * @param offset Смещение в буфере
     * @param type Тип данных
     * @param source Источник данных (null, буфер, или Lisp данные)
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
    
    // ========================================================================
    // ОСНОВНОЙ ИНТЕРФЕЙС (Чтение)
    // ========================================================================
    
    /**
     * Чтение значения по указателю на тип.
     */
    static Object read_value(TypeSystem* ts, StaticBuffer* src,
                            size_t offset, Type* type);
    
    /**
     * Чтение поля структуры по имени.
     */
    static Object read_field(TypeSystem* ts, StaticBuffer* src,
                            size_t base_offset, Type* type,
                            const std::string& field_name);
    
    /**
     * Рекурсивное чтение всей структуры.
     */
    static Object read_structure(TypeSystem* ts, StaticBuffer* src,
                                size_t offset, StructureType* type);
    
    /**
     * Преобразование буфера в Lisp представление.
     */
    static Object buffer_to_object(TypeSystem* ts, StaticBuffer* src,
                                  size_t offset, Type* type);
    
    // ========================================================================
    // УТИЛИТЫ КОНВЕРТАЦИИ
    // ========================================================================
    
    /**
     * Копирование с преобразованием типов.
     */
    static void copy_with_conversion(TypeSystem* ts,
                                    StaticBuffer* dest, size_t dest_offset, Type* dest_type,
                                    StaticBuffer* src, size_t src_offset, Type* src_type);
    
    /**
     * Проверка эквивалентности данных.
     */
    static bool compare_data(TypeSystem* ts,
                            StaticBuffer* buf1, size_t offset1, Type* type1,
                            StaticBuffer* buf2, size_t offset2, Type* type2);
    
    // ========================================================================
    // ОТЛАДОЧНЫЕ ФУНКЦИИ
    // ========================================================================
    
    /**
     * Дамп содержимого в читаемом формате.
     */
    static std::string dump_value(TypeSystem* ts, StaticBuffer* src,
                                 size_t offset, Type* type, int indent = 0);
    
    /**
     * Дамп всей структуры.
     */
    static std::string dump_structure(TypeSystem* ts, StaticBuffer* src,
                                     size_t offset, StructureType* type);
    
private:
    // ========================================================================
    // ПРИВАТНЫЕ МЕТОДЫ ЗАПИСИ
    // ========================================================================
    
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
    
    static void write_from_alist(TypeSystem* ts, StaticBuffer* dest,
                                size_t offset, StructureType* type, const Object& alist);
    
    // ========================================================================
    // ПРИВАТНЫЕ МЕТОДЫ ЧТЕНИЯ
    // ========================================================================
    
    static Object read_primitive_value(TypeSystem* ts, StaticBuffer* src,
                                      size_t offset, ValueType* type);
    
    static Object read_enum_value(TypeSystem* ts, StaticBuffer* src,
                                 size_t offset, EnumType* type);
    
    static Object read_bitfield_value(TypeSystem* ts, StaticBuffer* src,
                                     size_t offset, BitFieldType* type);
    
    static Object read_string_value(TypeSystem* ts, StaticBuffer* src,
                                   size_t offset, Type* type);
    
    // ========================================================================
    // ОБЩИЕ УТИЛИТЫ
    // ========================================================================
    
    static void check_bounds(StaticBuffer* buf, size_t offset, size_t size,
                            const std::string& operation, const std::string& type_name);
    
    static Type* get_field_type(TypeSystem* ts, const Field& field);
    
    static bool is_compatible_types(TypeSystem* ts, Type* type1, Type* type2);
};


// ========================================================================
// Buffer Writer
// ========================================================================

class StaticWriter : public Accessor {  // Наследуемся от Accessor, а не HeapObject
private:
    size_t m_cursor = 0;
    std::shared_ptr<StaticBuffer> m_buffer;
    std::shared_ptr<TypeSystem> m_ts;

    static std::string reloc_type_to_string(RelocType type) {
        switch (type) {
            case RelocType::ABS_ADDR: return "abs-addr";
            case RelocType::SYMBOL_CRC: return "symbol-crc";
            case RelocType::RELATIVE: return "relative";
            default: return "unknown";
        }
    }

public:
    // Конструкторы
    StaticWriter() {
        define_all_aliases();
    }
    
    explicit StaticWriter(std::shared_ptr<StaticBuffer> buffer,  std::shared_ptr<TypeSystem> ts)
        : m_buffer(std::move(buffer)), m_ts(std::move(ts)) {
        define_all_aliases();
    }

    // Основной метод записи
    size_t write(Type* type, const Object& data) {
        if (!m_buffer || !type) {
            throw std::runtime_error("[BufferWriter] Buffer or type is null");
        }
        
        // 1. Выравнивание
        int alignment = type->get_in_memory_alignment();
        if (alignment > 1) {
            m_cursor = align_up(m_cursor, alignment);
        }
        
        size_t start = m_cursor;
        
        // 2. Проверка границ
        size_t type_size = type->get_size_in_memory();
        if (start + type_size > m_buffer->size()) {
            throw std::runtime_error(fmt::format(
                "[BufferWriter] Buffer overflow: writing {} bytes at offset {}, buffer size {}",
                type_size, start, m_buffer->size()
            ));
        }
        
        // 3. Запись данных
        if (m_ts) {
            StaticBufferUtils::write_recursive(m_ts.get(), m_buffer.get(), start, type, data);
        } else {
            throw std::runtime_error("[BufferWriter] TypeSystem not set");
        }
        
        // 4. Обновление курсора
        m_cursor += type_size;
        return start;
    }
    
    // Перегрузка для записи по имени типа
    size_t write(const std::string& type_name, const Object& data) {
        if (!m_ts) {
            throw std::runtime_error("[BufferWriter] TypeSystem not set");
        }
        
        Type* type = m_ts->lookup_type(type_name);
        if (!type) {
            throw std::runtime_error(fmt::format(
                "[BufferWriter] Type not found: {}", type_name
            ));
        }
        
        return write(type, data);
    }
    
    // Методы управления курсором
    void seek(size_t position) {
        if (position > m_buffer->size()) {
            throw std::runtime_error(fmt::format(
                "[BufferWriter] Seek beyond buffer size: {} > {}",
                position, m_buffer->size()
            ));
        }
        m_cursor = position;
    }
    
    size_t tell() const {
        return m_cursor;
    }
    
    void align_to(size_t alignment) {
        if (alignment > 0 && (alignment & (alignment - 1)) == 0) { // Проверка степени двойки
            m_cursor = align_up(m_cursor, alignment);
        }
    }
    
    // Методы управления буфером
    void set_buffer(std::shared_ptr<StaticBuffer> buffer) {
        m_buffer = std::move(buffer);
        m_cursor = 0; // Сбрасываем курсор при смене буфера
    }
    
    void set_type_system(std::shared_ptr<TypeSystem> ts) {
        m_ts = ts;
    }
    
    std::shared_ptr<StaticBuffer> buffer() const {
        return m_buffer;
    }
    
    size_t remaining() const {
        if (!m_buffer) return 0;
        return m_buffer->size() - m_cursor;
    }
    
    float usage_percent() const {
        if (!m_buffer || m_buffer->size() == 0) return 0.0f;
        return (static_cast<float>(m_cursor) / m_buffer->size()) * 100.0f;
    }
    
    // Методы чтения (удобные обертки)
    Object read(Type* type, size_t offset) const {
        if (!m_buffer || !m_ts || !type) {
            return Object::make_null();
        }
        
        size_t type_size = type->get_size_in_memory();
        if (offset + type_size > m_buffer->size()) {
            return Object::make_null();
        }
        
        return StaticBufferUtils::read_value(m_ts.get(), m_buffer.get(), offset, type);
    }
    
    Object read_field(size_t base_offset, Type* type, const std::string& field_name) const {
        if (!m_buffer || !m_ts || !type) {
            return Object::make_null();
        }
        
        return StaticBufferUtils::read_field(m_ts.get(), m_buffer.get(), base_offset, type, field_name);
    }
    
    // Методы для удобной работы
    size_t write_string(const std::string& str, bool null_terminated = true) {
        if (!m_buffer) {
            throw std::runtime_error("[BufferWriter] Buffer is null");
        }
        
        size_t start = m_cursor;
        size_t bytes_needed = str.length() + (null_terminated ? 1 : 0);
        
        if (start + bytes_needed > m_buffer->size()) {
            throw std::runtime_error(fmt::format(
                "[BufferWriter] Not enough space for string: needed {} bytes, available {}",
                bytes_needed, m_buffer->size() - start
            ));
        }
        
        m_buffer->write_string(start, str, null_terminated);
        m_cursor += bytes_needed;
        return start;
    }
    
    size_t write_raw(const void* data, size_t size) {
        if (!m_buffer) {
            throw std::runtime_error("[BufferWriter] Buffer is null");
        }
        
        size_t start = m_cursor;
        if (start + size > m_buffer->size()) {
            throw std::runtime_error(fmt::format(
                "[BufferWriter] Not enough space: needed {} bytes, available {}",
                size, m_buffer->size() - start
            ));
        }
        
        std::memcpy(m_buffer->data() + start, data, size);
        m_cursor += size;
        return start;
    }
    
    // Методы печати и инспекции
    std::string print() const override {
        if (!m_buffer) {
            return "#<buffer-writer:empty>";
        }
        
        return fmt::format("#<buffer-writer :pos {} :total {} :used {:.1f}%>",
                          m_cursor, m_buffer->size(), usage_percent());
    }
    
    Object inspect() const override {
        if (!m_buffer) {
            return Object::make_list({
                Object::make_symbol("buffer-writer"),
                Object::make_symbol(":status"),
                Object::make_string("empty")
            });
        }
        
        // Собираем информацию о буфере
        std::string status = "ok";
        if (m_cursor >= m_buffer->size()) {
            status = "full";
        } else if (m_cursor == 0) {
            status = "empty";
        }
        
        // Создаем подробный список
        std::vector<Object> items = {
            Object::make_symbol("buffer-writer"),
            Object::make_symbol(":status"),
            Object::make_string(status),
            Object::make_symbol(":cursor"),
            Object::make_integer(static_cast<int64_t>(m_cursor)),
            Object::make_symbol(":size"),
            Object::make_integer(static_cast<int64_t>(m_buffer->size())),
            Object::make_symbol(":remaining"),
            Object::make_integer(static_cast<int64_t>(remaining())),
            Object::make_symbol(":used"),
            Object::make_float(usage_percent()),
            Object::make_symbol(":origin"),
            Object::make_integer(m_buffer->origin()),
            Object::make_symbol(":type-system"),
            m_ts ? Object::make_string("attached") : Object::make_string("detached"),
            Object::make_symbol(":buffer"),
            m_buffer ? Object::make_native_ref(m_buffer) : Object::make_null()
        };
        
        return Object::make_list(items);
    }
    
    // Реализация make_step_alias для доступа к свойствам
    Object make_step_alias(const Object& key) override {
        std::string name = key.to_std_string();

        // 1. Свойства (возвращают данные)
        if (name == "cursor")    return Object::make_integer(m_cursor);
        if (name == "size")      return Object::make_integer(m_buffer ? m_buffer->size() : 0);
        if (name == "remaining") return Object::make_integer(remaining());
        
        return Object::make_null();
    }
    
    // Определение алиасов для работы из Lisp
    void define_all_aliases() override {
        // write (type data) -> offset
        define_alias("write", [this](Accessor* self) -> Object {
            (void)self; // Не используется, но должен быть параметром
            
            // Нужно получить аргументы из контекста
            // В текущем интерфейсе Accessor аргументы не передаются
            // Это требует изменения архитектуры
            
            return Object::make_symbol("write-method-needs-args");
        });
        
        // tell () -> position
        define_alias("tell", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            return Object::make_integer(static_cast<int64_t>(m_cursor));
        });
        
        // buffer () -> buffer-ref
        define_alias("buffer", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            if (m_buffer) {
                return Object::make_native_ref(m_buffer);
            }
            return Object::make_null();
        });
        
        // dump ([limit] [offset]) -> string
        define_alias("dump", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            
            if (!m_buffer) {
                return Object::make_string("Buffer not initialized");
            }
            
            // Без аргументов даем дефолтный дамп
            return create_dump_string(256, 0);
        });
        
        // cursor property
        define_alias("cursor", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            return Object::make_integer(static_cast<int64_t>(m_cursor));
        });
        
        // size property
        define_alias("size", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            if (m_buffer) {
                return Object::make_integer(static_cast<int64_t>(m_buffer->size()));
            }
            return Object::make_integer(0);
        });
        
        // remaining property
        define_alias("remaining", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            return Object::make_integer(static_cast<int64_t>(remaining()));
        });
        
        // usage property (percentage)
        define_alias("usage", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            return Object::make_float(usage_percent());
        });
        
        // origin property
        define_alias("origin", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            if (m_buffer) {
                return Object::make_integer(m_buffer->origin());
            }
            return Object::make_integer(0);
        });
        
        // type-system property
        define_alias("type-system", [this](Accessor* self) -> Object {
            (void)self; // Не используется
            if (m_ts) {
                // Нужно как-то получить Object для TypeSystem
                // Если TypeSystem тоже Accessor:
                return Object::make_native_ref(m_ts);
            }
            return Object::make_string("detached");
        });
    }

private:
    // Создание строки дампа (вынесено в отдельный метод для reuse)
    Object create_dump_string(size_t limit, size_t start_offset) const {
        if (!m_buffer) {
            return Object::make_string("Buffer not initialized");
        }
        
        // Проверка границ
        if (start_offset >= m_buffer->size()) {
            return Object::make_string(fmt::format(
                "Start offset {} exceeds buffer size {}", 
                start_offset, m_buffer->size()
            ));
        }
        
        // Формирование заголовка
        std::string result = fmt::format("Buffer dump (size: {}, cursor: {}, origin: {:#x}):\n",
                                       m_buffer->size(), m_cursor, m_buffer->origin());
        
        // Hex-дамп с ASCII представлением
        size_t bytes_to_dump = std::min(limit, m_buffer->size() - start_offset);
        
        for (size_t i = 0; i < bytes_to_dump; i += 16) {
            size_t line_start = start_offset + i;
            size_t line_end = std::min(line_start + 16, start_offset + bytes_to_dump);
            
            // Адрес строки
            result += fmt::format("\n{:08x}: ", line_start);
            
            // Hex байты
            for (size_t j = line_start; j < line_end; j++) {
                if (j == m_cursor) {
                    result += "><"; // Маркер курсора вокруг байта
                } else {
                    result += fmt::format("{:02x} ", m_buffer->data()[j]);
                }
            }
            
            // Заполнение для выравнивания
            for (size_t j = line_end; j < line_start + 16; j++) {
                result += "   ";
            }
            
            // ASCII представление
            result += " ";
            for (size_t j = line_start; j < line_end; j++) {
                uint8_t byte = m_buffer->data()[j];
                if (byte >= 32 && byte < 127) {
                    result += static_cast<char>(byte);
                } else {
                    result += '.';
                }
            }
        }
        
        // Добавляем информацию о курсоре
        if (m_cursor >= start_offset && m_cursor < start_offset + bytes_to_dump) {
            result += fmt::format("\n\nCursor at offset: {:#x} ({})", 
                                m_cursor, m_cursor);
        }
        
        return Object::make_string(result);
    }
    
    // Утилита для выравнивания
    static size_t align_up(size_t value, size_t alignment) {
        if (alignment == 0) return value;
        return (value + alignment - 1) & ~(alignment - 1);
    }
};


} // namespace script