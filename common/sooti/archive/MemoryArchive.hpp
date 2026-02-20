// MemoryArchive.hpp
#pragma once

#include "Archive.hpp"
#include "MemoryRegion.hpp"
#include <stack>

namespace script {

/**
 * @brief Архиватор для работы с MemoryRegion
 *
 * Реализует виртуальные методы Archive для чтения/записи в память.
 * Дополнительно поддерживает стек объектов для рекурсивной сериализации.
 */
class MemoryArchive : public Archive {
  private:
    std::shared_ptr<MemoryRegion>           m_region;
    size_t                                  m_position;
    std::stack<std::shared_ptr<HeapObject>> m_object_stack;

  public:
    /**
     * @param region регион памяти
     * @param loading true - чтение, false - запись
     * @param version версия формата (по умолчанию 0)
     */
    MemoryArchive(std::shared_ptr<MemoryRegion> region, bool loading, int version = 0)
        : m_region(region), m_position(0) {
        m_version = version;
        m_is_loading = loading;
        m_is_saving = !loading;
        m_is_persistent = true; // архивируем в постоянное хранилище
        m_is_error = false;

        // Определяем порядок байт хоста
        union {
            uint32_t i;
            char     c[4];
        } test = {0x01020304};
        is_big_endian = (test.c[0] == 1);
    }

    static MemoryArchive for_writing(std::shared_ptr<MemoryRegion> region, int version = 0) {
        MemoryArchive ar(region, false, version);
        return ar;
    }

    static MemoryArchive for_reading(std::shared_ptr<MemoryRegion> region, int version = 0) {
        MemoryArchive ar(region, true, version);
        return ar;
    }

    // ------------------------------------------------------------
    // Archive interface
    // ------------------------------------------------------------

    void serialize(void *v, int length) override {
        if (!v || length <= 0)
            return;

        // Проверка границ
        if (m_position + length > m_region->size()) {
            m_is_error = true;
            throw std::runtime_error("MemoryArchive: out of bounds");
        }

        uint8_t *dest = m_region->data() + m_position;

        if (m_is_loading) {
            // Чтение из памяти
            memcpy(v, dest, length);
        } else {
            // Запись в память
            memcpy(dest, v, length);
        }

        m_position += length;
    }

    int tell() override {
        return m_position;
    }

    int total_size() override {
        return m_region->size();
    }

    bool at_end() override {
        return m_position >= m_region->size();
    }

    void seek(int pos) override {
        if (pos < 0 || pos > m_region->size()) {
            m_is_error = true;
            throw std::runtime_error("MemoryArchive: seek out of bounds");
        }
        m_position = pos;
    }

    void precache(int hint_count) override {
        (void)hint_count;
        // Для памяти не нужно
    }

    void flush() override {
        // Для памяти не нужно
    }

    bool close() override {
        return true;
    }

    bool get_error() override {
        return m_is_error;
    }
};

} // namespace script