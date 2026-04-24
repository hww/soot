#include "MemorySymbolTable.hpp"

namespace soot {

Object MemorySymbolTable::get_at(const Object &key) {
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства
        if (name == ":size" || name == "size" || name == ":count")
            return Object::make_integer(m_symbols.size());
        if (name == ":type")
            return Object::make_string(class_name());

        // Доступ по имени символа
        auto idx = find_by_name(name);
        if (idx) {
            // Возвращаем информацию о символе
            return pretty_print::build_list(
                pretty_print::build_list(Object::make_keyword(m_st, "name"),
                                         Object::make_string(get_name(*idx))),
                pretty_print::build_list(Object::make_keyword(m_st, "crc32"),
                                         Object::make_integer(get_crc32(*idx))),
                pretty_print::build_list(Object::make_keyword(m_st, "index"),
                                         Object::make_integer(*idx)));
        }

        // Доступ по индексу (число)
        if (key.is_integer()) {
            size_t idx = key.as_integer();
            if (idx < m_symbols.size()) {
                return pretty_print::build_list(
                    pretty_print::build_list(Object::make_keyword(m_st, "name"),
                                             Object::make_string(m_symbols[idx].name)),
                    pretty_print::build_list(Object::make_keyword(m_st, "crc32"),
                                             Object::make_integer(m_symbols[idx].crc32)));
            }
        }

        return Object::make_null();
    }

    throw std::runtime_error(fmt::format("MemorySymbolTable: unknown key {}", key.print()));
}

void MemorySymbolTable::set_at(const Object &key, const Object &value) {
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства только для чтения
        if (name == ":size" || name == "size" || name == ":count" || name == ":type" ||
            name == ":names" || name == ":crcs") {
            throw std::runtime_error("MemorySymbolTable::set_at: cannot set read-only property: " +
                                     name);
        }

        // Добавление символа
        if (value.is_true()) {
            // true - добавляем символ
            add_symbol(name);
        } else if (value.is_number()) {
            // Число - можно использовать как особый случай?
            // Пока просто добавляем символ
            add_symbol(name);
        } else {
            throw std::runtime_error("MemorySymbolTable::set_at: expected true to add symbol");
        }
        return;
    }

    // Доступ по индексу - только для чтения
    if (key.is_integer()) {
        throw std::runtime_error("MemorySymbolTable::set_at: cannot set by index");
    }

    throw std::runtime_error(fmt::format("MemorySymbolTable::set_at: invalid key {}", key.print()));
}
} // namespace soot
