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
Object MethodInfo::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Простые поля
    if (name == ".id")
        return Object::make_integer(this->id);
    if (name == ".name")
        return Object::make_string(this->name);
    if (name == ".defined-in")
        return Object::make_string(this->defined_in_type);
    if (name == ".type-name")
        return Object::make_string(this->type_name);

    // 2. Сложные поля (создаем объекты на лету)
    if (name == ".type") {
        // Оборачиваем TypeSpec. Теперь (-> method 'type 'base-type) сработает сам,
        // потому что у TypeSpec тоже будет свой make_step_accessor
        return Object::make_native_ref(std::make_shared<TypeSpec>(this->type));
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

Object Field::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Базовые свойства
    if (name == ".name")
        return Object::make_string(this->name());
    if (name == ".offset")
        return Object::make_integer(this->offset());
    if (name == ".alignment")
        return Object::make_integer(this->alignment());

    // 2. Тип (TypeSpec) — создаем NativeRef для дальнейшей навигации
    if (name == ".type") {
        return Object::make_native_ref(std::make_shared<TypeSpec>(this->type()));
    }

    // 3. Флаги состояния (теперь возвращают логический тип)
    if (name == ".inline?")
        return Object::make_boolean(this->is_inline());
    if (name == ".dynamic?")
        return Object::make_boolean(this->is_dynamic());
    if (name == ".array?")
        return Object::make_boolean(this->is_array());

    // 4. Специфичные поля
    if (name == ".array-size") {
        return Object::make_integer(this->is_array() ? this->array_size() : 0);
    }

    if (name == ".comment") {
        return this->has_comment() ? Object::make_string(this->comment()) : Object::make_null();
    }

    auto type = TypeSystem::instance().lookup_type(m_type);
    if (type) {
        return type->make_step_accessor(key);
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
// Used for the scripting systme
bool Type::set_method_by_id(int id, const script::Object method) {
    MethodInfo info;
    if (get_my_method(id, &info)) {
        info.method_impl = method;
        return true;
    }
    return false;
}
// Used for the scripting systme
bool Type::set_method_by_name(const std::string &name, const script::Object method) {
    MethodInfo info;
    if (get_my_method(name, &info)) {
        info.method_impl = method;
        return true;
    }
    return false;
}

bool Type::get_my_method(const std::string &name, MethodInfo *out) const {
    for (const auto &method : m_methods) {
        if (method.name == name) {
            if (out)
                *out = method;
            return true;
        }
    }
    return false;
}

bool Type::get_my_method(int id, MethodInfo *out) const {
    ASSERT(id > 0);
    for (const auto &method : m_methods) {
        if (method.id == id) {
            if (out)
                *out = method;
            return true;
        }
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

Object Type::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // Простое сравнение строк — это в разы быстрее, чем поиск в std::map<string, lambda>
    if (name == ".name")
        return Object::make_string(this->get_name());
    if (name == ".parent")
        return Object::make_string(this->get_parent());
    if (name == ".size")
        return Object::make_integer(this->get_size_in_memory());
    if (name == ".alignment")
        return Object::make_integer(this->get_in_memory_alignment());
    if (name == ".boxed?")
        return Object::make_boolean(this->is_boxed());
    if (name == ".methods-count")
        return Object::make_integer(this->get_num_methods());

    // Для специфических типов (например, StructureType) мы переопределим этот метод
    // и вызовем Type::make_step_accessor(key) в конце, если ничего не нашли.
    return Object::make_none();
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

Object NullType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем свои специфичные свойства
    if (name == ".null?") {
        return Object::make_boolean(true); // Используем boolean вместо integer 1
    }

    // 2. Если это не наше свойство, пробрасываем вызов родителю (Type)
    // Это и есть настоящая мощь наследования в нашей системе аксессоров.
    return Type::make_step_accessor(key);
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

Object ValueType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Проверяем специфичные поля ValueType
    if (name == ".size")
        return Object::make_integer(this->m_size);
    if (name == ".sign-extend?")
        return Object::make_boolean(this->m_sign_extend);
    if (name == ".offset")
        return Object::make_integer(this->m_offset);

    if (name == ".reg-class") {
        // Допустим, у нас есть маппинг энума в строку
        return Object::make_string(reg_kind_to_string(this->get_preferred_reg_class()));
    }

    // 2. Если это не наше поле, просим родителя (Type) ответить.
    // Тот проверит "name", "parent", "alignment" и т.д.
    return Type::make_step_accessor(key);
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

Object ReferenceType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем то, что специфично для ссылок
    if (name == ".heap-base")
        return Object::make_integer(this->heap_base());
    if (name == ".pointer?")
        return Object::make_boolean(true);
    if (name == ".load-size")
        return Object::make_integer(this->get_load_size());
    if (name == "r:eg-class") {
        // Допустим, у нас есть маппинг энума в строку
        return Object::make_string(reg_kind_to_string(this->get_preferred_reg_class()));
    }
    // 2. Если это не наше, просим ответить базовый класс Type
    // Важно: мы ВОЗВРАЩАЕМ результат этого вызова
    return Type::make_step_accessor(key);
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

Object StructureType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Специфические свойства структуры (динамическая проверка)
    if (name == ".dynamic?")
        return Object::make_boolean(this->is_dynamic());
    if (name == ".packed?")
        return Object::make_boolean(this->is_packed());
    if (name == ".always-stack-singleton?")
        return Object::make_boolean(this->is_always_stack_singleton());
    if (name == ".fields-count")
        return Object::make_integer(this->fields().size());
    if (name == ".first-unique-field-idx")
        return Object::make_integer(this->first_unique_field_idx());
    if (name == ".fields") {
        ListBuilder lb;
        for (auto &field : m_fields) {
            // 1. Приводим к неконстантному указателю (const_cast),
            // так как NativeRef ожидает владения, но мы его обманем.
            // 2. Добавляем лямбду-пустышку [](Field*){}, чтобы shared_ptr ничего не удалял.
            auto field_ptr = std::shared_ptr<Field>(const_cast<Field *>(&field), [](Field *) {});
            lb.add(Object::make_native_ref(field_ptr));
        }
        return lb.build();
    }
    if (name == ".field-names") {
        ListBuilder lb;
        for (auto &field : m_fields) {
            // 1. Приводим к неконстантному указателю (const_cast),
            // так как NativeRef ожидает владения, но мы его обманем.
            // 2. Добавляем лямбду-пустышку [](Field*){}, чтобы shared_ptr ничего не удалял.
            lb.add(Object::make_symbol(field.name().c_str()));
        }
        return lb.build();
    }
    if (name == ".methods") {
        ListBuilder lb;

        // 1. Добавляем специальный метод 'new', если он определен
        if (m_new_method_info_defined) {
            auto method_ptr = std::shared_ptr<MethodInfo>(
                const_cast<MethodInfo *>(&m_new_method_info), [](MethodInfo *) {});
            lb.add(Object::make_native_ref(method_ptr));
        }

        // 2. Добавляем все остальные методы из вектора
        for (auto &method : m_methods) {
            auto method_ptr =
                std::shared_ptr<MethodInfo>(const_cast<MethodInfo *>(&method), [](MethodInfo *) {});
            lb.add(Object::make_native_ref(method_ptr));
        }

        return lb.build();
    }
    if (name == ".methods-names") {
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
    if (name == ".methods-ids") {
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
    if (name == ".methods-max-id") {
        int max_id = -1;
        if (m_new_method_info_defined)
            max_id = 0;
        for (auto &method : m_methods) {
            if (method.id > max_id)
                max_id = method.id;
        }
        return Object::make_integer(max_id);
    }
    if (name == "new") {
        if (m_new_method_info_defined) {
            auto method_ptr = std::shared_ptr<MethodInfo>(
                const_cast<MethodInfo *>(&m_new_method_info), [](MethodInfo *) {});
            return Object::make_native_ref(method_ptr);
        }
        return Object::make_none();
    }

    for (auto &field : m_fields) {
        if (field.name() == name) {
            auto field_ptr = std::shared_ptr<Field>(const_cast<Field *>(&field), [](Field *) {});
            return Object::make_native_ref(field_ptr);
        }
    }

    for (auto &method : m_methods) {
        if (method.name == name) {
            auto method_ptr =
                std::shared_ptr<MethodInfo>(const_cast<MethodInfo *>(&method), [](MethodInfo *) {});
            return Object::make_native_ref(method_ptr);
        }
    }
    // 2. Если это не "структурное" свойство, передаем запрос родителю.
    // ReferenceType проверит "heap-base", "pointer?", "load-size".
    // Если и он не найдет, запрос уйдет в Type за "name", "size" и т.д.
    return ReferenceType::make_step_accessor(key);
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

Object BasicType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Сначала проверяем свойства, специфичные для BasicType
    if (name == ".final?")
        return Object::make_boolean(this->final());
    if (name == ".class-name")
        return Object::make_string(this->get_class_name());

    // 2. Если это не наше, пробрасываем запрос ВВЕРХ по цепочке:
    // BasicType -> StructureType -> ReferenceType -> Type
    // ОБЯЗАТЕЛЬНО используем return, чтобы результат прошел обратно к пользователю.
    return StructureType::make_step_accessor(key);
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
Object BitField::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Прямые свойства BitField
    if (name == ".name")
        return Object::make_string(this->name());
    if (name == ".offset")
        return Object::make_integer(this->offset());
    if (name == ".size")
        return Object::make_integer(this->size());
    if (name == ".skip-decomp?")
        return Object::make_boolean(this->skip_in_decomp());

    // 2. Сложные поля (рекурсия через NativeRef)
    if (name == ".type") {
        return Object::make_native_ref(std::make_shared<TypeSpec>(this->type()));
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

Object BitFieldType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Проверяем специфику BitFieldType
    if (name == ".fields-count") {
        return Object::make_integer(this->fields().size());
    }

    // 2. Если пользователь хочет получить список всех бит-полей
    if (name == ".fields") {
        ListBuilder lb;
        for (const auto &bf : m_fields) {
            // Создаем NativeRef на BitField.
            // Используем shared_ptr, чтобы объект жил, пока на него ссылается Лисп.
            // Если BitField хранятся в векторе по значению, создаем копию в куче:
            auto bf_ptr = std::make_shared<BitField>(bf);
            lb.add(Object::make_heap_object(bf_ptr, ObjectType::NATIVE_REF));
        }
        return lb.build();
    }

    // 3. Пробрасываем запрос ВВЕРХ: ValueType -> Type
    // Теперь цепочка полная: (-> bitfield-type 'size) придет сюда,
    // поймет что это не fields-count, и уйдет в ValueType.
    return ValueType::make_step_accessor(key);
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

Object EnumType::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Специфические свойства EnumType
    if (name == ".bitfield-enum?")
        return Object::make_boolean(this->is_bitfield());
    if (name == ".entries-count")
        return Object::make_integer(this->entries().size());

    // 2. Возможность получить конкретное значение по имени из Лиспа
    // Например: (-> my-enum 'MY_VALUE) вернет число
    auto it = this->entries().find(name);
    if (it != this->entries().end()) {
        return Object::make_integer(it->second);
    }

    // 3. Если это не мета-свойство и не элемент энума, идем вверх:
    // EnumType -> ValueType -> Type
    return ValueType::make_step_accessor(key);
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
