#include "LabelTable.hpp"

namespace script {
Object LabelTable::get_at(const Object &key) {
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
            for (const auto &[_, offset] : m_labels) {
                offsets.push_back(Object::make_integer(offset));
            }
            return Object::make_list(offsets);
        }
        if (name == ":table" || name == "table") {
            Object table = Object::make_hash_table();
            for (const auto &[label_name, offset] : m_labels) {
                table.as_hash_table()->set(label_name, Object::make_integer(offset));
            }
            return table;
        }

        // Доступ по имени метки - возвращаем смещение
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return Object::make_integer(it->second);
        }

        return Object::make_null();
    }

    // Доступ по индексу
    if (key.is_integer()) {
        size_t idx = key.as_integer();
        if (idx < m_labels.size()) {
            auto it = m_labels.begin();
            std::advance(it, idx);
            return pretty_print::build_list(
                pretty_print::build_list(Object::make_keyword("name"),
                                         Object::make_string(it->first)),
                pretty_print::build_list(Object::make_keyword("offset"),
                                         Object::make_integer(it->second)));
        }
    }

    throw std::runtime_error(fmt::format("LabelTable: unknown key {}", key.print()));
}

void LabelTable::set_at(const Object &key, const Object &value) {
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // Системные свойства только для чтения
        if (name == ":size" || name == "size" || name == ":count" || name == ":type" ||
            name == ":names" || name == ":offsets") {
            throw std::runtime_error("LabelTable::set_at: cannot set read-only property: " + name);
        }

        // Установка метки
        if (value.is_number()) {
            // Просто число - устанавливаем смещение
            size_t offset = value.as_integer();
            m_labels[name] = offset;
            return;
        } else if (value.is_pair()) {
            // Property list - может содержать :address и другие атрибуты в будущем
            // Формат: (:address 1234) или (:address 1234 :align 2)
            size_t offset = 0;

            // Парсим property list
            Object current = value;
            while (current.is_pair()) {
                Object entry = current.as_pair()->car;
                if (entry.is_pair()) {
                    Object prop = entry.as_pair()->car;
                    Object val = entry.as_pair()->cdr;

                    if (prop.is_keyword() && prop.to_std_string() == ":address") {
                        if (val.is_pair() && val.as_pair()->car.is_number()) {
                            offset = val.as_pair()->car.as_integer();
                        }
                    }
                    // В будущем можно добавить другие атрибуты
                }
                current = current.as_pair()->cdr;
            }

            m_labels[name] = offset;
            return;
        }

        throw std::runtime_error("LabelTable::set_at: expected number or property list");
    }

    throw std::runtime_error(fmt::format("LabelTable::set_at: invalid key {}", key.print()));
}
} // namespace script