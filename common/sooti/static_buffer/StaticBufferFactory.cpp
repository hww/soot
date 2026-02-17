#include "StaticBufferFactory.hpp"
#include <cstring>

namespace script {

/**
 * @brief Основной метод инициализации памяти на основе метаданных типа.
 * * Метод подготавливает "черновик" объекта в статическом буфере: зануляет память,
 * прописывает системные заголовки (type tags) и рекурсивно инициализирует
 * встроенные (inline) структуры.
 * * @param ts Указатель на систему типов для поиска определений полей.
 * @param dest Целевой буфер, в котором выделяется память.
 * @param offset Смещение от начала буфера.
 * @param type Указатель на объект типа (Structure, Basic, Value и т.д.).
 * @return size_t Общий размер инициализированной памяти в байтах.
 */
size_t StaticBufferFactory::write_from_type(TypeSystem *ts, StaticBuffer *dest, size_t offset,
                                            Type *type) {
    if (!type || !dest)
        return 0;

    // Получаем размер типа, необходимый для аллокации в памяти
    size_t size = type->get_size_in_memory();

    // Проверка на выход за границы буфера (реализуется внутри Utils/Factory)
    // check_bounds(dest, offset, size, "write_from_type", type->get_name());

    // 1. Базовое зануление.
    // Это автоматически инициализирует все ValueType (int, float) нулевыми значениями.
    std::memset(dest->data() + offset, 0, size);

    // 2. Инициализация Runtime Type Tag.
    // В GOAL типы, наследуемые от 'basic', хранят CRC32 своего имени в первом слове (offset 0).
    if (type->class_name() == "basic-type") {
        uint32_t type_tag = type->get_type_tag();
        dest->write_crc32(offset, type_tag, TypeConfig::crc_value_size);

        // Поле heap-base (offset 4) используется в некоторых системных структурах
        // для указания базового смещения в куче.
        if (type->heap_base() != 0) {
            dest->write_u32(offset + 4, type->heap_base());
        }
    }

    // 3. Рекурсивная инициализация полей структур.
    if (auto *struct_type = dynamic_cast<StructureType *>(type)) {
        for (const auto &field : struct_type->fields()) {
            // Ищем определение типа поля в системе типов
            Type *field_type = ts->lookup_type(field.type());
            if (!field_type)
                continue;

            // Вычисляем абсолютное смещение поля внутри буфера
            size_t absolute_field_offset = offset + field.offset();

            if (field.is_array()) {
                // Обработка массивов (как встроенных, так и ссылочных)
                write_array_field(ts, dest, absolute_field_offset, field, field_type);
            } else if (field.is_inline() && field_type->is_reference()) {
                // Если структура помечена как 'inline', она является частью текущей памяти.
                // Рекурсивно вызываем инициализацию для прописи её внутренних тегов.
                write_from_type(ts, dest, absolute_field_offset, field_type);
            }
        }
    }

    return size;
}

/**
 * @brief Инициализация полей, являющихся массивами.
 * * Если массив содержит структуры (особенно basic), каждый элемент должен быть
 * инициализирован индивидуально (прописаны теги типов).
 */
void StaticBufferFactory::write_array_field(TypeSystem *ts, StaticBuffer *dest, size_t field_offset,
                                            const Field &field, Type *element_type) {
    // Динамические массивы (размер которых определяется в рантайме) не инициализируются статически.
    if (field.is_dynamic())
        return;

    int array_size = field.array_size();
    int stride = element_type->get_size_in_memory();

    // Для массивов ссылочных типов (структур) в GOAL часто применяется
    // специфическое выравнивание шага (stride alignment).
    if (element_type->is_reference()) {
        stride = align_up(stride, element_type->get_inline_array_stride_alignment());
    }

    for (int i = 0; i < array_size; ++i) {
        size_t element_offset = field_offset + (i * stride);

        // Если элементы массива — ссылочные типы (структуры), инициализируем их.
        // Примитивные типы (числа) уже занулены общим memset в вызывающем методе.
        if (element_type->is_reference()) {
            write_from_type(ts, dest, element_offset, element_type);
        }
    }
}

/**
 * @brief Вспомогательная функция для выравнивания адреса/размера вверх.
 * * @param value Текущее значение.
 * @param alignment Выравнивание (должно быть степенью двойки).
 * @return size_t Выровненное значение.
 */
inline size_t StaticBufferFactory::align_up(size_t value, size_t alignment) {
    if (alignment == 0)
        return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace script