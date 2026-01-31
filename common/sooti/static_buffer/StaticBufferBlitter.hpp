#include "StaticBuffer.hpp"
#include <stdexcept>

namespace script {

class StaticBufferBlitter {
public:
    /**
     * @brief Копирует содержимое одного статического буфера в другой.
     * * @param src Исходный буфер (источник данных, релокаций и символов).
     * @param dest Целевой буфер (куда пишем).
     * @param dest_offset Смещение в целевом буфере, куда начнется запись.
     * @param src_offset Начало области копирования в источнике.
     * @param size Количество байт для копирования.
     */
    static void blit(StaticBuffer* src, StaticBuffer* dest, 
                    size_t dest_offset, size_t src_offset, size_t size) ;

    /**
     * @brief Копирует буфер целиком.
     */
    static void blit_full(StaticBuffer* src, StaticBuffer* dest, size_t dest_offset) ;
};

} // namespace script