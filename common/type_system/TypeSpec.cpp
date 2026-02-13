#include "common/type_system/TypeSpec.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include "common/util/Assert.hpp"
#include "fmt/format.h"
#include "type_system/TypeSystem.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>

using namespace script;

// ============================================================================
// TypeSpec constructors
// ============================================================================

TypeSpec::TypeSpec(std::string type) : m_type(std::move(type)) {}

TypeSpec::TypeSpec(std::string type, std::vector<TypeSpec> arguments)
    : m_type(std::move(type)),
      m_arguments(std::make_unique<std::vector<TypeSpec>>(std::move(arguments))) {}

// Rule of Five
TypeSpec::TypeSpec(const TypeSpec &other) : m_type(other.m_type), m_tags(other.m_tags) {
    if (other.m_arguments) {
        m_arguments = std::make_unique<std::vector<TypeSpec>>(*other.m_arguments);
    }
}

TypeSpec::TypeSpec(TypeSpec &&other) noexcept = default;

TypeSpec &TypeSpec::operator=(const TypeSpec &other) {
    if (this != &other) {
        m_type = other.m_type;
        m_tags = other.m_tags;
        if (other.m_arguments) {
            m_arguments = std::make_unique<std::vector<TypeSpec>>(*other.m_arguments);
        } else {
            m_arguments.reset();
        }
    }
    return *this;
}

TypeSpec &TypeSpec::operator=(TypeSpec &&other) noexcept = default;

// ============================================================================
// TypeSpec argument management
// ============================================================================

void TypeSpec::add_arg(const TypeSpec &ts) {
    if (!m_arguments) {
        m_arguments = std::make_unique<std::vector<TypeSpec>>();
    }
    m_arguments->push_back(ts);
}

void TypeSpec::add_arg(TypeSpec &&ts) {
    if (!m_arguments) {
        m_arguments = std::make_unique<std::vector<TypeSpec>>();
    }
    m_arguments->push_back(std::move(ts));
}

bool TypeSpec::has_single_arg() const {
    return m_arguments && m_arguments->size() == 1;
}

const TypeSpec &TypeSpec::get_single_arg() const {
    // ASSERT(m_arguments);
    // ASSERT(m_arguments->size() == 1);
    return m_arguments->front();
}

TypeSpec &TypeSpec::get_single_arg() {
    // ASSERT(m_arguments);
    // ASSERT(m_arguments->size() == 1);
    return m_arguments->front();
}

size_t TypeSpec::get_args_count() const {
    return m_arguments ? m_arguments->size() : 0;
}

const TypeSpec &TypeSpec::get_arg(int idx) const {
    // ASSERT(m_arguments);
    return m_arguments->at(idx);
}

TypeSpec &TypeSpec::get_arg(int idx) {
    // ASSERT(m_arguments);
    return m_arguments->at(idx);
}

const TypeSpec &TypeSpec::last_arg() const {
    // ASSERT(m_arguments);
    // ASSERT(!m_arguments->empty());
    return m_arguments->back();
}

TypeSpec &TypeSpec::last_arg() {
    // ASSERT(m_arguments);
    // ASSERT(!m_arguments->empty());
    return m_arguments->back();
}

bool TypeSpec::empty() const {
    return !m_arguments || m_arguments->empty();
}

// ============================================================================
// TypeTag Implementation
// ============================================================================

TypeTag::TypeTag(std::string name, std::string value)
    : name(std::move(name)), value(std::move(value)) {}

bool TypeTag::operator==(const TypeTag &other) const {
    return name == other.name && value == other.value;
}

bool TypeTag::operator!=(const TypeTag &other) const {
    return !(*this == other);
}

// ============================================================================
// TypeSpec Implementation
// ============================================================================

std::string TypeSpec::print() const {
    // Simple case: no arguments and no tags
    if ((!m_arguments || m_arguments->empty()) && m_tags.empty()) {
        return m_type;
    }

    // Complex case: with arguments and/or tags
    std::string result = "(" + m_type;

    // Print arguments
    if (m_arguments) {
        for (const auto &arg : *m_arguments) {
            result += " " + arg.print();
        }
    }

    // Print tags
    for (const auto &tag : m_tags) {
        result += fmt::format(" :{} {}", tag.name, tag.value);
    }

    return result + ")";
}

bool TypeSpec::operator==(const TypeSpec &other) const {
    // Compare base type
    if (m_type != other.m_type) {
        return false;
    }

    // Compare tags
    if (m_tags != other.m_tags) {
        return false;
    }

    // Compare arguments
    if (m_arguments && other.m_arguments) {
        return *m_arguments == *other.m_arguments;
    } else if (m_arguments || other.m_arguments) {
        // One has arguments, the other doesn't
        return false;
    }

    return true;
}

bool TypeSpec::operator!=(const TypeSpec &other) const {
    return !(*this == other);
}

TypeSpec TypeSpec::substitute_for_method_call(const std::string &method_type) const {
    TypeSpec result;
    result.m_type = (m_type == "_type_") ? method_type : m_type;

    // Recursively substitute arguments
    if (m_arguments) {
        result.m_arguments = std::make_unique<std::vector<TypeSpec>>();
        for (const auto &arg : *m_arguments) {
            result.m_arguments->push_back(arg.substitute_for_method_call(method_type));
        }
    }

    // Copy tags
    result.m_tags = m_tags;

    return result;
}

bool TypeSpec::is_compatible_child_method(const TypeSpec    &implementation,
                                          const std::string &child_type,
                                          int               *bad_arg_idx_out) const {
    // Check if base types are compatible
    bool base_type_ok = implementation.m_type == m_type ||
                        (m_type == "_type_" && implementation.m_type == child_type);

    if (!base_type_ok) {
        if (bad_arg_idx_out)
            *bad_arg_idx_out = -1;
        return false;
    }

    // Check argument count
    if (implementation.get_args_count() != get_args_count()) {
        if (bad_arg_idx_out)
            *bad_arg_idx_out = -1;
        return false;
    }

    // Check each argument for compatibility
    for (size_t i = 0; i < get_args_count(); i++) {
        if (!get_arg(i).is_compatible_child_method(implementation.get_arg(i), child_type)) {
            if (bad_arg_idx_out)
                *bad_arg_idx_out = i;
            return false;
        }
    }

    return true;
}

void TypeSpec::add_new_tag(const std::string &tag_name, const std::string &tag_value) {
    // Check for duplicate tag
    for (const auto &tag : m_tags) {
        if (tag.name == tag_name) {
            throw std::runtime_error(
                fmt::format("Attempted to add duplicate tag '{}' to TypeSpec", tag_name));
        }
    }

    m_tags.emplace_back(tag_name, tag_value);
}

std::optional<std::string> TypeSpec::try_get_tag(const std::string &tag_name) const {
    for (const auto &tag : m_tags) {
        if (tag.name == tag_name) {
            return tag.value;
        }
    }
    return std::nullopt;
}

const std::string &TypeSpec::get_tag(const std::string &tag_name) const {
    for (const auto &tag : m_tags) {
        if (tag.name == tag_name) {
            return tag.value;
        }
    }
    throw std::runtime_error(fmt::format("TypeSpec doesn't have tag '{}'", tag_name));
}

void TypeSpec::modify_tag(const std::string &tag_name, const std::string &tag_value) {
    for (auto &tag : m_tags) {
        if (tag.name == tag_name) {
            tag.value = tag_value;
            return;
        }
    }
    throw std::runtime_error(fmt::format("TypeSpec doesn't have tag '{}'", tag_name));
}

void TypeSpec::add_or_modify_tag(const std::string &tag_name, const std::string &tag_value) {
    for (auto &tag : m_tags) {
        if (tag.name == tag_name) {
            tag.value = tag_value;
            return;
        }
    }
    m_tags.emplace_back(tag_name, tag_value);
}

void TypeSpec::delete_tag(const std::string &tag_name) {
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it) {
        if (it->name == tag_name) {
            m_tags.erase(it);
            return;
        }
    }
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================
//
// TypeSpec coerce_to_reg_type(const TypeSpec& in) {
//    // For now, just return the input type unchanged
//    // In the original implementation, this would handle type coercion logic
//    return in;
//}

// ============================================================================
// Alias
// ============================================================================

Object TypeSpec::make_step_accessor(const Object &key) {

    auto name = key.to_std_string();
    // 1. Имя базового типа (например, "pointer")
    if (name == ".base-type") {
        return Object::make_string(base_type());
    }

    // 2. Количество аргументов
    if (name == ".args-count") {
        return Object::make_integer(get_args_count());
    }

    // 3. Список аргументов как объект (опционально, если хочешь видеть всё сразу)
    if (name == ".args-type-specs") {
        // Здесь можно либо вернуть список, либо специальный объект-итератор.
        // Пока оставим заглушку или вернем строку для отладки.
        return to_sexpr_typspec();
    }

    if (name == ".args-type-names") {
        // Здесь можно либо вернуть список, либо специальный объект-итератор.
        // Пока оставим заглушку или вернем строку для отладки.
        return to_sexpr_type_names();
    }

    if (name == ".args-types") {
        // Здесь можно либо вернуть список, либо специальный объект-итератор.
        // Пока оставим заглушку или вернем строку для отладки.
        return to_sexpr_type_objects();
    }

    // 4. Количество тегов
    if (name == ".tags-count") {
        return Object::make_integer(get_tags_count());
    };

    // Сначала проверяем: может быть ключ — это число (индекс аргумента)?
    // Это позволит писать (-> some-type-spec 0) чтобы получить первый аргумент
    if (key.is_integer()) {
        int64_t idx = key.as_integer();
        if (idx >= 0 && idx < (int64_t)get_args_count()) {
            // Возвращаем аргумент, обернутый в NativeRef, чтобы по нему можно было идти дальше
            return Object::make_native_ref(std::make_shared<TypeSpec>(get_arg(idx)));
        }
        return Object::make_null();
    }

    if (name == ".type") {
        return TypeSystem::instance().make_step_accessor(Object::make_string(base_type()));
    }

    // Если ключ — символ (например, 'base-type), используем стандартную карту HeapObject
    Object meta = HeapObject::make_step_accessor(key);

    if (!meta.is_none())
        return meta;

    throw std::runtime_error(
        fmt::format("type-spec step accessor does not know the key {}", key.to_std_string()));

    // Если HeapObject ничего не нашел (undefined), возвращаем пустой список (nil) для Лиспа
    return Object::make_null();
}

// Вспомогательная функция для преобразования TypeSpec
Object TypeSpec::inspect() const {
    std::vector<Object> list_elements;
    list_elements.push_back(Object::make_symbol("type-spec"));
    if (base_type() == "none" || base_type().empty()) {
        list_elements.push_back(Object::make_null());
        return pretty_print::build_list(list_elements);
    }

    Object base = Object::make_symbol(base_type());

    // Если нет ни аргументов, ни тегов — возвращаем просто символ (атом)
    if (get_args_count() == 0 && get_tags_count() == 0) {
        list_elements.push_back(base);
        return pretty_print::build_list(list_elements);
    }

    // Если есть хоть что-то — строим список (base args... tags...)
    list_elements.push_back(base);

    // 1. Добавляем вложенные TypeSpec (аргументы)
    for (size_t i = 0; i < get_args_count(); ++i) {
        list_elements.push_back(get_arg(i).inspect());
    }

    // 2. Добавляем теги в формате :key value
    for (const auto &tag : get_tags()) {
        // Превращаем имя тега в ключевое слово (например, "foo" -> ":foo")
        std::string keyword = ":" + tag.name;
        list_elements.push_back(Object::make_symbol(keyword));

        // Значение тега (предполагаем, что это может быть имя типа или строка)
        // Если значение выглядит как число, можно было бы парсить,
        // но пока оставим как символ/строку для простоты
        list_elements.push_back(Object::make_symbol(tag.value));
    }

    return pretty_print::build_list(list_elements);
}

// 1) S-expression с TypeSpec объектами
Object TypeSpec::to_sexpr_typspec() const {
    ListBuilder builder;
    builder.add(
        Object::make_native_ref(std::make_shared<TypeSpec>(*this) // Создаем копию как HeapObject
                                ));

    // Добавляем базовый тип
    builder.add(Object::make_string(m_type));

    // Рекурсивно добавляем аргументы
    if (m_arguments) {
        for (const auto &arg : *m_arguments) {
            builder.add(arg.to_sexpr_typspec());
        }
    }

    return builder.build();
}

// 2) S-expression с именами типов (как строки/символы)
Object TypeSpec::to_sexpr_type_names() const {
    ListBuilder builder;
    builder.add_symbol(m_type);

    // Рекурсивно добавляем аргументы
    if (m_arguments) {
        for (const auto &arg : *m_arguments) {
            builder.add(arg.to_sexpr_type_names());
        }
    }

    return builder.build();
}

// 3) S-expression с Type объектами (обертки для типов)
Object TypeSpec::to_sexpr_type_objects() const {
    ListBuilder builder;

    // Создаем объект, представляющий тип (может быть отдельный класс Type)
    // Предполагаем, что есть функция или конструктор make_type()
    auto type = TypeSystem::instance().make_step_accessor(Object::make_string(m_type));
    builder.add(type);

    // Рекурсивно добавляем аргументы
    if (m_arguments) {
        for (const auto &arg : *m_arguments) {
            builder.add(arg.to_sexpr_type_objects());
        }
    }

    return builder.build();
}

// Вспомогательный метод для рекурсивного обхода
void TypeSpec::append_to_sexpr(ListBuilder &builder, int mode) const {
    switch (mode) {
    case 0: // TypeSpec objects
        builder.add(Object::make_native_ref(std::make_shared<TypeSpec>(*this)));
        break;
    case 1: // Type names
        builder.add_symbol(m_type);
        break;
    case 2: // Type objects
        auto type = TypeSystem::instance().make_step_accessor(Object::make_string(m_type));
        builder.add(type);
        break;
    }

    if (m_arguments) {
        for (const auto &arg : *m_arguments) {
            arg.append_to_sexpr(builder, mode);
        }
    }
}

// ============================================================================
// typespec namespace Implementation
// ============================================================================

namespace typespec {
TypeSpec bool_() {
    return TypeSpec("bool");
}
TypeSpec object() {
    return TypeSpec("object");
}
TypeSpec int32() {
    return TypeSpec("int32");
}
TypeSpec int64() {
    return TypeSpec("int64");
}
TypeSpec float_() {
    return TypeSpec("float");
}
TypeSpec string() {
    return TypeSpec("string");
}
TypeSpec symbol() {
    return TypeSpec("symbol");
}
TypeSpec function() {
    return TypeSpec("function");
}

TypeSpec pointer(const TypeSpec &element) {
    return TypeSpec("pointer", {element});
}

TypeSpec inline_array(const TypeSpec &element) {
    return TypeSpec("inline-array", {element});
}
} // namespace typespec
