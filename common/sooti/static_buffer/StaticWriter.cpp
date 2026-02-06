#include "StaticWriter.hpp"
#include "TypePointer.hpp"
#include "common/type_system/TypeSystem.hpp"

namespace script {

/**
 * Основной метод: "Зарезервировать" место под тип и вернуть ячейку для записи.
 */
Object StaticWriter::allocate(const std::string &type_name) {
    if (!m_buffer)
        return Object::make_undefined();

    Type *type = TypeSystem::instance().lookup_type(type_name);
    if (!type)
        return Object::make_undefined();

    // 1. Выравниваем курсор под требования типа
    size_t alignment = type->get_in_memory_alignment();
    m_position = (m_position + alignment - 1) & ~(alignment - 1);

    // 2. Проверяем границы
    size_t size = type->get_size_in_memory();
    if (m_position + size > m_buffer->size()) {
        throw std::runtime_error("StaticBufferWriter: Buffer overflow");
    }

    // 3. Получаем адрес в памяти
    void *ptr = m_buffer->data() + m_position;

    // 4. Создаем TypePointer на это место
    auto cell = std::make_shared<TypePointer>(ptr, type);

    // 5. Двигаем курсор вперед
    m_position += size;

    return Object::make_heap_object(cell, ObjectType::POINTER);
}

StaticWriter::StaticWriter(const std::shared_ptr<StaticBuffer> &buffer)
    : m_position(0), m_buffer(buffer) {}
} // namespace script