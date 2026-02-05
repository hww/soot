#include "TypeCell.hpp"
#include "StaticBufferReader.hpp"
#include "StaticBufferWriter.hpp"
#include <fmt/format.h>
namespace script {

// ===========================================================================
// TypeCell
// ===========================================================================

Object TypeCell::get() {
    void *ptr = resolve_ptr();
    if (!ptr || !m_type)
        return Object::make_undefined();

    // 1. Примитивы, Enum, Bitfield
    if (!m_type->is_reference()) {
        return StaticBufferReader::read_value_at_ptr(&TypeSystem::instance(), ptr, m_type);
    }

    // 2. Строки и Символы
    if (m_type->get_name() == "string" || m_type->get_name() == "symbol") {
        return StaticBufferReader::read_string_at_ptr(ptr);
    }

    // 3. Структуры — возвращаем саму ячейку для дальнейшей навигации
    return Object::make_heap_object(shared_from_this(), ObjectType::CELL);
}

void TypeCell::set(const Object &val) {
    if (!m_ptr || !m_type) {
        fmt::print(stderr, "[ERROR] TypeCell::set failed: m_ptr or m_type is null\n");
        return;
    }
    // Печатаем адрес в HEX (0x8000), название типа и значение
    fmt::print("TypeCell::set: [addr: {:p}] [type: {:>8}] [val: {}]\n", m_ptr,
               m_type->get_runtime_name(), val.print());
    // 1. Используем официальную точку входа.
    // Она сама разберется: примитив это, enum или bitfield.
    StaticBufferWriter::write_value_at_ptr(m_ptr, m_type, val);

    // 2. Уведомление владельца.
    // Если есть публичный метод в StaticBuffer для этого — используй его.
    // Если нет, и ты добавил friend class TypeCell в StaticBuffer, то:
    if (m_owner && m_key.is_integer()) {
        if (auto *buffer = dynamic_cast<StaticBuffer *>(m_owner.get())) {
            // fmt::print("TypeCell::set: update_addr_range [start: {:08X}] [size: {:08X}]]\n",
            //            m_key.as_integer(), m_type->get_size_in_memory());
            //  Вызываем метод уведомления
            buffer->update_addr_range(static_cast<uint32_t>(m_key.as_integer()),
                                      static_cast<uint32_t>(m_type->get_size_in_memory()));
        }
    }
}

Object TypeCell::make_step_accessor(const Object &key) {
    if (!m_type || !m_ptr)
        return Object::make_undefined();

    // 1. Используем m_ptr напрямую (физика уже тут)
    uint8_t *current_ptr = static_cast<uint8_t *>(m_ptr);

    Type       *next_type = nullptr;
    std::string next_path = m_path;
    size_t      offset_delta = 0;

    // --- ЛОГИКА СМЕЩЕНИЯ ---
    if (key.is_integer()) {
        int    index = key.as_integer();
        size_t stride = m_type->get_size_in_memory();
        int    alignment = m_type->get_inline_array_stride_alignment();
        if (alignment > 1) {
            stride = (stride + alignment - 1) & ~(alignment - 1);
        }

        offset_delta = index * stride;
        next_type = m_type; // Для массивов тип элемента тот же
        next_path += fmt::format("[{}]", index);
    } else if (key.is_symbol()) {
        auto *struct_type = dynamic_cast<StructureType *>(m_type);
        if (struct_type) {
            Field       field;
            std::string field_name = key.as_symbol();
            if (struct_type->lookup_field(field_name, &field)) {
                offset_delta = field.offset();
                next_type = TypeSystem::instance().lookup_type(field.type());
                next_path = m_path.empty() ? field_name : m_path + "." + field_name;
            }
        }
    }

    // --- СОЗДАНИЕ НОВОЙ ЯЧЕЙКИ ---
    if (next_type) {
        // Вычисляем новый физический адрес
        void *next_ptr = current_ptr + offset_delta;

        // Вычисляем новый логический ключ (оффсет в буфере)
        Object next_key = m_key;
        if (m_key.is_integer()) {
            next_key = Object::make_integer(m_key.as_integer() + offset_delta);
        }

        // Создаем ячейку: передаем новый PTR и сохраняем OWNER
        auto next_cell = std::make_shared<TypeCell>(next_ptr,  // Физика (адрес в памяти)
                                                    next_type, // Тип
                                                    m_owner,   // Контекст (владелец-буфер)
                                                    next_key,  // Оффсет для магии Intel HEX
                                                    next_path  // Путь для отладки
        );

        return Object::make_heap_object(next_cell, ObjectType::CELL);
    }

    return Object::make_undefined();
}

std::string TypeCell::print() const {
    void *ptr = resolve_ptr();
    return fmt::format("#<type-cell {} @ {:p}>",
                       m_path.empty() ? (m_type ? m_type->get_name() : "null") : m_path, ptr);
}

Object TypeCell::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("type-cell"));
    lb.push_kv("path", Object::make_string(m_path));
    lb.push_kv("key", m_key);
    lb.push_kv("address", Object::make_integer((uintptr_t)resolve_ptr()));
    if (m_type)
        lb.push_kv("type", Object::make_symbol(m_type->get_name()));

    return lb.finalize();
}

void *TypeCell::resolve_ptr() const {
    if (!m_owner)
        return nullptr;

    // Если владелец — буфер, то ключ — это оффсет
    if (auto *buffer = dynamic_cast<StaticBuffer *>(m_owner.get())) {
        return buffer->data() + m_key.as_integer();
    }

    // Если владелец — другой объект, тут будет другая логика
    return nullptr;
}
} // namespace script
