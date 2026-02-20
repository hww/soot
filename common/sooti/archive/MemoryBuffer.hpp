#pragma once

#include "Archive.hpp"
#include "LabelTable.hpp"
#include "MemoryArchive.hpp"
#include "MemoryRegion.hpp"
#include "MemorySymbolTable.hpp"
#include "RelocationTable.hpp"
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
class MemoryBuffer : public HeapObject {
  private:
    std::shared_ptr<MemoryRegion>      m_region;
    std::shared_ptr<MemorySymbolTable> m_symbols; // если нужны
    std::shared_ptr<RelocationTable>   m_relocs;  // если нужны
    std::shared_ptr<LabelTable>        m_labels;  // если нужны

  public:
    MemoryBuffer(std::shared_ptr<MemoryRegion> region) : m_region(region) {}

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

    // Сериализация - просто делегирует компонентам
    void serialize(Archive &ar) override {
        if (ar.is_loading()) {
            // При чтении создаем новые компоненты
            m_region = std::make_shared<MemoryRegion>();
            m_symbols = std::make_shared<MemorySymbolTable>();
            m_relocs = std::make_shared<RelocationTable>();
            m_labels = std::make_shared<LabelTable>();
        }

        // Явно вызываем serialize для каждого
        m_region->serialize(ar);
        m_symbols->serialize(ar);
        m_relocs->serialize(ar);
        m_labels->serialize(ar);
    }
};
} // namespace script