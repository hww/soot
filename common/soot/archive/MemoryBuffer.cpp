#include "MemoryBuffer.hpp"

namespace soot {
Object MemoryBuffer::get_at(SymbolTable* st, const Object &key) {
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Доступ к компонентам
        if (name == ":region" || name == "region")
            return Object::make_heap_obj(m_region);
        if (name == ":symbols" || name == "symbols")
            return m_symbols ? Object::make_heap_obj(m_symbols) : Object::make_null();
        if (name == ":relocs" || name == "relocs")
            return m_relocs ? Object::make_heap_obj(m_relocs) : Object::make_null();
        if (name == ":labels" || name == "labels")
            return m_labels ? Object::make_heap_obj(m_labels) : Object::make_null();

        return Object::make_null();
    }

    // Делегируем региону для доступа по смещению
    if (m_region) {
        return m_region->get_at(st, key);
    }

    throw std::runtime_error(fmt::format("MemoryBuffer: unknown key {}", key.print()));
}
} // namespace soot
