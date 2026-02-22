// MemoryRegion.hpp
#pragma once

#include "common/sooti/Archive.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include <cstdint>
#include <vector>

namespace script {

/**
 * @brief Простой регион памяти - только байты и базовый адрес
 *
 * Никаких меток, релокаций, типов - только голая память.
 */
/**
 * @brief Простой регион памяти - только байты и базовый адрес
 *
 * Никаких меток, релокаций, типов - только голая память.
 */
class MemoryRegion : public NativeObject {
  private:
    std::vector<uint8_t> m_data;
    uint32_t             m_base_address; // виртуальный базовый адрес (origin)

  public:
    MemoryRegion() : m_data(0, 0), m_base_address(0) {}
    MemoryRegion(size_t size, uint32_t base = 0) : m_data(size, 0), m_base_address(base) {}

    // Доступ к данным (как было)
    uint8_t *data() {
        return m_data.data();
    }
    const uint8_t *data() const {
        return m_data.data();
    }
    size_t size() const {
        return m_data.size();
    }
    uint32_t base() const {
        return m_base_address;
    }

    // Проверка границ (как было)
    bool contains(size_t offset) const {
        return offset < m_data.size();
    }
    bool contains_address(uint32_t addr) const {
        return addr >= m_base_address && addr < m_base_address + m_data.size();
    }

    // Преобразование адрес <-> смещение (как было)
    size_t address_to_offset(uint32_t addr) const {
        if (!contains_address(addr))
            return (size_t)-1;
        return addr - m_base_address;
    }

    uint32_t offset_to_address(size_t offset) const {
        if (!contains(offset))
            return 0;
        return m_base_address + offset;
    }

    // ============================================================
    // Сериализация
    // ============================================================

    void serialize(Archive &ar) override {
        if (ar.is_reading()) {
            // Чтение
            CompactIndex ver;
            ar << ver; // версия формата

            CompactIndex sz;
            ar << sz; // размер данных

            uint32_t base;
            ar << base; // базовый адрес

            m_base_address = base;
            m_data.resize(sz.value);

            // Читаем сами данные
            ar.serialize(m_data.data(), sz.value);

        } else {
            // Запись
            CompactIndex version(1);
            ar << version; // версия формата

            CompactIndex size(m_data.size());
            ar << size; // размер данных

            ar << m_base_address; // базовый адрес

            // Пишем сами данные
            ar.serialize(m_data.data(), m_data.size());
        }
    }

    // ============================================================
    // NativeObject implementation
    // ============================================================

    Object get_at(const Object &key) override;
    void   set_at(const Object &key, const Object &value) override;

    // Object interface
    std::string class_name() const override {
        return "memory-region";
    }
    std::string full_class_name() const override {
        return "MemoryRegion";
    }

    std::string print() const override {
        return fmt::format("#<memory-region {:04x}-{:04x} size={}>", m_base_address,
                           m_base_address + m_data.size(), m_data.size());
    }

    Object inspect() const override {
        return pretty_print::build_list(
            pretty_print::build_list(Object::make_symbol(":type"),
                                     Object::make_string(class_name())),
            pretty_print::build_list(Object::make_symbol(":base"),
                                     Object::make_integer(m_base_address)),
            pretty_print::build_list(Object::make_symbol(":size"),
                                     Object::make_integer(m_data.size())));
    }

    // Операторы для Archive
    inline friend Archive &operator<<(Archive &ar, MemoryRegion &v) {
        v.serialize(ar);
        return ar;
    }

    inline friend Archive &operator<<(Archive &ar, const std::shared_ptr<MemoryRegion> &ptr) {
        if (ptr) {
            ptr->serialize(ar);
        } else {
            // Сериализуем nullptr как специальный маркер
            CompactCrc32 null_magic{0x00000000};
            ar << null_magic;
        }
        return ar;
    }

    /*!
     * Dump buffer to string
     */
    std::string hex_dump(size_t start_offset, size_t bytes_to_dump, bool show_ascii = true,
                         size_t bytes_per_line = 16, int origin = 0) const {
        if (bytes_to_dump == 0) {
            bytes_to_dump = m_data.size() - start_offset;
        }

        if (start_offset >= m_data.size()) {
            return fmt::format("Offset {} exceeds buffer size {}", start_offset, m_data.size());
        }

        size_t end_offset = std::min(start_offset + bytes_to_dump, m_data.size());

        std::string result = "";
        result += fmt::format("Buffer: ({} bytes, origin: {:#x}):\n", m_data.size(), origin);

        for (size_t offset = start_offset; offset < end_offset; offset += bytes_per_line) {
            size_t line_end = std::min(offset + bytes_per_line, end_offset);

            // Адрес
            result += fmt::format("{:08x}: ", offset + origin);

            // Hex байты
            for (size_t i = offset; i < line_end; i++) {
                result += fmt::format("{:02x} ", m_data[i]);
            }

            // Заполнение для выравнивания
            for (size_t i = line_end; i < offset + bytes_per_line; i++) {
                result += "   ";
            }

            // ASCII представление (опционально)
            if (show_ascii) {
                result += " |";
                for (size_t i = offset; i < line_end; i++) {
                    uint8_t byte = m_data[i];
                    if (byte >= 32 && byte < 127) {
                        result += static_cast<char>(byte);
                    } else {
                        result += '.';
                    }
                }
                result += '|';
            }
            result += '\n';
        }

        return result;
    }

    bool export_intel_hex_file(const std::string &path, size_t start_offset, size_t end_offset,
                               bool append) const;

  private:
};

} // namespace script