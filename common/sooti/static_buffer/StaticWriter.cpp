#include "StaticWriter.hpp"
#include "TypePointer.hpp"

namespace script {

/**
 * Основной метод: "Зарезервировать" место под тип и вернуть ячейку для записи.
 */
Object StaticWriter::allocate(Type *type) {
    if (!m_buffer) {
        throw std::runtime_error("StaticBufferWriter: Unitialized buffer");
        return Object::make_none();
    }

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
    // Инициализируем
    std::memset(ptr, 0, size);

    // 4. Создаем TypePointer на это место
    auto pointer = std::make_shared<TypePointer>(ptr, type, m_buffer);

    // 5. Двигаем курсор вперед
    m_position += size;
    // 6. Возвращаем
    return Object::make_pointer(pointer);
}

StaticWriter::StaticWriter(const std::shared_ptr<StaticBuffer> &buffer)
    : m_position(0), m_buffer(buffer) {}
} // namespace script