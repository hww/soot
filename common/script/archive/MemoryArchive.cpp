#include "MemoryArchive.hpp"
namespace soot {

Object MemoryArchive::get_at(SymbolTable* st, const Object &key) {
    (void)st;
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Доступ к компонентам
        if (name == ":region")
            return Object::make_heap_obj(m_region);
        if (name == ":position")
            return Object::make_integer(m_position);

        return Object::make_null();
    }

    throw std::runtime_error(fmt::format("MemoryArchive: unknown key {}", key.print()));
}

void MemoryArchive::set_at(SymbolTable* st, const Object &key, const Object &value) {
    (void)st;
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Доступ к компонентам
        if (name == ":position") {
            if (value.is_integer())
                m_position = value.as_integer();
            else
                throw std::runtime_error("MemoryArchive set position except number");
            return;
        }
    }

    throw std::runtime_error(fmt::format("MemoryArchive: unknown key {}", key.print()));
}
} // namespace soot
