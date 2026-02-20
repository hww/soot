#include "StaticBufferBlitter.hpp"

namespace script {
/*!
 * @brief Копирует содержимое одного статического буфера в другой.
 * * @param src Исходный буфер (источник данных, релокаций и символов).
 * @param dest Целевой буфер (куда пишем).
 * @param dest_offset Смещение в целевом буфере, куда начнется запись.
 * @param src_offset Начало области копирования в источнике.
 * @param size Количество байт для копирования.
 */
void StaticBufferBlitter::blit(StaticBuffer *src, StaticBuffer *dest, size_t dest_offset,
                               size_t src_offset, size_t size) {

    if (!src || !dest) {
        throw std::runtime_error("Blitter: Source or Destination is null");
    }

    // 1. Валидация границ
    if (src_offset + size > src->size()) {
        throw std::runtime_error("Blitter: Source overflow");
    }
    if (dest_offset + size > dest->size()) {
        throw std::runtime_error("Blitter: Destination overflow");
    }

    // 2. Копирование сырых данных
    std::memcpy(dest->data() + dest_offset, src->data() + src_offset, size);

    // 3. Перенос и фильтрация релокаций
    // Мы переносим только те релокации, которые попали в диапазон копирования
    for (const auto &reloc : src->get_relocations()) {
        if (reloc.offset >= src_offset && reloc.offset < src_offset + size) {

            // Вычисляем новое смещение релокации в целевом буфере
            size_t relative_offset = reloc.offset - src_offset;
            size_t new_total_offset = dest_offset + relative_offset;

            // Если это релокация символа, нужно убедиться, что символ есть в целевой таблице
            if (reloc.type == RelocType::SYMBOL_CRC || reloc.type == RelocType::SYMBOL_TABLE_REF) {
                // Импортируем символ в целевой буфер (если его нет, он добавится)
                dest->add_symbol(reloc.target_name);
            }

            // Добавляем релокацию в целевой буфер с поправленным смещением
            dest->add_reloc(new_total_offset, reloc.type, reloc.target_name);
        }
    }
}

/*!
 * @brief Копирует буфер целиком.
 */
void StaticBufferBlitter::blit_full(StaticBuffer *src, StaticBuffer *dest, size_t dest_offset) {
    blit(src, dest, dest_offset, 0, src->size());
}

} // namespace script