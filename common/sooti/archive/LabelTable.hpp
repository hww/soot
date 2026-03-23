#pragma once

#include "common/sooti/Archive.hpp"
#include "common/sooti/ListBuilder.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace script {

struct MemoryLabel : public NativeObject {
    std::string name;
    size_t      offset;  // Смещение в буфере
    Object      segment; // Имя или объект сегмента (Object для гибкости)
    Object      info;    // Метаданные (asmsym-info из Lisp)

    // Конструктор для удобства
    MemoryLabel(std::string n, size_t a, Object seg, Object i)
        : name(n), offset(a), segment(seg), info(i) {}

    std::string full_class_name() const override {
        return "MemoryLabel";
    }

    std::string class_name() const override {
        return "memory-label";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == MemoryLabel::type_name_obj() || NativeObject::is_class_name(name);
    }

    std::string print() const override {
        return fmt::format("#<memory-label {:08X} {} {}>", offset, segment.print(), info.print());
    };

    Object inspect() const override {
        // Создаем Map или список пар для отображения внутреннего состояния
        // Предполагаю, у тебя есть метод создания словаря/карты
        ListBuilder builder{};
        builder.add_symbol(name);
        builder.add_key_value("address", Object::make_integer(offset));
        builder.add_key_value("segment", segment);
        builder.add_key_value("info", info);
        return builder.build();
    }

    Object get_at(const Object &key) override {
        if (key.is_symbol() || key.is_string()) {
            std::string key_str = key.to_std_string();
            if (key_str == ":name") {
                return Object::make_symbol(name);
            }

            // Позволяем доставать адрес
            if (key_str == ":offset") {
                return Object::make_integer(offset);
            }

            // Позволяем доставать сегмент
            if (key_str == ":segment") {
                return segment;
            }

            // Позволяем доставать метаданные
            if (key_str == ":info") {
                return info;
            }
        }
        throw std::runtime_error("MemoryLabel expects :offset, :segment or :info, got " +
                                 key.print());
        // Если ключ не распознан, возвращаем undefined или ошибку
        return Object::make_none();
    }

    void serialize(Archive &ar) override {
        // Магия для идентификации типа объекта
        CompactCrc32 magic{0x4D4C424C}; // "MLBL" (Memory LaBeL)
        ar << magic;

        // Версия формата
        CompactIndex version(1);
        ar << version;

        if (ar.is_reading()) {
            // ============================================================
            // РЕЖИМ ЧТЕНИЯ
            // ============================================================

            // Читаем имя
            CompactIndex name_len;
            ar << name_len;
            name.resize(name_len.value);
            ar.serialize_obj(&name[0], name_len);

            // Читаем адрес
            CompactPointer addr_cp;
            ar << addr_cp;
            offset = addr_cp.value;

            // Читаем сегмент (Object умеет себя сериализовать)
            segment.serialize(ar);

            // Читаем метаданные (Object умеет себя сериализовать)
            info.serialize(ar);

        } else {
            // ============================================================
            // РЕЖИМ ЗАПИСИ
            // ============================================================

            // Пишем имя
            CompactIndex name_len(name.length());
            ar << name_len;
            ar.serialize_obj(const_cast<char *>(name.data()), name.length());

            // Пишем адрес
            CompactPointer addr_cp(offset);
            ar << addr_cp;

            // Пишем сегмент
            segment.serialize(ar);

            // Пишем метаданные
            info.serialize(ar);
        }
    }
};

/**
 * @brief Таблица меток - только имена и смещения
 *
 * Никакой логики линковки, только хранение и сериализация.
 */
class LabelTable : public NativeObject {
  private:
    std::unordered_map<std::string, std::shared_ptr<MemoryLabel>> m_labels;

  public:
    LabelTable() = default;
    ~LabelTable() {}

    // ============================================================
    // NativeObject implementation
    // ============================================================

    Object get_at(const Object &key) override;
    void   set_at(const Object &key, const Object &value) override;

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
        return name == LabelTable::type_name_obj() || NativeObject::is_class_name(name);
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
        // Магия для идентификации таблицы
        CompactCrc32 magic{0x4C41424C}; // "LABL"
        ar << magic;

        // Версия формата
        CompactIndex version(2); // увеличили версию
        ar << version;

        if (ar.is_reading()) {
            // Чтение
            CompactIndex count;
            ar << count;

            m_labels.clear();
            m_labels.reserve(count.value);

            for (int i = 0; i < count.value; i++) {
                // Создаем новую метку
                auto label =
                    std::make_shared<MemoryLabel>("", 0, Object::make_null(), Object::make_null());

                // Метка сериализует себя сама
                label->serialize(ar);

                // Сохраняем в таблицу
                m_labels[label->name] = label;
            }
        } else {
            // Запись
            CompactIndex count(m_labels.size());
            ar << count;

            for (const auto &[_, label] : m_labels) {
                label->serialize(ar);
            }
        }
    }

    // ============================================================
    // Public API
    // ============================================================

    /**
     * Добавить метку
     */
    void add_label(const std::string &name, size_t addr,
                   const Object &segment = Object::make_null(),
                   const Object &info = Object::make_null()) {
        auto label = std::make_shared<MemoryLabel>(name, addr, segment, info);
        m_labels[name] = label;
    }

    /**
     * Получить метку по метке
     */
    std::shared_ptr<MemoryLabel> get_label(const std::string &name) const {
        auto it = m_labels.find(name);
        return it != m_labels.end() ? it->second : nullptr;
    }

    /**
     * Получить метку в виде объекта
     */
    Object get_label_object(const std::string &name) const {
        auto label = get_label(name);
        if (label) {
            return Object::make_heap_obj(label);
        }
        return Object::make_null();
    }

    /**
     * Получить смещение по метке
     */
    std::optional<size_t> get_offset(const std::string &name) const {
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return it->second->offset;
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
