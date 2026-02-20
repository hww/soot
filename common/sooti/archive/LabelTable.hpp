#pragma once

#include "Archive.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace script {

/**
 * @brief Таблица меток - только имена и смещения
 *
 * Никакой логики линковки, только хранение и сериализация.
 */
class LabelTable : public HeapObject {
  private:
    std::unordered_map<std::string, size_t> m_labels;

  public:
    LabelTable() = default;

    // ============================================================
    // HeapObject implementation
    // ============================================================

    std::string class_name() const override {
        return "label-table";
    }

    std::string full_class_name() const override {
        return "LabelTable";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == type_name_obj() || HeapObject::is_class_name(name);
    }

    std::string print() const override {
        return fmt::format("#<label-table {} labels>", m_labels.size());
    }

    Object inspect() const override {
        return pretty_print::build_list(
            pretty_print::build_list(Object::make_symbol(":type"),
                                     Object::make_string(class_name())),
            pretty_print::build_list(Object::make_symbol(":count"),
                                     Object::make_integer(m_labels.size())));
    }

    // ============================================================
    // Serialization
    // ============================================================

    inline friend Archive &operator<<(Archive &ar, LabelTable &v) {
        v.serialize(ar);
        return ar;
    }

    // Для shared_ptr
    inline friend Archive &operator<<(Archive &ar, const std::shared_ptr<LabelTable> &ptr) {
        if (ptr) {
            ptr->serialize(ar);
        } else {
            // Сериализуем nullptr как нулевую магию
            CompactPointer null_magic{0x00000000};
            ar << null_magic;
        }
        return ar;
    }

    void serialize(Archive &ar) override {
        // Магия для идентификации
        Crc32Value magic{0x4C41424C}; // "LABL"
        ar << magic;

        // Версия формата
        CompactIndex version(1);
        ar << version;

        if (ar.is_loading()) {
            // Чтение
            CompactIndex count;
            ar << count;

            m_labels.clear();
            m_labels.reserve(count.value);

            for (int i = 0; i < count.value; i++) {
                // Читаем имя
                CompactIndex name_len;
                ar << name_len;

                std::string name;
                name.resize(name_len.value);
                ar.serialize(&name[0], name_len.value);

                // Читаем смещение
                CompactPointer offset;
                ar << offset;

                m_labels[name] = offset.value;
            }
        } else {
            // Запись
            CompactIndex count(m_labels.size());
            ar << count;

            for (const auto &[name, offset] : m_labels) {
                // Пишем имя
                CompactIndex name_len(name.length());
                ar << name_len;
                ar.serialize(const_cast<char *>(name.data()), name.length());

                // Пишем смещение
                CompactPointer cp(offset);
                ar << cp;
            }
        }
    }

    // ============================================================
    // Public API
    // ============================================================

    /**
     * Добавить метку
     */
    void add(const std::string &name, size_t offset) {
        m_labels[name] = offset;
    }

    /**
     * Получить смещение по метке
     */
    std::optional<size_t> get(const std::string &name) const {
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * Проверить наличие метки
     */
    bool has(const std::string &name) const {
        return m_labels.find(name) != m_labels.end();
    }

    /**
     * Удалить метку
     */
    bool remove(const std::string &name) {
        return m_labels.erase(name) > 0;
    }

    /**
     * Получить все метки
     */
    const auto &all() const {
        return m_labels;
    }

    /**
     * Очистить таблицу
     */
    void clear() {
        m_labels.clear();
    }

    /**
     * Количество меток
     */
    size_t size() const {
        return m_labels.size();
    }

    /**
     * Пустая ли таблица
     */
    bool empty() const {
        return m_labels.empty();
    }
};

} // namespace script
