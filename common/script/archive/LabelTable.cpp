#include "LabelTable.hpp"

namespace soot {
Object LabelTable::get_at(SymbolTable* st, const Object &key) {
    (void)st;
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства
        if (name == ":size" || name == "size" || name == ":count")
            return Object::make_integer(m_labels.size());

        if (name == ":type")
            return Object::make_string(class_name());

        if (name == ":names" || name == "names") {
            std::vector<Object> names;
            for (const auto &[label_name, _] : m_labels) {
                names.push_back(Object::make_string(label_name));
            }
            return Object::make_list(names);
        }

        if (name == ":offsets" || name == "offsets") {
            std::vector<Object> offsets;
            for (const auto &[_, label] : m_labels) {
                offsets.push_back(Object::make_integer(label->offset));
            }
            return Object::make_list(offsets);
        }

        if (name == ":labels" || name == "labels") {
            std::vector<Object> labels;
            for (const auto &[_, label] : m_labels) {
                labels.push_back(Object::make_heap_obj(label));
            }
            return Object::make_list(labels);
        }

        if (name == ":table" || name == "table") {
            Object table = Object::make_hash_table();
            for (const auto &[label_name, label] : m_labels) {
                table.as_hash_table()->set(label_name, Object::make_heap_obj(label));
            }
            return table;
        }

        // Доступ по имени метки - возвращаем объект MemoryLabel
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return Object::make_heap_obj(it->second);
        }

        return Object::make_null();
    }

    // Доступ по индексу
    if (key.is_integer()) {
        size_t idx = key.as_integer();
        if (idx < m_labels.size()) {
            auto it = m_labels.begin();
            std::advance(it, idx);

            // Возвращаем объект MemoryLabel
            return Object::make_heap_obj(it->second);
        }
    }

    throw std::runtime_error(fmt::format("LabelTable: unknown key {}", key.print()));
}

void LabelTable::set_at(SymbolTable* st, const Object &key, const Object &value) {
    (void)st;(void)value;
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства только для чтения
        if (name == ":size" || name == "size" || name == ":count" || name == ":type" ||
            name == ":names" || name == ":offsets") {
            throw std::runtime_error("LabelTable::set_at: cannot set read-only property: " + name);
        }

        throw std::runtime_error("LabelTable::set_at: expected number or property list");
    }

    throw std::runtime_error(fmt::format("LabelTable::set_at: invalid key {}", key.print()));
}
} // namespace soot