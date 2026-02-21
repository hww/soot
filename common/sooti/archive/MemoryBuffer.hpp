#pragma once

#include "LabelTable.hpp"
#include "MemoryArchive.hpp"
#include "MemoryRegion.hpp"
#include "MemorySymbolTable.hpp"
#include "RelocationTable.hpp"
#include "common/sooti/Archive.hpp"
#include "common/type_system/Type.hpp"
#include <unordered_map>
#include <vector>

namespace script {

/**
 * @brief Буфер памяти с метаданными
 *
 * Объединяет:
 * - MemoryRegion - сырые данные
 * - MemorySymbolTable - таблица символов
 * - RelocationTable - релокации
 *
 * Вся сериализация только через Archive!
 */

/**
 * @brief MemoryBuffer - просто контейнер, который ничего не умеет,
 *        только хранит компоненты и дает к ним доступ
 */
class MemoryBuffer : public NativeObject {
  private:
    std::shared_ptr<MemoryRegion>      m_region;
    std::shared_ptr<MemorySymbolTable> m_symbols; // если нужны
    std::shared_ptr<RelocationTable>   m_relocs;  // если нужны
    std::shared_ptr<LabelTable>        m_labels;  // если нужны

  public:
    MemoryBuffer() {}
    MemoryBuffer(std::shared_ptr<MemoryRegion> region) : m_region(region) {

        m_symbols = std::make_shared<MemorySymbolTable>();
        m_relocs = std::make_shared<RelocationTable>();
        m_labels = std::make_shared<LabelTable>();
    }
    ~MemoryBuffer() {}
    // Только геттеры и сеттеры компонентов
    void set_symbols(std::shared_ptr<MemorySymbolTable> s) {
        m_symbols = s;
    }
    void set_relocs(std::shared_ptr<RelocationTable> r) {
        m_relocs = r;
    }
    void set_labels(std::shared_ptr<LabelTable> l) {
        m_labels = l;
    }

    auto region() const {
        return m_region;
    }
    auto symbols() const {
        return m_symbols;
    }
    auto relocs() const {
        return m_relocs;
    }
    auto labels() const {
        return m_labels;
    }

    void serialize(Archive &ar) override {
        if (ar.is_loading()) {
            m_region = std::make_shared<MemoryRegion>();
            m_symbols = std::make_shared<MemorySymbolTable>();
            m_relocs = std::make_shared<RelocationTable>();
            m_labels = std::make_shared<LabelTable>();
        }

        m_region->serialize(ar);
        m_symbols->serialize(ar);
        m_relocs->serialize(ar);
        m_labels->serialize(ar);
    }

    std::string hex_dump(size_t start_offset, size_t bytes_to_dump, bool show_ascii = true,
                         size_t bytes_per_line = 16, int origin = 0) const {
        m_region->hex_dump(start_offset, bytes_to_dump, show_ascii, bytes_per_line, origin);
    }
    // ============================================================
    // MativeObject implementation
    // ============================================================

    Object get_at(const Object &key) override;

    // Object interface
    std::string class_name() const override {
        return "memory-buffer";
    }
    std::string full_class_name() const override {
        return "MemoryBuffer";
    }

    std::string print() const override {
        return fmt::format("#<memory-buffer {:04x}-{:04x} size={}>", m_region->base(),
                           m_region->base() + m_region->size(), m_region->size());
    }

    Object inspect() const override {
        return pretty_print::build_list(
            pretty_print::build_list(Object::make_symbol(":type"),
                                     Object::make_string(class_name())),
            pretty_print::build_list(Object::make_symbol(":base"),
                                     Object::make_integer(m_region->base())),
            pretty_print::build_list(Object::make_symbol(":size"),
                                     Object::make_integer(m_region->size())));
    }
};
} // namespace script