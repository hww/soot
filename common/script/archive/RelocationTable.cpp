#include "RelocationTable.hpp"

namespace soot {
Object RelocationTable::get_at(SymbolTable* st, const Object &key) {
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        if (name == ":size" || name == "size" || name == ":count")
            return Object::make_integer(m_relocations.size());
        if (name == ":type")
            return Object::make_string(class_name());

        // Получить все релокации
        if (name == ":all" || name == "all") {
            std::vector<Object> reloc_list;
            for (const auto &reloc : m_relocations) {
                reloc_list.push_back(pretty_print::build_list(
                    pretty_print::build_list(Object::make_keyword(st, "offset"),
                                             Object::make_integer(reloc.offset)),
                    pretty_print::build_list(Object::make_keyword(st, "type"),
                                             Object::make_integer(static_cast<int>(reloc.type))),
                    pretty_print::build_list(Object::make_keyword(st, "target"),
                                             Object::make_string(reloc.target_name))));
            }
            return Object::make_list(reloc_list);
        }

        return Object::make_null();
    }

    // Доступ по индексу
    if (key.is_integer()) {
        size_t idx = key.as_integer();
        if (idx < m_relocations.size()) {
            const auto &reloc = m_relocations[idx];
            return pretty_print::build_list(
                pretty_print::build_list(Object::make_keyword(st, "offset"),
                                         Object::make_integer(reloc.offset)),
                pretty_print::build_list(Object::make_keyword(st, "type"),
                                         Object::make_integer(static_cast<int>(reloc.type))),
                pretty_print::build_list(Object::make_keyword(st, "target"),
                                         Object::make_string(reloc.target_name)));
        }
    }

    throw std::runtime_error(fmt::format("RelocationTable: unknown key {}", key.print()));
}

void RelocationTable::set_at(SymbolTable* st, const Object &key, const Object &value) {
    (void)st;

    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства только для чтения
        if (name == ":size" || name == "size" || name == ":count" || name == ":type" ||
            name == ":all") {
            throw std::runtime_error("RelocationTable::set_at: cannot set read-only property: " +
                                     name);
        }

        // Добавление релокации через property list
        // Ожидаемый формат: (:offset 123 :type 'abs :target "label")
        if (value.is_pair()) {
            size_t      offset = 0;
            RelocType   type = RelocType::ABS_ADDR;
            std::string target;

            // Парсим property list
            Object current = value;
            while (current.is_pair()) {
                Object entry = current.as_pair()->car;
                if (entry.is_pair()) {
                    Object prop = entry.as_pair()->car;
                    Object val = entry.as_pair()->cdr;

                    if (prop.is_keyword()) {
                        std::string prop_name = prop.to_std_string();

                        if (prop_name == ":offset") {
                            if (val.is_pair() && val.as_pair()->car.is_number()) {
                                offset = val.as_pair()->car.as_integer();
                            }
                        } else if (prop_name == ":type") {
                            if (val.is_pair()) {
                                Object type_val = val.as_pair()->car;
                                if (type_val.is_keyword() || type_val.is_symbol()) {
                                    std::string type_str = type_val.to_std_string();
                                    if (type_str == ":abs" || type_str == "abs") {
                                        type = RelocType::ABS_ADDR;
                                    } else if (type_str == ":rel" || type_str == "rel") {
                                        type = RelocType::RELATIVES;
                                    } else if (type_str == ":crc" || type_str == "crc") {
                                        type = RelocType::SYMBOL_CRC;
                                    } else if (type_str == ":table" || type_str == "table") {
                                        type = RelocType::SYMBOL_TABLE_REF;
                                    }
                                }
                            }
                        } else if (prop_name == ":target") {
                            if (val.is_pair()) {
                                Object target_val = val.as_pair()->car;
                                if (target_val.is_string() || target_val.is_symbol()) {
                                    target = target_val.to_std_string();
                                }
                            }
                        }
                    }
                }
                current = current.as_pair()->cdr;
            }

            if (!target.empty()) {
                add(offset, type, target);
                return;
            }
        }

        throw std::runtime_error(
            "RelocationTable::set_at: expected property list with :offset, :type, :target");
    }

    // Доступ по индексу - только для чтения
    if (key.is_integer()) {
        throw std::runtime_error("RelocationTable::set_at: cannot set by index");
    }

    throw std::runtime_error(fmt::format("RelocationTable::set_at: invalid key {}", key.print()));
}
} // namespace soot