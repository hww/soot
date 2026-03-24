#include "common/type_system/Type.hpp"
#include "TypeSystem.hpp"
#include "common/sooti/ListBuilder.hpp"
#include "common/util/Assert.hpp"
#include "fmt/format.h"
#include <algorithm>
#include <stdexcept>

std::string reg_kind_to_string(RegClass reg_class) {
    switch (reg_class) {
    case RegClass::GPR_8:
        return "gpr8";
    case RegClass::GPR_16:
        return "gpr16";
    case RegClass::GPR_32:
        return "gpr32";
    case RegClass::GPR_64:
        return "gpr64";
    case RegClass::FPR:
        return "float";
    case RegClass::INVALID:
        return "invalid";
    default:
        return "unknown";
    }
}

int Type::verbose = 0;

// ============================================================================
// MethodInfo Implementation
// ============================================================================

bool MethodInfo::operator==(const MethodInfo &other) const {
    return id == other.id && name == other.name && type == other.type &&
           defined_in_type == other.defined_in_type && no_virtual == other.no_virtual &&
           overrides_parent == other.overrides_parent &&
           only_overrides_docstring == other.only_overrides_docstring;
}

std::string MethodInfo::print_one_line() const {
    return fmt::format("Method {:3d}: {:20} {}", id, name, type.print());
}

bool MethodInfo::operator!=(const MethodInfo &other) const {
    return !(*this == other);
}

Object MethodInfo::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Простые поля
    if (name == ":id")
        return Object::make_integer(this->id);
    if (name == ":name")
        return Object::make_string(this->name);
    if (name == ":defined-in")
        return Object::make_string(this->defined_in_type);
    if (name == ":type-name")
        return Object::make_string(this->type_name);

    // 2. Сложные поля (создаем объекты на лету)
    if (name == ":type-spec") {
        // Оборачиваем TypeSpec. Теперь (-> method 'type 'base-type) сработает сам,
        // потому что у TypeSpec тоже будет свой get_at
        return Object::make_heap_obj(std::make_shared<TypeSpec>(this->type));
    }

    if (name == ":type") {
        // Оборачиваем TypeSpec. Теперь (-> method 'type 'base-type) сработает сам,
        // потому что у TypeSpec тоже будет свой get_at
        auto base_type = this->type.base_type();
        return TypeSystem::instance().get_at(Object::make_string(base_type));
    }

    // 3. Флаги и логика
    if (name == "virtual?")
        return Object::make_boolean(!this->no_virtual);
    if (name == "overrides?")
        return Object::make_boolean(this->overrides_parent);

    // 4. Опциональные поля
    if (name == "doc") {
        return this->docstring ? Object::make_string(*this->docstring) : Object::make_null();
    }
    if (name == "overlay") {
        return this->overlay_name ? Object::make_string(*this->overlay_name) : Object::make_null();
    }

    // Если ничего не подошло — отдаем undefined
    return Object::make_none();
}

// ============================================================================
// Field Implementation
// ============================================================================

Field::Field(std::string name, TypeSpec ts) : m_name(std::move(name)), m_type(std::move(ts)) {}

Field::Field(std::string name, TypeSpec ts, int offset)
    : m_name(std::move(name)), m_type(std::move(ts)), m_offset(offset) {}

std::string Field::print() const {
    return fmt::format("Field: ({} {} :offset {}) inline: {:5}, dynamic: {:5}, array: {:5}, array "
                       "size {:3}, align {:2}, skip {}",
                       m_name, m_type.print(), m_offset, m_inline, m_dynamic, m_array, m_array_size,
                       m_alignment, m_skip_in_static_decomp);
}

void Field::set_dynamic() {
    m_dynamic = true;
    m_array = true;
}

void Field::set_array(int size) {
    m_array_size = size;
    m_array = true;
}

void Field::set_inline() {
    m_inline = true;
}

bool Field::operator==(const Field &other) const {
    return m_name == other.m_name && m_type == other.m_type && m_offset == other.m_offset &&
           m_inline == other.m_inline && m_dynamic == other.m_dynamic && m_array == other.m_array &&
           m_array_size == other.m_array_size && m_alignment == other.m_alignment;
}

void Field::set_override_type(const TypeSpec &new_type) {
    m_type = new_type;
    m_override_type = true;
}

void Field::mark_as_user_placed() {
    m_placed_by_user = true;
}

bool Field::operator!=(const Field &other) const {
    return !(*this == other);
}

Object Field::inspect() const {
    ListBuilder lb;
    lb.add_symbol("field");

    lb.add_keyword("name");
    lb.add_string(m_name);

    lb.add_keyword("type");
    lb.add_string(m_type.print());

    lb.add_keyword("offset");
    lb.add_integer(m_offset);

    lb.add_keyword("inline");
    lb.add_boolean(m_inline);

    lb.add_keyword("dynamic");
    lb.add_boolean(m_dynamic);

    lb.add_keyword("array");
    lb.add_boolean(m_array);

    lb.add_keyword("array_size");
    lb.add_integer(m_array_size);

    lb.add_keyword("alignment");
    lb.add_integer(m_alignment);

    lb.add_keyword("skip_in_static_decomp");
    lb.add_boolean(m_skip_in_static_decomp);

    return lb.build();
}

Object Field::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Базовые свойства
    if (name == ":name")
        return Object::make_string(this->name());
    if (name == ":offset")
        return Object::make_integer(this->offset());
    if (name == ":alignment")
        return Object::make_integer(this->alignment());

    // 2. Тип (TypeSpec) — создаем HeapObject для дальнейшей навигации
    if (name == ":type-spec") {
        return Object::make_heap_obj(std::make_shared<TypeSpec>(this->type()));
    }
    if (name == ":type") {
        // Оборачиваем TypeSpec. Теперь (-> method 'type 'base-type) сработает сам,
        // потому что у TypeSpec тоже будет свой get_at
        auto base_type = this->type().base_type();
        return TypeSystem::instance().get_at(Object::make_string(base_type));
    }

    // 3. Флаги состояния (теперь возвращают логический тип)
    if (name == ":inline?")
        return Object::make_boolean(this->is_inline());
    if (name == ":dynamic?")
        return Object::make_boolean(this->is_dynamic());
    if (name == ":array?")
        return Object::make_boolean(this->is_array());

    // 4. Специфичные поля
    if (name == ":array-size") {
        return Object::make_integer(this->is_array() ? this->array_size() : 0);
    }

    if (name == ":comment") {
        return this->has_comment() ? Object::make_string(this->comment()) : Object::make_null();
    }

    auto type = TypeSystem::instance().lookup_type(m_type);
    if (type) {
        return type->get_at(key);
    }
    // Если ключ не найден, возвращаем undefined, чтобы navigation понял, что пути нет
    return Object::make_none();
}

// ============================================================================
// Type Base Implementation
// ============================================================================

Type::Type(std::string parent, std::string name, bool is_boxed, int heap_base)
    : m_parent(std::move(parent)), m_name(std::move(name)), m_is_boxed(is_boxed),
      m_heap_base(heap_base) {
    m_runtime_name = m_name;
}

bool Type::get_my_method(const std::string &name, MethodInfo *out) const {

    for (const auto &method : m_methods) {
        if (method.name == name) {
            if (out)
                *out = method;
            return true;
        }
    }
    if (name == "new") {
        return get_my_new_method(out);
    }
    return false;
}

bool Type::get_my_method(int id, MethodInfo *out) const {
    for (const auto &method : m_methods) {
        if (method.id == id) {
            if (out)
                *out = method;
            return true;
        }
    }
    if (id == 0) {
        return get_my_new_method(out);
    }
    return false;
}

bool Type::get_my_last_method(MethodInfo *out) const {
    for (auto it = m_methods.rbegin(); it != m_methods.rend(); ++it) {
        if (!it->overrides_parent && !it->only_overrides_docstring) {
            if (out)
                *out = *it;
            return true;
        }
    }
    return false;
}
size_t Type::methods_max_id() const {
    size_t id = -1;
    if (has_new_method())
        id = 0;
    for (auto it = m_methods.rbegin(); it != m_methods.rend(); ++it) {
        if (it->id > id)
            id = it->id;
    }
    return id;
}

bool Type::get_my_new_method(MethodInfo *out) const {
    if (m_new_method_info_defined) {
        if (out)
            *out = m_new_method_info;
        return true;
    }
    return false;
}

int Type::get_num_methods() const {
    int count = 0;
    for (const auto &method : m_methods) {
        if (!method.only_overrides_docstring) {
            count++;
        }
    }
    return count;
}

const MethodInfo &Type::add_method(const MethodInfo &info) {
    if (!info.overrides_parent) {
        // Verify method ID ordering
        for (auto it = m_methods.rbegin(); it != m_methods.rend(); ++it) {
            if (!it->overrides_parent && !it->only_overrides_docstring) {
                ASSERT(it->id + 1 == info.id);
                break;
            }
        }
    }

    m_methods.push_back(info);
    return m_methods.back();
}

const MethodInfo &Type::add_new_method(const MethodInfo &info) {
    ASSERT(info.name == "new");
    m_new_method_info_defined = true;
    m_new_method_info = info;
    return m_new_method_info;
}

std::string Type::print_method_info() const {
    std::string result;
    if (m_new_method_info_defined) {
        result += "  " + m_new_method_info.print_one_line() + "\n";
    }

    for (const auto &method : m_methods) {
        result += "  " + method.print_one_line() + "\n";
    }

    return result;
}

void Type::add_state(const std::string &name, const TypeSpec &type) {
    auto result = m_states.insert({name, type});
    if (!result.second) {
        throw std::runtime_error(fmt::format("State {} is already defined in type", name));
    }
}

std::string Type::diff(const Type &other) const {
    return common_type_info_diff(other) + diff_impl(other);
}

bool Type::common_type_info_equal(const Type &other) const {
    // Check methods (ignoring docstring-only overrides)
    bool methods_equal = true;
    for (const auto &method : m_methods) {
        if (method.only_overrides_docstring)
            continue;

        bool found = false;
        for (const auto &other_method : other.m_methods) {
            if (method.id == other_method.id) {
                if (method == other_method) {
                    found = true;
                    break;
                } else {
                    methods_equal = false;
                    break;
                }
            }
        }
        if (!methods_equal || !found) {
            methods_equal = false;
            break;
        }
    }

    return methods_equal && m_states == other.m_states &&
           m_new_method_info == other.m_new_method_info &&
           m_new_method_info_defined == other.m_new_method_info_defined &&
           m_parent == other.m_parent && m_name == other.m_name &&
           m_allow_in_runtime == other.m_allow_in_runtime &&
           m_runtime_name == other.m_runtime_name && m_is_boxed == other.m_is_boxed &&
           m_generate_inspect == other.m_generate_inspect && m_heap_base == other.m_heap_base;
}

std::string Type::common_type_info_diff(const Type &other) const {
    std::string result;

    // Compare methods
    if (m_methods.size() != other.m_methods.size()) {
        result += fmt::format("Method count: {} vs {}\n", m_methods.size(), other.m_methods.size());
    }

    for (size_t i = 0; i < std::min(m_methods.size(), other.m_methods.size()); ++i) {
        if (m_methods[i] != other.m_methods[i]) {
            result += fmt::format("Method {} differs\n", i);
            // TODO: Add detailed method diff
        }
    }

    // Compare other common fields
    if (m_parent != other.m_parent) {
        result += fmt::format("Parent: {} vs {}\n", m_parent, other.m_parent);
    }
    if (m_name != other.m_name) {
        result += fmt::format("Name: {} vs {}\n", m_name, other.m_name);
    }
    if (m_is_boxed != other.m_is_boxed) {
        result += fmt::format("Is boxed: {} vs {}\n", m_is_boxed, other.m_is_boxed);
    }

    return result;
}

std::string Type::incompatible_diff(const Type &other) const {
    return fmt::format("Cannot compare {} with {}\n", typeid(*this).name(), typeid(other).name());
}

std::string Type::get_runtime_name() const {
    if (!m_allow_in_runtime) {
        throw std::runtime_error(fmt::format("Type {} is not allowed in runtime", get_name()));
    }
    return m_runtime_name;
}

Object Type::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // Простое сравнение строк — это в разы быстрее, чем поиск в std::map<string, lambda>
    if (name == ":name")
        return Object::make_string(this->get_name());
    if (name == ":class")
        return Object::make_string(this->class_name());
    if (name == ":parent")
        return Object::make_string(this->get_parent());
    if (name == ":size" || name == ":size-in-memory")
        return Object::make_integer(this->get_size_in_memory());
    if (name == ":alignment" || name == ":in-memory-alignment")
        return Object::make_integer(this->get_in_memory_alignment());
    if (name == ":boxed?")
        return Object::make_boolean(this->is_boxed());
    if (name == ":methods-count")
        return Object::make_integer(this->get_num_methods());
    if (name == ":has-methods")
        return Object::make_boolean(this->get_num_methods() > 0);
    if (name == ":is-reference")
        return Object::make_boolean(false);

    // -----------------------------------
    if (name == ":methods") {
        ListBuilder lb;

        // 1. Добавляем специальный метод 'new', если он определен
        if (m_new_method_info_defined) {
            auto method_ptr = std::shared_ptr<MethodInfo>(
                const_cast<MethodInfo *>(&m_new_method_info), [](MethodInfo *) {});
            lb.add(Object::make_heap_obj(method_ptr));
        }

        // 2. Добавляем все остальные методы из вектора
        for (auto &method : m_methods) {
            auto method_ptr =
                std::shared_ptr<MethodInfo>(const_cast<MethodInfo *>(&method), [](MethodInfo *) {});
            lb.add(Object::make_heap_obj(method_ptr));
        }

        return lb.build();
    }
    if (name == ":methods-names") {
        ListBuilder lb;

        // 1. Добавляем специальный метод 'new', если он определен
        if (m_new_method_info_defined) {
            lb.add(Object::make_symbol("new"));
        }

        // 2. Добавляем все остальные методы из вектора
        for (auto &method : m_methods) {
            lb.add(Object::make_symbol(method.name.c_str()));
        }

        return lb.build();
    }
    if (name == ":methods-ids") {
        ListBuilder lb;

        // 1. Добавляем специальный метод 'new', если он определен
        if (m_new_method_info_defined) {
            lb.add(Object::make_integer(0));
        }

        // 2. Добавляем все остальные методы из вектора
        for (auto &method : m_methods) {
            lb.add(Object::make_integer(method.id));
        }

        return lb.build();
    }
    if (name == ":methods-max-id") {
        int max_id = -1;
        if (m_new_method_info_defined)
            max_id = 0;
        for (auto &method : m_methods) {
            if (method.id > max_id)
                max_id = method.id;
        }
        return Object::make_integer(max_id);
    }
    // -----------------------------------
    // Check if this is the `new` method
    if (name == "new") {
        if (m_new_method_info_defined) {
            auto method_ptr = std::shared_ptr<MethodInfo>(
                const_cast<MethodInfo *>(&m_new_method_info), [](MethodInfo *) {});
            return Object::make_heap_obj(method_ptr);
        }
        return Object::make_none();
    }

    // Search method by name
    for (auto &method : m_methods) {
        if (method.name == name) {
            auto method_ptr =
                std::shared_ptr<MethodInfo>(const_cast<MethodInfo *>(&method), [](MethodInfo *) {});
            return Object::make_heap_obj(method_ptr);
        }
    }
    throw std::runtime_error(fmt::format("Type.get_at called with unknown key {}", name));
}

// ============================================================================
// NullType Implementation
// ============================================================================

NullType::NullType(std::string name) : Type("object", std::move(name), false, 0) {}

bool NullType::is_reference() const {
    throw std::runtime_error("is_reference called on NullType");
}

int NullType::get_load_size() const {
    throw std::runtime_error("get_load_size called on NullType");
}

bool NullType::get_load_signed() const {
    throw std::runtime_error("get_load_signed called on NullType");
}

int NullType::get_size_in_memory() const {
    throw std::runtime_error("get_size_in_memory called on NullType");
}

RegClass NullType::get_preferred_reg_class() const {
    throw std::runtime_error("get_preferred_reg_class called on NullType");
}

int NullType::get_offset() const {
    throw std::runtime_error("get_offset called on NullType");
}

int NullType::get_in_memory_alignment() const {
    throw std::runtime_error("get_in_memory_alignment called on NullType");
}

int NullType::get_inline_array_stride_alignment() const {
    throw std::runtime_error("get_inline_array_stride_alignment called on NullType");
}

int NullType::get_inline_array_start_alignment() const {
    throw std::runtime_error("get_inline_array_start_alignment called on NullType");
}

std::string NullType::print() const {
    return m_name;
}

bool NullType::operator==(const Type &other) const {
    return this == &other; // Only equal to itself
}

std::string NullType::diff_impl(const Type &other) const {
    return (*this == other) ? "" : "NullType comparison failed";
}

Object NullType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем свои специфичные свойства
    if (name == ":null?") {
        return Object::make_boolean(true); // Используем boolean вместо integer 1
    }

    // 2. Если это не наше свойство, пробрасываем вызов родителю (Type)
    // Это и есть настоящая мощь наследования в нашей системе аксессоров.
    return Type::get_at(key);
}

// ============================================================================
// ValueType Implementation
// ============================================================================

ValueType::ValueType(std::string parent, std::string name, bool is_boxed, int size,
                     bool sign_extend, RegClass reg)
    : Type(std::move(parent), std::move(name), is_boxed, 0), m_size(size),
      m_sign_extend(sign_extend), m_reg_kind(reg) {}

int ValueType::get_offset() const {
    return m_offset;
}

int ValueType::get_in_memory_alignment() const {
    return m_size;
}

int ValueType::get_inline_array_stride_alignment() const {
    return m_size;
}

int ValueType::get_inline_array_start_alignment() const {
    return m_size;
}

void ValueType::inherit(const ValueType *parent) {
    m_sign_extend = parent->m_sign_extend;
    m_size = parent->m_size;
    m_offset = parent->m_offset;
    m_reg_kind = parent->m_reg_kind;
}

std::string ValueType::print() const {
    return fmt::format(
        "[ValueType] {}\n parent: {}\n boxed: {}\n size: {}\n sext: {}\n register: {}\n{}", m_name,
        m_parent, m_is_boxed, m_size, m_sign_extend, reg_kind_to_string(m_reg_kind),
        print_method_info());
}

bool ValueType::operator==(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }

    const ValueType *other_value = dynamic_cast<const ValueType *>(&other);
    return common_type_info_equal(other) && m_size == other_value->m_size &&
           m_sign_extend == other_value->m_sign_extend && m_reg_kind == other_value->m_reg_kind &&
           m_offset == other_value->m_offset;
}

std::string ValueType::diff_impl(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return incompatible_diff(other);
    }

    const ValueType *other_value = dynamic_cast<const ValueType *>(&other);
    std::string      result;

    if (m_size != other_value->m_size) {
        result += fmt::format("Size: {} vs {}\n", m_size, other_value->m_size);
    }
    if (m_sign_extend != other_value->m_sign_extend) {
        result += fmt::format("Sign extend: {} vs {}\n", m_sign_extend, other_value->m_sign_extend);
    }
    if (m_reg_kind != other_value->m_reg_kind) {
        result += fmt::format("Register kind: {} vs {}\n", static_cast<int>(m_reg_kind),
                              static_cast<int>(other_value->m_reg_kind));
    }
    if (m_offset != other_value->m_offset) {
        result += fmt::format("Offset: {} vs {}\n", m_offset, other_value->m_offset);
    }

    return result;
}

bool ValueType::is_reference() const {
    return false;
}

int ValueType::get_load_size() const {
    return m_size;
}

bool ValueType::get_load_signed() const {
    return m_sign_extend;
}

int ValueType::get_size_in_memory() const {
    return m_size;
}

RegClass ValueType::get_preferred_reg_class() const {
    return m_reg_kind;
}

Object ValueType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Проверяем специфичные поля ValueType
    if (name == ":size")
        return Object::make_integer(this->m_size);
    if (name == ":sign-extend?")
        return Object::make_boolean(this->m_sign_extend);
    if (name == ":offset")
        return Object::make_integer(this->m_offset);

    if (name == ":reg-class") {
        // Допустим, у нас есть маппинг энума в строку
        return Object::make_string(reg_kind_to_string(this->get_preferred_reg_class()));
    }

    // 2. Если это не наше поле, просим родителя (Type) ответить.
    // Тот проверит "name", "parent", "alignment" и т.д.
    return Type::get_at(key);
}

/*!
 * Serialize value type
 */
bool ValueType::serialize_obj(Archive &ar, Object &data) {
    std::string type_name = get_name();

    if (ar.is_reading()) {
        // ============================================================
        // РЕЖИМ ЧТЕНИЯ: из архива в Object
        // ============================================================

        if (type_name == "pointer" || type_name == "object") {
            // Указатель - читаем как 16-битное смещение
            uint16_t offset;
            ar << offset;
            data = Object::make_integer(offset);

        } else if (type_name == "string" || type_name == "symbol") {
            // Строка или символ - читаем как Pascal-строку (длина + данные)
            CompactIndex len;
            ar << len;

            std::string str;
            str.resize(len.value);
            ar.serialize_obj(&str[0], len.value);

            if (type_name == "symbol") {
                data = Object::make_symbol(str);
            } else {
                data = Object::make_string(str);
            }

        } else if (type_name == "bool") {
            // Булево значение - 1 байт
            uint8_t b;
            ar << b;
            data = Object::make_boolean(b != 0);

        } else {
            // Числовые типы
            int64_t value = 0;

            switch (get_load_size()) {
            case 1: {
                uint8_t v;
                ar << v;
                value = v;
                break;
            }
            case 2: {
                uint16_t v;
                ar << v;
                value = v;
                break;
            }
            case 4: {
                if (type_name == "float") {
                    float v;
                    ar << v;
                    data = Object::make_float(v);
                    return true;
                } else {
                    uint32_t v;
                    ar << v;
                    value = v;
                }
                break;
            }
            case 8: {
                if (type_name == "double") {
                    double v;
                    ar << v;
                    data = Object::make_float(v);
                    return true;
                } else {
                    uint64_t v;
                    ar << v;
                    value = v;
                }
                break;
            }
            default:
                throw std::runtime_error("ValueType: unsupported size for " + type_name);
            }

            data = Object::make_integer(value);
        }

        return true;

    } else {
        // ============================================================
        // РЕЖИМ ЗАПИСИ: из Object в архив
        // ============================================================

        if (type_name == "pointer" || type_name == "object") {
            // Указатель - ожидаем число (смещение)
            if (!data.is_number()) {
                throw std::runtime_error("ValueType '" + type_name + "': expected number");
            }
            uint16_t offset = data.as_integer();
            ar << offset;

        } else if (type_name == "string" || type_name == "symbol") {
            // Строка или символ
            std::string str;
            if (type_name == "symbol") {
                if (!data.is_symbol()) {
                    throw std::runtime_error("ValueType 'symbol': expected symbol");
                }
                str = data.to_std_string();
            } else {
                if (!data.is_string()) {
                    throw std::runtime_error("ValueType 'string': expected string");
                }
                str = data.as_string()->data;
            }

            CompactIndex len(str.length());
            ar << len;
            ar.serialize_obj(&str[0], str.length());

        } else if (type_name == "bool") {
            // Булево значение
            bool    b = data.is_true();
            uint8_t byte = b ? 1 : 0;
            ar << byte;

        } else {
            // Числовые типы
            int64_t value = 0;
            if (data.is_number()) {
                value = data.as_integer();
            } else if (data.is_null()) {
                // default 0 will be
            } else if (!data.is_number()) {
                throw std::runtime_error("ValueType '" + type_name + "': expected number, got " +
                                         data.print());
            }

            switch (get_load_size()) {
            case 1: {
                uint8_t v = value;
                ar << v;
                break;
            }
            case 2: {
                uint16_t v = value;
                ar << v;
                break;
            }
            case 4: {
                if (type_name == "float") {
                    float v = data.as_float();
                    ar << v;
                } else {
                    uint32_t v = value;
                    ar << v;
                }
                break;
            }
            case 8: {
                if (type_name == "double") {
                    double v = data.as_float();
                    ar << v;
                } else {
                    uint64_t v = value;
                    ar << v;
                }
                break;
            }
            default:
                throw std::runtime_error("ValueType: unsupported size for " + type_name);
            }
        }

        return false;
    }
}

// ============================================================================
// ReferenceType Implementation
// ============================================================================

ReferenceType::ReferenceType(std::string parent, std::string name, bool is_boxed, int heap_base)
    : Type(std::move(parent), std::move(name), is_boxed, heap_base) {}

std::string ReferenceType::print() const {
    return fmt::format("[ReferenceType] {}\n parent: {}\n boxed: {}\n{}", m_name, m_parent,
                       m_is_boxed, print_method_info());
}

Object ReferenceType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем то, что специфично для ссылок
    if (name == ":heap-base")
        return Object::make_integer(this->heap_base());
    if (name == ":pointer?")
        return Object::make_boolean(true);
    if (name == ":load-size")
        return Object::make_integer(this->get_load_size());
    if (name == "r:eg-class") {
        // Допустим, у нас есть маппинг энума в строку
        return Object::make_string(reg_kind_to_string(this->get_preferred_reg_class()));
    }
    if (name == ":is-reference")
        return Object::make_boolean(true);
    // 2. Если это не наше, просим ответить базовый класс Type
    // Важно: мы ВОЗВРАЩАЕМ результат этого вызова
    return Type::get_at(key);
}

bool ReferenceType::serialize_obj(Archive &ar, Object &data) {
    (void)ar;
    (void)data;
    return false;
}

// ============================================================================
// StructureType Implementation
// ============================================================================

StructureType::StructureType(std::string parent, std::string name, bool boxed, bool dynamic,
                             bool pack, int heap_base)
    : ReferenceType(std::move(parent), std::move(name), boxed, heap_base), m_dynamic(dynamic),
      m_pack(pack) {}

std::string StructureType::print() const {
    std::string result =
        fmt::format("[StructureType] {}\n parent: {}\n boxed: {}\n dynamic: {}\n size: {}\n pack: "
                    "{}\n fields:\n",
                    m_name, m_parent, m_is_boxed, m_dynamic, m_size_in_mem, m_pack);

    for (const auto &field : m_fields) {
        result += "   " + field.print() + "\n";
    }
    result += " methods:\n" + print_method_info();
    return result;
}

void StructureType::inherit(StructureType *parent) {
    if (!parent)
        return;
    if (Type::verbose) {
        fmt::print("DEBUG: Inheriting from {} to {}\n", parent->get_name(), get_name());
        fmt::print("DEBUG: Parent has {} fields, size: {}\n", parent->fields().size(),
                   parent->get_size_in_memory());
    }

    // Копируем ВСЕ поля родителя
    m_fields = parent->fields();                  // Копируем поля
    m_size_in_mem = parent->get_size_in_memory(); // Наследуем размер
    m_dynamic = parent->is_dynamic();
    m_idx_of_first_unique_field = m_fields.size();

    if (Type::verbose) {
        fmt::print("DEBUG: After inheritance - {} has {} fields, size: {}\n", get_name(),
                   m_fields.size(), m_size_in_mem);
    }
}

bool StructureType::operator==(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }

    const StructureType *other_struct = dynamic_cast<const StructureType *>(&other);
    return common_type_info_equal(other) && m_fields == other_struct->m_fields &&
           m_dynamic == other_struct->m_dynamic && m_size_in_mem == other_struct->m_size_in_mem &&
           m_pack == other_struct->m_pack && m_allow_misalign == other_struct->m_allow_misalign &&
           m_offset == other_struct->m_offset &&
           m_always_stack_singleton == other_struct->m_always_stack_singleton;
}

std::string StructureType::diff_impl(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return incompatible_diff(other);
    }

    const StructureType *other_struct = dynamic_cast<const StructureType *>(&other);
    return diff_structure_common(*other_struct);
}

std::string StructureType::diff_structure_common(const StructureType &other) const {
    std::string result;

    if (m_fields != other.m_fields) {
        result += "Fields differ\n";
        // TODO: Add detailed field comparison
    }
    if (m_dynamic != other.m_dynamic) {
        result += fmt::format("Dynamic: {} vs {}\n", m_dynamic, other.m_dynamic);
    }
    if (m_size_in_mem != other.m_size_in_mem) {
        result += fmt::format("Size in memory: {} vs {}\n", m_size_in_mem, other.m_size_in_mem);
    }
    if (m_pack != other.m_pack) {
        result += fmt::format("Pack: {} vs {}\n", m_pack, other.m_pack);
    }

    return result;
}

bool StructureType::lookup_field(const std::string &name, Field *out) {
    for (auto &field : m_fields) {
        if (field.name() == name) {
            if (out)
                *out = field;
            return true;
        }
    }
    return false;
}

void StructureType::override_field_type(const std::string &field_name, const TypeSpec &new_type) {
    for (auto &field : m_fields) {
        if (field.name() == field_name) {
            field.set_override_type(new_type);
            break;
        }
    }
}

Object StructureType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Специфические свойства структуры (динамическая проверка)
    if (name == ":dynamic?")
        return Object::make_boolean(this->is_dynamic());
    if (name == ":packed?")
        return Object::make_boolean(this->is_packed());
    if (name == ":always-stack-singleton?")
        return Object::make_boolean(this->is_always_stack_singleton());
    if (name == ":fields-count")
        return Object::make_integer(this->fields().size());
    if (name == ":first-unique-field-idx")
        return Object::make_integer(this->first_unique_field_idx());
    if (name == ":fields") {
        ListBuilder lb;
        for (auto &field : m_fields) {
            // 1. Приводим к неконстантному указателю (const_cast),
            // так как HeapObject ожидает владения, но мы его обманем.
            // 2. Добавляем лямбду-пустышку [](Field*){}, чтобы shared_ptr ничего не удалял.
            auto field_ptr = std::shared_ptr<Field>(const_cast<Field *>(&field), [](Field *) {});
            lb.add(Object::make_heap_obj(field_ptr));
        }
        return lb.build();
    }
    if (name == ":field-names") {
        ListBuilder lb;
        for (auto &field : m_fields) {
            // 1. Приводим к неконстантному указателю (const_cast),
            // так как HeapObject ожидает владения, но мы его обманем.
            // 2. Добавляем лямбду-пустышку [](Field*){}, чтобы shared_ptr ничего не удалял.
            lb.add(Object::make_symbol(field.name().c_str()));
        }
        return lb.build();
    }

    for (auto &field : m_fields) {
        if (field.name() == name) {
            auto field_ptr = std::shared_ptr<Field>(const_cast<Field *>(&field), [](Field *) {});
            return Object::make_heap_obj(field_ptr);
        }
    }

    // 2. Если это не "структурное" свойство, передаем запрос родителю.
    // ReferenceType проверит "heap-base", "pointer?", "load-size":
    // Если и он не найдет, запрос уйдет в Type за "name", "size" и т.д.
    return ReferenceType::get_at(key);
}
bool StructureType::serialize_obj(Archive &ar, Object &data) {
    if (ar.is_reading()) {
        // ============================================================
        // РЕЖИМ ЧТЕНИЯ: читаем данные в том порядке, как определены поля
        // ============================================================

        // Создаем пустой property list
        data = Object::make_null();

        // Читаем поля в порядке их определения в структуре
        for (const auto &field : fields()) {
            // Получаем тип поля
            Type *field_type = TypeSystem::instance().lookup_type(field.type().base_type());
            if (!field_type) {
                throw std::runtime_error("StructureType: unknown field type " +
                                         field.type().base_type() + " in " + get_name());
            }

            // Читаем значение поля
            Object field_value;
            field_type->serialize_obj(ar, field_value);

            // Добавляем в property list в формате (:field-name value)
            data = Object::make_pair(
                Object::make_pair(Object::make_keyword(field.name()),
                                  Object::make_pair(field_value, Object::make_null())),
                data);
        }

        return true;

    } else {
        // ============================================================
        // РЕЖИМ ЗАПИСИ: пишем данные в том порядке, как определены поля
        // ============================================================

        if (!data.is_pair() && !data.is_null()) {
            throw std::runtime_error("StructureType: expected property list");
        }

        // Для каждого поля структуры
        for (const auto &field : fields()) {
            // Ищем значение поля в property list
            Object field_value = PairObject::plist_get(data, field.name(), true);

            // Получаем тип поля
            Type *field_type = TypeSystem::instance().lookup_type(field.type().base_type());
            if (!field_type) {
                throw std::runtime_error("StructureType: unknown field type " +
                                         field.type().base_type() + " in " + get_name());
            }

            // Если поле не найдено в property list, передаем null
            // (тип поля сам решит, что с этим делать)
            if (field_value.is_null()) {
                field_value = Object::make_null();
            }

            // Пишем значение поля
            field_type->serialize_obj(ar, field_value);
        }

        return false;
    }
}

// ============================================================================
// BasicType Implementation
// ============================================================================

BasicType::BasicType(std::string parent, std::string name, bool dynamic, int heap_base)
    : StructureType(std::move(parent), std::move(name), true, dynamic, false, heap_base) {}

std::string BasicType::print() const {
    std::string result = fmt::format(
        "[BasicType] {}\n parent: {}\n dynamic: {}\n size: {}\n heap-base: {}\n fields:\n", m_name,
        m_parent, m_dynamic, m_size_in_mem, m_heap_base);

    for (const auto &field : m_fields) {
        result += "   " + field.print() + "\n";
    }
    result += " methods:\n" + print_method_info();
    return result;
}

bool BasicType::operator==(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }

    const BasicType *other_basic = dynamic_cast<const BasicType *>(&other);
    return StructureType::operator==(other) && m_final == other_basic->m_final;
}

std::string BasicType::diff_impl(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return incompatible_diff(other);
    }

    const BasicType *other_basic = dynamic_cast<const BasicType *>(&other);
    std::string      result = diff_structure_common(*other_basic);

    if (m_final != other_basic->m_final) {
        result += fmt::format("Final: {} vs {}\n", m_final, other_basic->m_final);
    }

    return result;
}

Object BasicType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем свойства, специфичные для BasicType
    if (name == ":final?")
        return Object::make_boolean(this->final());

    // 2. Если это не наше, пробрасываем запрос ВВЕРХ по цепочке:
    // BasicType -> StructureType -> ReferenceType -> Type
    // ОБЯЗАТЕЛЬНО используем return, чтобы результат прошел обратно к пользователю.
    return StructureType::get_at(key);
}

/*!
 * Serialize basic type
 */
bool BasicType::serialize_obj(Archive &ar, Object &data) {
    if (ar.is_reading()) {
        // ============================================================
        // РЕЖИМ ЧТЕНИЯ: создаем объект по type_name из архива
        // ============================================================

        // Читаем type_name как CompactCrc32
        CompactCrc32 type_crc;
        ar << type_crc;

        // Ищем тип по CRC
        Type *type = TypeSystem::instance().lookup_type_by_crc(type_crc.value);
        if (!type) {
            throw std::runtime_error("BasicType: unknown type CRC " +
                                     std::to_string(type_crc.value));
        }

        // Создаем экземпляр этого типа
        Object structure_data;
        if (!StructureType::serialize_obj(ar, structure_data)) {
            return false;
        }

        // Сериализуем сам объект через его тип
        Object::make_pair(Object::make_keyword("_type_"),
                          Object::make_pair(Object::make_symbol(type->get_name()), structure_data));

        return true;
    } else {
        // ============================================================
        // РЕЖИМ ЗАПИСИ: сохраняем type_name и делегируем StructureType
        // ============================================================

        // Пишем CRC типа
        uint32_t     type_crc = util::compute_crc32(get_name());
        CompactCrc32 crc(type_crc);
        ar << crc;

        // Делегируем сериализацию самому типу
        // (для структур это будет StructureType::serialize_obj)
        return StructureType::serialize_obj(ar, data);
    }
}

// ============================================================================
// BitField Implementation
// ============================================================================

BitField::BitField(TypeSpec type, std::string name, int offset, int size, bool skip_in_decomp)
    : m_type(std::move(type)), m_name(std::move(name)), m_offset(offset), m_size(size),
      m_skip_in_static_decomp(skip_in_decomp) {}

bool BitField::operator==(const BitField &other) const {
    return m_type == other.m_type && m_name == other.m_name && m_offset == other.m_offset &&
           m_size == other.m_size;
}

std::string BitField::print() const {
    return fmt::format("[{} {}] sz {} off {}", m_name, m_type.print(), m_size, m_offset);
}

bool BitField::operator!=(const BitField &other) const {
    return !(*this == other);
}

Object BitField::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Прямые свойства BitField
    if (name == ":name")
        return Object::make_string(this->name());
    if (name == ":offset")
        return Object::make_integer(this->offset());
    if (name == ":size")
        return Object::make_integer(this->size());
    if (name == ":skip-decomp?")
        return Object::make_boolean(this->skip_in_decomp());

    // 2. Сложные поля (рекурсия через HeapObject)
    if (name == ":type") {
        return Object::make_heap_obj(std::make_shared<TypeSpec>(this->type()));
    }

    // 3. Базовый класс (если BitField наследуется от HeapObject/HeapObject)
    // Просто возвращаем undefined, так как у родителя нет свойств.
    return Object::make_none();
}

// ============================================================================
// BitFieldType Implementation
// ============================================================================

BitFieldType::BitFieldType(std::string parent, std::string name, int size, bool sign_extend)
    : ValueType(std::move(parent), std::move(name), false, size, sign_extend, RegClass::GPR_64) {}

bool BitFieldType::lookup_field(const std::string &name, BitField *out) const {
    for (const auto &field : m_fields) {
        if (field.name() == name) {
            if (out)
                *out = field;
            return true;
        }
    }
    return false;
}

std::string BitFieldType::print() const {
    std::string result = fmt::format("[BitFieldType] {}\nFields:\n", m_name);
    for (const auto &field : m_fields) {
        result += "  " + field.print() + "\n";
    }
    return result;
}

bool BitFieldType::operator==(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }

    const BitFieldType *other_bitfield = dynamic_cast<const BitFieldType *>(&other);
    return common_type_info_equal(other) && m_fields == other_bitfield->m_fields;
}

std::string BitFieldType::diff_impl(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return incompatible_diff(other);
    }

    const BitFieldType *other_bitfield = dynamic_cast<const BitFieldType *>(&other);
    std::string         result;

    if (m_fields != other_bitfield->m_fields) {
        result += "Bitfield fields differ\n";
    }

    return result;
}

Object BitFieldType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Проверяем специфику BitFieldType
    if (name == ":fields-count") {
        return Object::make_integer(this->fields().size());
    }

    // 2. Если пользователь хочет получить список всех бит-полей
    if (name == ":fields") {
        ListBuilder lb;
        for (const auto &bf : m_fields) {
            // Создаем HeapObject на BitField.
            // Используем shared_ptr, чтобы объект жил, пока на него ссылается Лисп.
            // Если BitField хранятся в векторе по значению, создаем копию в куче:
            auto bf_ptr = std::make_shared<BitField>(bf);
            lb.add(Object::make_heap_obj(bf_ptr, ObjectType::NATIVE_OBJECT));
        }
        return lb.build();
    }

    // 3. Пробрасываем запрос ВВЕРХ: ValueType -> Type
    // Теперь цепочка полная: (-> bitfield-type 'size) придет сюда,
    // поймет что это не fields-count, и уйдет в ValueType.
    return ValueType::get_at(key);
}

bool BitFieldType::serialize_obj(Archive &ar, Object &data) {
    if (ar.is_reading()) {
        // ============================================================
        // РЕЖИМ ЧТЕНИЯ: из архива в Object
        // ============================================================

        uint64_t raw_value = 0;

        // Читаем raw значение
        switch (get_load_size()) {
        case 1: {
            uint8_t v;
            ar << v;
            raw_value = v;
            break;
        }
        case 2: {
            uint16_t v;
            ar << v;
            raw_value = v;
            break;
        }
        case 4: {
            uint32_t v;
            ar << v;
            raw_value = v;
            break;
        }
        case 8: {
            uint64_t v;
            ar << v;
            raw_value = v;
            break;
        }
        default:
            throw std::runtime_error("BitFieldType: unsupported size " +
                                     std::to_string(get_load_size()));
        }

        // Преобразуем в список установленных флагов
        std::vector<Object> flags;
        for (const auto &field : m_fields) {
            uint64_t mask = ((1ULL << field.size()) - 1) << field.offset();
            if ((raw_value & mask) != 0) {
                flags.push_back(Object::make_symbol(field.name()));
            }
        }

        if (flags.empty()) {
            // Если нет флагов - возвращаем 0
            data = Object::make_integer(0);
        } else if (flags.size() == 1) {
            // Если один флаг - возвращаем символ
            data = flags[0];
        } else {
            // Если несколько - возвращаем список
            data = Object::make_list(flags);
        }

        return true;

    } else {
        // ============================================================
        // РЕЖИМ ЗАПИСИ: из Object в архив
        // ============================================================
        uint64_t value_to_write = 0;

        if (data.is_integer()) {
            value_to_write = data.as_integer();

        } else if (data.is_symbol() || data.is_string()) { // ДОБАВИТЬ строки
            std::string name = data.to_std_string();
            bool        found = false;
            for (const auto &field : m_fields) {
                if (field.name() == name) {
                    uint64_t mask = ((1ULL << field.size()) - 1) << field.offset();
                    value_to_write = mask; // для одного флага - присваивание
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("BitFieldType: unknown flag '" + name + "'");
            }

        } else if (data.is_pair() || data.is_list()) {
            Object current = data;
            while (current.is_pair()) {
                Object item = current.as_pair()->car;
                if (item.is_symbol() || item.is_string()) { // ДОБАВИТЬ строки
                    std::string name = item.to_std_string();
                    for (const auto &field : m_fields) {
                        if (field.name() == name) {
                            uint64_t mask = ((1ULL << field.size()) - 1) << field.offset();
                            value_to_write |= mask; // для списка - OR
                            break;
                        }
                    }
                }
                current = current.as_pair()->cdr;
            }

        } else {
            throw std::runtime_error("BitFieldType: expected integer, symbol, string, or list");
        }

        // Записываем raw значение
        switch (get_load_size()) {
        case 1: {
            uint8_t v = value_to_write;
            ar << v;
            break;
        }
        case 2: {
            uint16_t v = value_to_write;
            ar << v;
            break;
        }
        case 4: {
            uint32_t v = value_to_write;
            ar << v;
            break;
        }
        case 8: {
            uint64_t v = value_to_write;
            ar << v;
            break;
        }
        default:
            throw std::runtime_error("BitFieldType: unsupported size " +
                                     std::to_string(get_load_size()));
        }

        return false;
    }
}

// ============================================================================
// EnumType Implementation
// ============================================================================

EnumType::EnumType(const ValueType *parent, std::string name, bool is_bitfield,
                   const std::unordered_map<std::string, int64_t> &entries)
    : ValueType(parent->get_parent(), std::move(name), parent->is_boxed(), parent->get_load_size(),
                parent->get_load_signed(), parent->get_preferred_reg_class()),
      m_is_bitfield(is_bitfield), m_entries(entries) {}

std::string EnumType::print() const {
    return fmt::format("[EnumType] {}", m_name);
}

bool EnumType::operator==(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }

    const EnumType *other_enum = dynamic_cast<const EnumType *>(&other);
    return common_type_info_equal(other) && m_entries == other_enum->m_entries &&
           m_is_bitfield == other_enum->m_is_bitfield;
}

std::string EnumType::diff_impl(const Type &other) const {
    if (typeid(*this) != typeid(other)) {
        return incompatible_diff(other);
    }

    const EnumType *other_enum = dynamic_cast<const EnumType *>(&other);
    std::string     result;

    if (m_is_bitfield != other_enum->m_is_bitfield) {
        result += fmt::format("Is bitfield: {} vs {}\n", m_is_bitfield, other_enum->m_is_bitfield);
    }

    if (m_entries != other_enum->m_entries) {
        result += "Enum entries differ\n";
    }

    return result;
}

Object EnumType::get_at(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Специфические свойства EnumType
    if (name == ":bitfield-enum?")
        return Object::make_boolean(this->is_bitfield());
    if (name == ":entries-count")
        return Object::make_integer(this->entries().size());

    // 2. Возможность получить конкретное значение по имени из Лиспа
    // Например: (-> my-enum 'MY_VALUE) вернет число
    auto it = this->entries().find(name);
    if (it != this->entries().end()) {
        return Object::make_integer(it->second);
    }

    // 3. Если это не мета-свойство и не элемент энума, идем вверх:
    // EnumType -> ValueType -> Type
    return ValueType::get_at(key);
}

/*!
 * Get name for value
 */
std::string EnumType::get_name_for_value(int64_t value) const {
    for (const auto &[name, val] : m_entries) {
        if (val == value) {
            return name;
        }
    }
    return "";
}

bool EnumType::serialize_obj(Archive &ar, Object &data) {
    if (ar.is_reading()) {
        // Читаем сырое значение через ValueType
        Object raw_value;
        ValueType::serialize_obj(ar, raw_value); // читает CompactCrc32 + число

        if (!raw_value.is_number()) {
            throw std::runtime_error("EnumType: expected number from archive");
        }

        int64_t num = raw_value.as_integer();

        // Преобразуем число в символ если есть имя
        std::string name = get_name_for_value(num);
        if (!name.empty()) {
            data = Object::make_symbol(name);
        } else {
            data = raw_value; // оставляем числом
        }
        return true;

    } else {
        // Преобразуем символ в число если нужно
        int64_t value_to_write;

        if (data.is_symbol()) {
            std::string name = data.to_std_string();
            auto        it = m_entries.find(name);
            if (it != m_entries.end()) {
                value_to_write = it->second;
            } else {
                throw std::runtime_error("EnumType: unknown enumerator " + name);
            }
        } else if (data.is_number()) {
            value_to_write = data.as_integer();
        } else {
            throw std::runtime_error("EnumType: expected symbol or number");
        }

        // Пишем через ValueType
        Object num_obj = Object::make_integer(value_to_write);
        return ValueType::serialize_obj(ar, num_obj); // пишет CompactCrc32 + число
    }
}

// ============================================================================
// MethodInfo full implementation
// ============================================================================

std::string MethodInfo::diff(const MethodInfo &other) const {
    std::string result;
    if (id != other.id) {
        result += fmt::format("id: {} vs. {}\n", id, other.id);
    }
    if (name != other.name) {
        result += fmt::format("name: {} vs. {}\n", name, other.name);
    }
    if (type != other.type) {
        result += fmt::format("type: {} vs. {}\n", type.print(), other.type.print());
    }
    if (defined_in_type != other.defined_in_type) {
        result +=
            fmt::format("defined_in_type: {} vs. {}\n", defined_in_type, other.defined_in_type);
    }
    if (no_virtual != other.no_virtual) {
        result += fmt::format("no_virtual: {} vs. {}\n", no_virtual, other.no_virtual);
    }
    if (overrides_parent != other.overrides_parent) {
        result +=
            fmt::format("overrides_parent: {} vs. {}\n", overrides_parent, other.overrides_parent);
    }
    if (only_overrides_docstring != other.only_overrides_docstring) {
        result += fmt::format("only_overrides_docstring: {} vs. {}\n", only_overrides_docstring,
                              other.only_overrides_docstring);
    }
    return result;
}

// ============================================================================
// Field full implementation
// ============================================================================

std::string Field::diff(const Field &other) const {
    std::string result;
    if (m_name != other.m_name) {
        result += fmt::format("name: {} vs. {}\n", m_name, other.m_name);
    }
    if (m_type != other.m_type) {
        result += fmt::format("type: {} vs. {}\n", m_type.print(), other.m_type.print());
    }
    if (m_offset != other.m_offset) {
        result += fmt::format("offset: {} vs. {}\n", m_offset, other.m_offset);
    }
    if (m_inline != other.m_inline) {
        result += fmt::format("inline: {} vs. {}\n", m_inline, other.m_inline);
    }
    if (m_dynamic != other.m_dynamic) {
        result += fmt::format("dynamic: {} vs. {}\n", m_dynamic, other.m_dynamic);
    }
    if (m_array != other.m_array) {
        result += fmt::format("array: {} vs. {}\n", m_array, other.m_array);
    }
    if (m_array_size != other.m_array_size) {
        result += fmt::format("array_size: {} vs. {}\n", m_array_size, other.m_array_size);
    }
    if (m_alignment != other.m_alignment) {
        result += fmt::format("alignment: {} vs. {}\n", m_alignment, other.m_alignment);
    }
    if (m_skip_in_static_decomp != other.m_skip_in_static_decomp) {
        result += fmt::format("skip_in_static_decomp: {} vs. {}\n", m_skip_in_static_decomp,
                              other.m_skip_in_static_decomp);
    }
    return result;
}
