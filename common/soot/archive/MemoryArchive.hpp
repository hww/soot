// MemoryArchive.hpp
#pragma once

#include "MemoryRegion.hpp"
#include "common/soot/Archive.hpp"
#include <stack>

namespace soot {

/**
 * @brief Архиватор для работы с MemoryRegion
 *
 * Реализует виртуальные методы Archive для чтения/записи в память.
 * Дополнительно поддерживает стек объектов для рекурсивной сериализации.
 */
class MemoryArchive : public Archive {
  private:
    std::shared_ptr<MemoryRegion> m_region;
    size_t                        m_position;

  public:
    const int VERSION = 1;
    /**
     * @param region регион памяти
     * @param loading true - чтение, false - запись
     * @param version версия формата (по умолчанию 0)
     */
    MemoryArchive(std::shared_ptr<MemoryRegion> region, bool loading, bool saving, int persistant)
        : m_region(region), m_position(0) {
        m_version = VERSION;
        m_is_reading = loading;
        m_is_writing = saving;
        m_is_persistent = persistant; // архивируем в постоянное хранилище
        m_is_error = false;

        // Определяем порядок байт хоста
        union {
            uint32_t i;
            char     c[4];
        } test = {0x01020304};
        is_big_endian = (test.c[0] == 1);
    }

    static MemoryArchive for_writing(std::shared_ptr<MemoryRegion> region,
                                     bool                          persistant = false) {
        MemoryArchive ar(region, false, true, persistant);
        return ar;
    }

    static MemoryArchive for_reading(std::shared_ptr<MemoryRegion> region,
                                     bool                          persistant = false) {
        MemoryArchive ar(region, true, false, persistant);
        return ar;
    }

    Object get_at(const Object &key) override;

    void set_at(const Object &key, const Object &value) override;

    std::string class_name() const override {
        return "memory-archive";
    }

    std::string full_class_name() const override {
        return "MemoryArchive";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == MemoryArchive::type_name_obj() || Archive::is_class_name(name);
    }

    std::string print() const override {
        return fmt::format("#<memory-archive>");
    }

    Object inspect() const override {
        return pretty_print::build_list(pretty_print::build_list(
            Object::make_symbol(":type"), Object::make_string(class_name())));
    }

    // ------------------------------------------------------------
    // Archive interface
    // ------------------------------------------------------------

    void serialize_obj(void *v, int length) override {
        if (!v || length <= 0)
            return;

        // Проверка границ
        if (m_position + length > m_region->size()) {
            m_is_error = true;
            throw std::runtime_error("MemoryArchive: out of bounds");
        }

        uint8_t *dest = m_region->data() + m_position;

        if (m_is_reading) {
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

    void seek(size_t pos) override {
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

} // namespace soot