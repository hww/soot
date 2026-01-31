#include "common/type_system/TypeSystem.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"  
#include "fmt/format.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

    template <typename... Args>
    [[noreturn]] void throw_typesystem_error(const std::string& str, Args&&... args) {
        throw std::runtime_error(
            fmt::format("Type Error: {}", fmt::format(fmt::runtime(str), std::forward<Args>(args)...)));
    }
    std::vector<FieldReverseLookupOutput::Token> parent_to_vector(const ReverseLookupNode* parent) {
        if (!parent) {
            return {};
        }
        return parent->to_vector();
    }
}  // namespace

// ============================================================================
// TypeSystem Constructor
// ============================================================================

TypeSystem::TypeSystem() {

    // Add basic null types
    // add_type("none", std::make_unique<NullType>("none"));
    // add_type("_type_", std::make_unique<NullType>("_type_"));
    // add_type("_varargs_", std::make_unique<NullType>("_varargs_"));
}

// ============================================================================
// Type Management
// ============================================================================

Type* TypeSystem::add_type(const std::string& name, std::unique_ptr<Type> type) {
    // Check forward declared method counts
    auto method_kv = m_forward_declared_method_counts.find(name);
    if (method_kv != m_forward_declared_method_counts.end()) {
        int method_count = get_next_method_id(type.get());
        if (method_count != method_kv->second) {
            throw_typesystem_error(
                "Type {} was defined with {} methods, but was forward declared with {}",
                name, method_count, method_kv->second);
        }
    }

    auto kv = m_types.find(name);
    if (kv != m_types.end()) {
        // Type already exists
        if (*kv->second != *type) {
            // Type is being redefined differently
            // ДОБАВЛЕНО: Разрешаем переопределение null типов и идентичных типов
            if (dynamic_cast<NullType*>(type.get()) && dynamic_cast<NullType*>(kv->second.get())) {
                // Null types are always compatible, allow redefinition
                return kv->second.get();
            }

            if (m_allow_redefinition ||
                std::find(m_types_allowed_to_be_redefined.begin(),
                    m_types_allowed_to_be_redefined.end(), name) !=
                m_types_allowed_to_be_redefined.end()) {

                // Keep old type for reference
                m_old_types.push_back(std::move(m_types[name]));
                // Update with new type
                m_types[name] = std::move(type);
            }
            else {
                throw_typesystem_error(
                    "Inconsistent type definition. Type {} was originally:\n{}\n"
                    "and is redefined as:\n{}\nDiff:\n{}",
                    name, kv->second->print(), type->print(),
                    kv->second->diff(*type));
            }
        }
        else {
            // Types are identical, return existing
            return kv->second.get();
        }
    }
    else {
        // New type
        if (name != "object" && name != "none" && name != "_type_" && name != "_varargs_") {
            // ДОБАВЛЕНО: Проверяем forward declared types более аккуратно
            auto fwd_parent_it = m_forward_declared_types.find(type->get_parent());
            if (fwd_parent_it != m_forward_declared_types.end()) {
                throw_typesystem_error(
                    "Cannot create new type {}. The parent type {} is not fully defined.",
                    name, type->get_parent());
            }

            auto parent_it = m_types.find(type->get_parent());
            if (parent_it == m_types.end()) {
                // ДОБАВЛЕНО: Для bitfield разрешаем создание без родителя
                if (type->get_parent() != "bitfield") {
                    throw_typesystem_error(
                        "Cannot create new type {}. The parent type {} is not defined.",
                        name, type->get_parent());
                }
            }
        }

        m_types[name] = std::move(type);

        // Check forward declarations
        auto fwd_it = m_forward_declared_types.find(name);
        if (fwd_it != m_forward_declared_types.end()) {
            if (!tc(TypeSpec(fwd_it->second), TypeSpec(name))) {
                throw_typesystem_error(
                    "Type {} was originally declared as a child of {}, but is not.",
                    name, fwd_it->second);
            }
        }
        m_forward_declared_types.erase(name);
    }

    return m_types[name].get();
}

void TypeSystem::forward_declare_type_as_type(const std::string& name) {
    if (m_types.find(name) != m_types.end()) {
        return; // Already fully defined
    }

    auto it = m_forward_declared_types.find(name);
    if (it == m_forward_declared_types.end()) {
        m_forward_declared_types[name] = "object";
    }
    else {
        throw_typesystem_error(
            "Tried to forward declare {} as a type multiple times. Previous: {} Current: object",
            name, it->second);
    }
}

void TypeSystem::forward_declare_type_as(const std::string& new_type,
    const std::string& parent_type) {
    auto type_it = m_types.find(new_type);
    if (type_it != m_types.end()) {
        // Type already exists, verify parent
        if (!tc(TypeSpec(parent_type), TypeSpec(new_type))) {
            throw_typesystem_error(
                "Forward declaration that type {} is a {} disagrees with existing definition",
                new_type, parent_type);
        }
        return;
    }

    auto fwd_it = m_forward_declared_types.find(new_type);
    if (fwd_it == m_forward_declared_types.end()) {
        m_forward_declared_types[new_type] = parent_type;
    }
    else {
        if (fwd_it->second != parent_type) {
            throw_typesystem_error(
                "Got forward declaration that type {} is a {}, which disagrees with "
                "previous forward declaration that it was a {}",
                new_type, parent_type, fwd_it->second);
        }
    }
}

// ============================================================================
// Type Lookup
// ============================================================================

Type* TypeSystem::lookup_type(const std::string& name) const {
    auto kv = m_types.find(name);
    if (kv != m_types.end()) {
        return kv->second.get();
    }

    auto fd = m_forward_declared_types.find(name);
    if (fd != m_forward_declared_types.end()) {
        throw_typesystem_error("Type `{}` is not fully defined", name);
    }
    else {
        throw_typesystem_error("Type `{}` is not defined", name);
    }

    return nullptr;
}

Type* TypeSystem::lookup_type(const TypeSpec& ts) const {
    return lookup_type(ts.base_type());
}

Type* TypeSystem::lookup_type_no_throw(const std::string& name) const {
    auto kv = m_types.find(name);
    return (kv != m_types.end()) ? kv->second.get() : nullptr;
}

Type* TypeSystem::lookup_type_no_throw(const TypeSpec& ts) const {
    return lookup_type_no_throw(ts.base_type());
}

Type* TypeSystem::lookup_type_allow_partial_def(const std::string& name) const {
    // Try fully defined types first
    auto kv = m_types.find(name);
    if (kv != m_types.end()) {
        return kv->second.get();
    }

    // Follow forward declaration chain
    std::string current_name = name;
    while (true) {
        auto fwd_dec = m_forward_declared_types.find(current_name);
        if (fwd_dec == m_forward_declared_types.end()) {
            throw_typesystem_error("Type '{}' is unknown", name);
        }

        current_name = fwd_dec->second;
        auto type_lookup = m_types.find(current_name);
        if (type_lookup != m_types.end()) {
            return type_lookup->second.get();
        }
    }
}

Type* TypeSystem::lookup_type_allow_partial_def(const TypeSpec& ts) const {
    return lookup_type_allow_partial_def(ts.base_type());
}

// ============================================================================
// TypeSpec Creation
// ============================================================================

TypeSpec TypeSystem::make_typespec(const std::string& name) const {
    if (m_types.find(name) != m_types.end() ||
        m_forward_declared_types.find(name) != m_forward_declared_types.end()) {
        return TypeSpec(name);
    }
    else {
        throw_typesystem_error("Can't make typespec for unknow type `{}`", name);
    }
}

TypeSpec TypeSystem::make_pointer_typespec(const std::string& type) const {
    return make_pointer_typespec(make_typespec(type));
}

TypeSpec TypeSystem::make_pointer_typespec(const TypeSpec& type) const {
    return TypeSpec("pointer", { type });
}

TypeSpec TypeSystem::make_inline_array_typespec(const std::string& type) const {
    return make_inline_array_typespec(make_typespec(type));
}

TypeSpec TypeSystem::make_inline_array_typespec(const TypeSpec& type) const {
    return TypeSpec("inline-array", { type });
}

TypeSpec TypeSystem::make_array_typespec(const std::string& array_type,
    const TypeSpec& element_type) const {
    return TypeSpec(array_type, { element_type });
}

TypeSpec TypeSystem::make_function_typespec(const std::vector<std::string>& arg_types,
    const std::string& return_type) const {
    auto result = make_typespec("function");
    for (const auto& arg_type : arg_types) {
        result.add_arg(make_typespec(arg_type));
    }
    result.add_arg(make_typespec(return_type));
    return result;
}

// ============================================================================
// Type Checking
// ============================================================================

bool TypeSystem::typecheck_and_throw(const TypeSpec& expected,
    const TypeSpec& actual,
    const std::string& error_source_name,
    bool print_on_error,
    bool throw_on_error,
    bool allow_type_alias) const {
    bool success = true;

    // Check base types
    if (!typecheck_base_types(expected.base_type(), actual.base_type(), allow_type_alias)) {
        success = false;
    }

    // Check arguments
    if (expected.get_args_count() == actual.get_args_count()) {
        for (size_t i = 0; i < expected.get_args_count(); i++) {
            if (!tc(expected.get_arg(i), actual.get_arg(i))) {
                success = false;
                break;
            }
        }
    }
    else if (expected.get_args_count() != 0) {
        // Different sizes and we expected arguments
        success = false;
    }

    // Check tags - КАК У ОРИГИНАЛА
    for (const auto& tag : expected.tags()) {
        if (tag.name == "behavior") {
            auto got = actual.try_get_tag(tag.name);
            if (!got) {
                success = false;
            }
            else {
                // Используем TypeSpec для значений тега behavior
                TypeSpec expected_behavior(tag.value);
                TypeSpec actual_behavior(*got);
                if (!tc(expected_behavior, actual_behavior)) {
                    success = false;
                }
            }
        }
        else {
            throw_typesystem_error("Unknown tag {}", tag.name);
        }
    }

    if (!success) {
        if (print_on_error) {
            if (error_source_name.empty()) {
                fmt::print("[TypeSystem] Got type \"{}\" when expecting \"{}\"\n",
                    actual.print(), expected.print());
            }
            else {
                fmt::print("[TypeSystem] For {}, got type \"{}\" when expecting \"{}\"\n",
                    error_source_name, actual.print(), expected.print());
            }
        }

        if (throw_on_error) {
            throw std::runtime_error("typecheck failed");
        }
    }

    return success;
}

bool TypeSystem::tc(const TypeSpec& less_specific, const TypeSpec& more_specific) const {
    return typecheck_and_throw(less_specific, more_specific, "", false, false);
}

bool TypeSystem::typecheck_base_types(const std::string& expected,
    const std::string& actual,
    bool allow_alias) const {
    std::string exp = expected;
    std::string act = actual;

    // Handle unit types
    if (exp == "meters" || exp == "degrees") exp = "float";
    if (act == "meters" || act == "degrees") act = "float";

    if (allow_alias) {
        if (exp == "time-frame") exp = "int";
        if (act == "time-frame") act = "int";
    }

    // Make sure types exist
    lookup_type_allow_partial_def(exp);

    if (exp == act || exp == lookup_type_allow_partial_def(act)->get_name()) {
        return true;
    }

    // Check inheritance
    std::string current_actual = act;
    auto current_type = lookup_type_allow_partial_def(current_actual);

    while (current_type->has_parent()) {
        current_actual = current_type->get_parent();
        current_type = lookup_type_allow_partial_def(current_actual);

        if (exp == current_actual) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Method System
// ============================================================================

int TypeSystem::get_next_method_id(const Type* type) const {
    MethodInfo info;

    while (true) {
        if (type->get_my_last_method(&info)) {
            return info.id + 1;
        }

        if (type->has_parent()) {
            type = lookup_type(type->get_parent());
        }
        else {
            // No methods defined yet, start after new method
            return 1;
        }
    }
}

MethodInfo TypeSystem::declare_method(Type* type,
    const std::string& method_name,
    const std::optional<std::string>& docstring,
    bool no_virtual,
    const TypeSpec& ts,
    bool override_type) {
    if (method_name == "new") {
        if (override_type) {
            throw_typesystem_error("Cannot use :replace option with a new method");
        }
        return add_new_method(type, ts, docstring);
    }

    if (method_name == "delete" && !override_type) {
        // Можно добавить специальную логику для delete
        if (Type::verbose)
            fmt::print("DEBUG: Declaring delete method for type {}\n", type->get_name());
    }
    // Look for existing method
    MethodInfo existing_info;
    bool got_existing = try_lookup_method(type, method_name, &existing_info);

    if (override_type) {
        if (!got_existing) {
            throw_typesystem_error(
                "Cannot use :replace on method {} of {} because this method was not "
                "previously declared in a parent", method_name, type->get_name());
        }

        return type->add_method({ existing_info.id,
                                method_name,
                                ts,
                                type->get_name(),
                                type->get_name(),
                                no_virtual,
                                true,
                                false,
                                docstring,
                                std::nullopt });
    }
    else {
        if (got_existing) {
            // Verify compatibility
            if (!existing_info.type.is_compatible_child_method(ts, type->get_name())) {
                throw_typesystem_error(
                    "The method {} of type {} was originally declared as {}, but has been "
                    "redeclared as {}. Originally declared in {}",
                    method_name, type->get_name(), existing_info.type.print(), ts.print(),
                    existing_info.defined_in_type);
            }

            return existing_info;
        }
        else {
            // Add new method
            return type->add_method({ get_next_method_id(type),
                                    method_name,
                                    ts,
                                    type->get_name(),
                                    type->get_name(),
                                    no_virtual,
                                    false,
                                    false,
                                    docstring,
                                    std::nullopt });
        }
    }
}

MethodInfo TypeSystem::add_new_method(Type* type,
    const TypeSpec& ts,
    const std::optional<std::string>& docstring) {
    MethodInfo existing;
    if (type->get_my_new_method(&existing)) {
        // Verify compatibility
        if (!existing.type.is_compatible_child_method(ts, type->get_name())) {
            throw_typesystem_error(
                "Cannot add new method. Type does not match declaration. The new method of {} "
                "was originally defined as {}, but has been redefined as {}",
                type->get_name(), existing.type.print(), ts.print());
        }
        return existing;
    }
    else {
        return type->add_new_method(
            { 0, "new", ts, type->get_name(), type->get_name(), false, false, false,
            docstring, 
            std::nullopt });
    }
}

bool TypeSystem::try_lookup_method(const Type* type,
    const std::string& method_name,
    MethodInfo* info) const {
    while (true) {
        if (method_name == "new") {
            if (type->get_my_new_method(info)) {
                return true;
            }
        }
        else {
            if (type->get_my_method(method_name, info)) {
                return true;
            }
        }

        if (type->has_parent()) {
            type = lookup_type(type->get_parent());
        }
        else {
            break;
        }
    }
    return false;
}

MethodInfo TypeSystem::lookup_method(const std::string& type_name,
    const std::string& method_name) const {

    if (method_name == "new") {
        return lookup_new_method(type_name);
    }

    MethodInfo info;
    auto* type = lookup_type(type_name);

    while (true) {
        if (type->get_my_method(method_name, &info)) {
            return info;
        }

        if (type->has_parent()) {
            type = lookup_type(type->get_parent());
        }
        else {
            break;
        }
    }

    throw_typesystem_error("The method {} of type {} could not be found", method_name, type_name);
}

MethodInfo TypeSystem::lookup_new_method(const std::string& type_name) const {
    MethodInfo info;
    auto* type = lookup_type(type_name);

    while (true) {
        if (type->get_my_new_method(&info)) {
            return info;
        }

        if (type->has_parent()) {
            type = lookup_type(type->get_parent());
        }
        else {
            break;
        }
    }

    throw_typesystem_error("The new method of type {} could not be found", type_name);
}

// ============================================================================
// Field System
// ============================================================================

Field TypeSystem::lookup_field(const std::string& type_name,
    const std::string& field_name) const {
    auto type = get_type_of_type<StructureType>(type_name);
    Field field;
    if (!type->lookup_field(field_name, &field)) {
        throw_typesystem_error("Type {} has no field named {}", type_name, field_name);
    }
    return field;
}

FieldLookupInfo TypeSystem::lookup_field_info(const std::string& type_name,
    const std::string& field_name) const {
    FieldLookupInfo info;
    info.field = lookup_field(type_name, field_name);

    // Get array size for bounds checking
    if (info.field.is_array() && !info.field.is_dynamic()) {
        info.array_size = info.field.array_size();
    }

    auto base_type = lookup_type_allow_partial_def(info.field.type());
    if (base_type->is_reference()) {
        if (info.field.is_inline()) {
            if (info.field.is_array()) {
                // Inline array of reference types
                info.needs_deref = false;
                info.type = make_inline_array_typespec(info.field.type());
            }
            else {
                // Inline object
                info.needs_deref = false;
                info.type = info.field.type();
            }
        }
        else {
            if (info.field.is_array()) {
                info.needs_deref = false;
                info.type = make_pointer_typespec(info.field.type());
            }
            else {
                info.needs_deref = true;
                info.type = info.field.type();
            }
        }
    }
    else {
        // Value types
        if (info.field.is_array()) {
            info.needs_deref = false;
            info.type = make_pointer_typespec(info.field.type());
        }
        else {
            info.needs_deref = true;
            info.type = info.field.type();
        }
    }

    return info;
}

// В TypeSystem.cpp, в add_field_to_type
int TypeSystem::add_field_to_type(StructureType* type,
    const std::string& field_name,
    const TypeSpec& field_type,
    bool is_inline,
    bool is_dynamic,
    int array_size,
    int offset_override,
    bool skip_in_static_decomp,
    double score,
    const std::optional<TypeSpec> decomp_as_ts) {

    // Проверяем существование поля
    Field existing_field;
    if (type->lookup_field(field_name, &existing_field)) {
        throw_typesystem_error("Type {} already has a field named {}",
            type->get_name(), field_name);
    }

    // Создаем поле
    Field field(field_name, field_type);
    if (is_inline) field.set_inline();
    if (is_dynamic) {
        field.set_dynamic();
        type->set_dynamic();
    }
    if (array_size != -1) field.set_array(array_size);

    // ВЫЧИСЛЯЕМ СМЕЩЕНИЕ с учетом выравнивания
    int offset = offset_override;
    if (offset == -1) {
        // Автоматическое размещение
        offset = type->get_size_in_memory();
        int alignment = get_alignment_in_type(field);

        // ВЫРАВНИВАЕМ offset
        if (offset % alignment != 0) {
            offset = (offset + alignment - 1) & ~(alignment - 1);
        }
    }

    field.set_offset(offset);
    field.set_alignment(get_alignment_in_type(field));

    // ВЫЧИСЛЯЕМ РАЗМЕР ПОЛЯ
    int field_size = get_size_in_type(field);

    // ДЛЯ INLINE STRUCTURES: учитываем выравнивание самой структуры
    if (field.is_inline() && !field.is_array()) {
        auto field_type_obj = lookup_type_allow_partial_def(field.type());
        if (auto field_struct = dynamic_cast<StructureType*>(field_type_obj)) {
            int struct_alignment = field_struct->get_in_memory_alignment();
            int struct_size = field_struct->get_size_in_memory();

            // Выравниваем размер структуры
            if (struct_size % struct_alignment != 0) {
                struct_size = (struct_size + struct_alignment - 1) & ~(struct_alignment - 1);
            }

            // Обновляем field_size если нужно
            if (field_size != struct_size) {
                field_size = struct_size;
            }
        }
    }

    if (skip_in_static_decomp) {
        field.set_skip_in_static_decomp();
    }

    field.set_field_score(score);
    if (decomp_as_ts) {
        field.set_decomp_as_ts(*decomp_as_ts);
    }

    // ОБНОВЛЯЕМ РАЗМЕР СТРУКТУРЫ
    int after_field = offset + field_size;
    if (type->get_size_in_memory() < after_field) {
        type->override_size_in_memory(after_field);
    }

    // ДОБАВЛЯЕМ ПОЛЕ В СТРУКТУРУ
    type->m_fields.push_back(field);
    if (Type::verbose)
        fmt::print("DEBUG: Added field {} to type {}, offset: {}, size: {}, inline: {}, array: {}\n",
            field_name, type->get_name(), offset, field_size, is_inline, array_size);

    return offset;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool TypeSystem::fully_defined_type_exists(const std::string& name) const {
    return m_types.find(name) != m_types.end();
}

bool TypeSystem::fully_defined_type_exists(const TypeSpec& type) const {
    return fully_defined_type_exists(type.base_type());
}

bool TypeSystem::partially_defined_type_exists(const std::string& name) const {
    return m_forward_declared_types.find(name) != m_forward_declared_types.end();
}

std::string TypeSystem::get_runtime_type(const TypeSpec& ts) {
    return lookup_type(ts)->get_runtime_name();
}

std::string TypeSystem::print_all_type_information() const {
    std::string result;
    for (const auto& kv : m_types) {
        result += kv.second->print() + "\n";
    }
    return result;
}

script::Object TypeSystem::get_all_type_information() const {
    script::Object result = script::Object::make_null();
    for (const auto& kv : m_types) {
        result = script::Object::make_pair(script::Object::make_string(kv.second->print()), result);
    }
    return result;
}

// ============================================================================
// Built-in Type Factories (Simplified)
// ============================================================================

StructureType* TypeSystem::add_builtin_structure(const std::string& parent,
    const std::string& type_name,
    bool boxed) {

    auto structure = std::make_unique<StructureType>(parent, type_name, boxed, false, false, 0);
    StructureType* result = structure.get();
    add_type(type_name, std::move(structure));

    // ВЫЗЫВАЕМ НАСЛЕДОВАНИЕ если родитель существует
    if (parent != "object" && fully_defined_type_exists(parent)) {
        StructureType* parent_type = get_type_of_type<StructureType>(parent);
        result->inherit(parent_type);
    }

    return result;
}

BasicType* TypeSystem::add_builtin_basic(const std::string& parent,
    const std::string& type_name) {
    add_type(type_name, std::make_unique<BasicType>(parent, type_name, false, 0));
    return get_type_of_type<BasicType>(type_name);
}

ValueType* TypeSystem::add_builtin_value_type(const std::string& parent,
    const std::string& type_name,
    int size,
    bool boxed,
    bool sign_extend,
    RegClass reg) {
    add_type(type_name,
        std::make_unique<ValueType>(parent, type_name, boxed, size, sign_extend, reg));
    return get_type_of_type<ValueType>(type_name);
}

// ============================================================================
// Builting Types Tree
// object
// ├── number
// │   ├── integer
// │   │   ├── sinteger
// │   │   │   ├── int8
// │   │   │   ├── int16
// │   │   │   ├── int32
// │   │   │   └── int64
// │   │   ├── uinteger
// │   │   │   ├── uint8
// │   │   │   ├── uint16
// │   │   │   ├── uint32
// │   │   │   └── uint64
// │   │   ├── int(псевдоним)
// │   │   └── uint(псевдоним)
// │   └── float
// ├── structure
// ├── basic
// └── ...
// ============================================================================
void TypeSystem::add_builtin_types() {
    TypeConfig::pointer_reg_class = RegClass::GPR_64;
    TypeConfig::pointer_size = 4;
    TypeConfig::array_data_offset = 12;
    TypeConfig::default_alignment = 4;
    TypeConfig::crc_value_size = 4;
    TypeConfig::struct_alignment = 16;
    TypeConfig::struct_array_stride_alignment = 16;
    TypeConfig::struct_array_start_alignment = 16;
    TypeConfig::basic_array_start_alignment = 16;


    // Проверяем что базовые типы еще не инициализированы
    if (!m_types.empty() && m_types.find("object") != m_types.end()) {
        return; // Уже инициализированы
    }

    // Базовые null типы
    add_type("none", std::make_unique<NullType>("none"));
    add_type("_type_", std::make_unique<NullType>("_type_"));
    add_type("_varargs_", std::make_unique<NullType>("_varargs_"));

    // OBJECT - корневой тип
    auto obj_type = add_type("object", std::make_unique<ValueType>("object", "object", false, 4, true, RegClass::GPR_64));

    add_builtin_value_type("object", "pointer", 4);

    // Базовые структурные типы
    auto structure_type = add_builtin_structure("object", "structure");
    auto basic_type     = add_builtin_basic("structure", "basic");

    // BitFieldType должен быть создан ПЕРЕД другими типами, так как они могут на него ссылаться
    // Создаем BitFieldType как ValueType с правильными параметрами
    auto bitfield_type = add_type("bitfield", std::make_unique<ValueType>("object", "bitfield", false, 4, false, RegClass::GPR_64));

    // Базовые типы
    auto symbol_type   = add_builtin_basic("basic", "symbol");
    auto type_type     = add_builtin_basic("basic", "type");
    auto string_type   = add_builtin_basic("basic", "string");
    auto function_type = add_builtin_basic("basic", "function");



    // Матричный тип для тестов
    auto matrix_type = add_builtin_structure("structure", "matrix");

    // ПРАВИЛЬНАЯ числовая иерархия как в OpenGOAL:
    // object -> number -> integer -> sinteger -> int32/int64
    auto number_type  = add_builtin_value_type("object", "number", 8, false, false, RegClass::GPR_64);

    // float
    auto float_type   = add_builtin_value_type("number", "float", 4, false, false, RegClass::FPR);

    // integer
    auto integer_type = add_builtin_value_type("number", "integer", 8, false, false, RegClass::GPR_64);

    // signed integers
    auto sinteger_type = add_builtin_value_type("integer", "sinteger", 8, false, true, RegClass::GPR_64);
    auto int8_type  = add_builtin_value_type("sinteger", "int8", 1, false, true);
    auto int16_type = add_builtin_value_type("sinteger", "int16", 2, false, true);
    auto int32_type = add_builtin_value_type("sinteger", "int32", 4, false, true);
    auto int64_type = add_builtin_value_type("sinteger", "int64", 8, false, true);

    // unsigned integers
    auto uinteger_type = add_builtin_value_type("integer", "uinteger", 8, false, false, RegClass::GPR_64);
    auto uint8_type  = add_builtin_value_type("uinteger", "uint8", 1, false, false);
    auto uint16_type = add_builtin_value_type("uinteger", "uint16", 2, false, false);
    auto uint32_type = add_builtin_value_type("uinteger", "uint32", 4, false, false);
    auto uint64_type = add_builtin_value_type("uinteger", "uint64", 8, false, false);

    // Псевдонимы как в оригинале
    auto int_type = add_builtin_value_type("integer", "int", 8, false, true, RegClass::GPR_64);
    int_type->disallow_in_runtime();

    auto uint_type = add_builtin_value_type("uinteger", "uint", 8, false, false, RegClass::GPR_64);
    uint_type->disallow_in_runtime();

    // Предеклорация
    forward_declare_type_as("memory-usage-block", "basic");

    // Добавляем методы object
    declare_method(obj_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "int" }, "_type_"), false);
    declare_method(obj_type, "delete", {}, false,
        make_function_typespec({ "_type_" }, "none"), false);
    declare_method(obj_type, "print", {}, false,
        make_function_typespec({ "_type_" }, "_type_"), false);
    declare_method(obj_type, "inspect", {}, false,
        make_function_typespec({ "_type_" }, "_type_"), false);
    declare_method(obj_type, "length", {}, false,
        make_function_typespec({ "_type_" }, "int"), false);
    declare_method(obj_type, "asize-of", {}, false,
        make_function_typespec({ "_type_" }, "int"), false);
    declare_method(obj_type, "copy", {}, false,
        make_function_typespec({ "_type_", "symbol" }, "_type_"), false);
    declare_method(obj_type, "relocate", {}, false,
        make_function_typespec({ "_type_", "int" }, "_type_"), false);
    declare_method(obj_type, "mem-usage", {}, false,
        make_function_typespec({ "_type_", "memory-usage-block", "int" }, "_type_"), false);

    // Добавлегте полей
    add_field_to_type(basic_type, "type", make_typespec("type"));

    // TYPE
    builtin_structure_inherit(type_type);
    add_field_to_type(type_type, "symbol", make_typespec("symbol"));
    add_field_to_type(type_type, "parent", make_typespec("type"));
    add_field_to_type(type_type, "size",   make_typespec("uint16"));  // actually u16
    add_field_to_type(type_type, "psize",  make_typespec("uint16"));  // todo, u16 or s16. what really is this?
    add_field_to_type(type_type, "heap-base",        make_typespec("uint16"));         // todo
    add_field_to_type(type_type, "allocated-length", make_typespec("uint16"));  // todo
    add_field_to_type(type_type, "method-table",     make_typespec("function"), false, true);

    builtin_structure_inherit(symbol_type);
    add_field_to_type(symbol_type, "value", make_typespec("object"), 4);

    builtin_structure_inherit(string_type);
    add_field_to_type(string_type, "length", make_typespec("int32"), 4);
    add_field_to_type(string_type, "data", make_pointer_typespec("uint8"), 8, false, true); // dynamic



    if (Type::verbose)
        fmt::print("DEBUG: Builtin types initialized successfully\n");
    verify_type_sizes();
}

void TypeSystem::verify_type_sizes() {
    // Проверяем критические размеры
    auto check_size = [&](const std::string& name, size_t expected) {
        Type* type = lookup_type(name);
        if (type && type->get_size_in_memory() != expected) {
            fmt::print("[WARNING] Type '{}' has size {} but expected {}\n",
                      name, type->get_size_in_memory(), expected);
        }
    };
    
    check_size("object", 4);
    check_size("int8", 1);
    check_size("int16", 2);
    check_size("int", 8);
    check_size("uint8", 1);
    check_size("uint16", 2);
    check_size("uint", 8);
    check_size("basic", 4); 
    check_size("symbol", 8);
    check_size("string", 12);
    check_size("type", 20); 
}

// ============================================================================
// Builtin Types Tree (Z80 Optimized)
// object [2 bytes: pointer/offset]
// ├── number
// │   └── integer
// │       ├── sinteger
// │       │   ├── int8    [1 byte]
// │       │   ├── int16   [2 bytes, default 'int']
// │       │   └── int32   [4 bytes, compound]
// │       └── uinteger
// │           ├── uint8   [1 byte]
// │           ├── uint16  [2 bytes, default 'uint']
// │           └── uint32  [4 bytes, compound]
// ├── structure         [base for all structs]
// ├── basic             [base for 'tagged' objects]
// │   ├── symbol
// │   ├── string
// │   ├── type
// │   └── function
// └── bitfield          [for hardware registers/flags]
// ============================================================================
void TypeSystem::add_builtin_types_z80() {
    TypeConfig::pointer_reg_class = RegClass::GPR_16;
    TypeConfig::pointer_size = 2;
    TypeConfig::array_data_offset = 2;
    TypeConfig::default_alignment = 1;
    TypeConfig::crc_value_size = 2;
    TypeConfig::struct_alignment = 2;
    TypeConfig::struct_array_stride_alignment = 2;
    TypeConfig::struct_array_start_alignment = 2;
    TypeConfig::basic_array_start_alignment = 2;

    // 1. Технические типы
    add_type("none",   std::make_unique<NullType>("none"));
    add_type("_type_", std::make_unique<NullType>("_type_"));

    // 2. Указатель (object) - фундамент
    auto obj_type = add_type("object", 
        std::make_unique<ValueType>("object", "object", false, 2, true, RegClass::GPR_16));
    add_builtin_value_type("object", "pointer", 2);

    // 3. Числа
    add_builtin_value_type("object", "number", 2);
    add_builtin_value_type("number", "integer", 2);
    
    // signed integers
    add_builtin_value_type("integer", "int8", 1, false, true, RegClass::GPR_8);
    add_builtin_value_type("integer", "int16", 2, false, true, RegClass::GPR_16);
    add_builtin_value_type("integer", "int", 2, false, true, RegClass::GPR_16);
    
    // unsigned integers  
    add_builtin_value_type("integer", "uint8", 1, false, false, RegClass::GPR_8);
    add_builtin_value_type("integer", "uint16", 2, false, false, RegClass::GPR_16);
    add_builtin_value_type("integer", "uint", 2, false, false, RegClass::GPR_16);

    // Костыль для парсера
    auto i64 = add_builtin_value_type("integer", "int64", 8);
    i64->disallow_in_runtime();

    // 4. Структуры
    auto structure_type = add_builtin_structure("object", "structure");
    auto basic_type = add_builtin_basic("structure", "basic");
    
    // 5. Basic типы
    auto symbol_type   = add_builtin_basic("basic", "symbol");
    auto string_type   = add_builtin_basic("basic", "string");
    auto type_type     = add_builtin_basic("basic", "type");
    auto function_type = add_builtin_basic("basic", "function");
    string_type->set_final();  // string не имеет виртуальных методов в Z80

    // ============================================================================
    // КРИТИЧЕСКИ ВАЖНЫЕ ПОЛЯ ДЛЯ BASIC ТИПОВ
    // ============================================================================
    
    // BASIC: первые 2 байта - type tag (тип объекта)
    add_field_to_type(basic_type, "type", make_typespec("type"));
    
    // SYMBOL для Z80 (упрощенная версия)
    // symbol имеет: type (2), value (2) = всего 4 байта
    builtin_structure_inherit(symbol_type);
    add_field_to_type(symbol_type, "value", make_typespec("object"), 2); // offset 2
    
    // STRING для Z80 (упрощенная версия)
    // string имеет: type (2), length (2), data (указатель или inline) = 4+ байта
    builtin_structure_inherit(string_type);
    add_field_to_type(string_type, "length", make_typespec("uint16"), 2); // offset 2
    add_field_to_type(string_type, "data", make_pointer_typespec("uint8"), 4, false, true); // offset 4, dynamic
    
    // TYPE для Z80
    builtin_structure_inherit(type_type);
    add_field_to_type(type_type, "parent", make_typespec("type"), 2);    // offset 2
    add_field_to_type(type_type, "size", make_typespec("uint16"), 4);    // offset 4
    add_field_to_type(type_type, "psize", make_typespec("uint16"), 6);   // offset 6 (placeholder)
    add_field_to_type(type_type, "heap-base", make_typespec("uint16"), 8); // offset 8
    
    // FUNCTION для Z80
    builtin_structure_inherit(function_type);
    // function в Z80 может быть просто указателем на код

    // ============================================================================
    // КРИТИЧЕСКИ ВАЖНЫЕ МЕТОДЫ
    // ============================================================================
    
    // OBJECT методы
    declare_method(obj_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "int" }, "_type_"), false);
    declare_method(obj_type, "delete", {}, false, 
        make_function_typespec({ "_type_" }, "none"), false);
    declare_method(obj_type, "print", {}, false, 
        make_function_typespec({ "_type_" }, "_type_"), false);
    
    // STRUCTURE методы
    declare_method(structure_type, "new", {}, false,
        make_function_typespec({ "symbol", "type" }, "_type_"), false);
    
    // BASIC методы (наследует от structure)
    declare_method(basic_type, "new", {}, false,
        make_function_typespec({ "symbol", "type" }, "_type_"), false);
    
    // SYMBOL методы (нельзя создавать new)
    declare_method(symbol_type, "new", {}, false,
        make_function_typespec({}, "none"), false);
    
    // STRING методы (специальный конструктор)
    declare_method(string_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "int", "string" }, "_type_"), false);
    
    // TYPE методы
    declare_method(type_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "int" }, "_type_"), false);
    
    // ============================================================================
    // ДОПОЛНИТЕЛЬНЫЕ ТИПЫ ДЛЯ Z80
    // ============================================================================
    
    // pair для cons-ячеек
    auto pair_type = add_builtin_structure("object", "pair", true);
    pair_type->override_offset(2); // специальное смещение для pair
    add_field_to_type(pair_type, "car", make_typespec("object"), 0);
    add_field_to_type(pair_type, "cdr", make_typespec("object"), 2);
    declare_method(pair_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "object", "object" }, "_type_"), false);
    
    // array для массивов
    auto array_type = add_builtin_basic("basic", "array");
    builtin_structure_inherit(array_type);
    add_field_to_type(array_type, "length", make_typespec("int16"), 2);
    add_field_to_type(array_type, "data", make_typespec("uint8"), 4, false, true);
    declare_method(array_type, "new", {}, false,
        make_function_typespec({ "symbol", "type", "type", "int" }, "_type_"), false);
    
    // bitfield для аппаратных регистров
    add_builtin_value_type("object", "bitfield", 2);
    
    // enum для перечислений (наследует от соответствующих integer типов)
    // Определяются динамически через defenum
    
    // ============================================================================
    // ПРОВЕРКА РАЗМЕРОВ
    // ============================================================================
    
    // Проверяем что размеры типов правильные для Z80
    verify_type_sizes_z80();
}

void TypeSystem::verify_type_sizes_z80() {
    // Проверяем критические размеры
    auto check_size = [&](const std::string& name, size_t expected) {
        Type* type = lookup_type(name);
        if (type && type->get_size_in_memory() != expected) {
            fmt::print("[WARNING] Type '{}' has size {} but expected {}\n",
                      name, type->get_size_in_memory(), expected);
        }
    };
    
    check_size("object", 2);
    check_size("int8", 1);
    check_size("int16", 2);
    check_size("int", 2);
    check_size("uint8", 1);
    check_size("uint16", 2);
    check_size("uint", 2);
    check_size("basic", 2); // только type tag
    check_size("symbol", 4); // type (2) + value (2)
    check_size("string", 6); // type (2) + length (2), data отдельно
    check_size("type", 10); // type (2) + поля
    check_size("pair", 4); // car (2) + cdr (2)
}

// ============================================================================
// TypeSpec Coercion
// ============================================================================

TypeSpec coerce_to_reg_type(const TypeSpec& in) {
    // Simplified implementation
    if (in.get_args_count() == 0) {
        if (in.base_type() == "int8" || in.base_type() == "int16" ||
            in.base_type() == "int32" || in.base_type() == "int64") {
            return TypeSpec("int");
        }
        if (in.base_type() == "uint8" || in.base_type() == "uint16" ||
            in.base_type() == "uint32" || in.base_type() == "uint64") {
            return TypeSpec("uint");
        }
    }
    return in;
}

// ============================================================================
// Inheritance and Type Hierarchy
// ============================================================================

std::vector<std::string> TypeSystem::get_path_up_tree(const std::string& type) const {
    std::vector<std::string> path;
    std::string current = type;

    while (true) {
        path.push_back(current);
        auto current_type = lookup_type_allow_partial_def(current);
        if (!current_type->has_parent()) {
            break;
        }
        current = current_type->get_parent();
    }

    return path;
}

std::string TypeSystem::lca_base(const std::string& a, const std::string& b) const {
    if (a == b) {
        return a;
    }

    auto a_path = get_path_up_tree(a);
    auto b_path = get_path_up_tree(b);

    // Find common ancestor by comparing paths from root down
    int a_idx = a_path.size() - 1;
    int b_idx = b_path.size() - 1;

    std::string result = "object"; // fallback

    while (a_idx >= 0 && b_idx >= 0) {
        if (a_path[a_idx] == b_path[b_idx]) {
            result = a_path[a_idx];
            a_idx--;
            b_idx--;
        }
        else {
            break;
        }
    }

    return result;
}

TypeSpec TypeSystem::lowest_common_ancestor(const TypeSpec& a, const TypeSpec& b) const {
    // Handle base types
    auto result = make_typespec(lca_base(a.base_type(), b.base_type()));

    // Handle arguments recursively if compatible
    if (!a.empty() && !b.empty() && a.get_args_count() == b.get_args_count()) {
        for (size_t i = 0; i < a.get_args_count(); i++) {
            result.add_arg(lowest_common_ancestor(a.get_arg(i), b.get_arg(i)));
        }
    }

    return result;
}

TypeSpec TypeSystem::lowest_common_ancestor(const std::vector<TypeSpec>& types) const {
    ASSERT(!types.empty());
    if (types.size() == 1) {
        return types.front();
    }

    auto result = lowest_common_ancestor(types[0], types[1]);
    for (size_t i = 2; i < types.size(); i++) {
        result = lowest_common_ancestor(result, types[i]);
    }
    return result;
}

TypeSpec TypeSystem::lowest_common_ancestor_reg(const TypeSpec& a, const TypeSpec& b) const {
    return coerce_to_reg_type(lowest_common_ancestor(a, b));
}

// ============================================================================
// Memory Access and Dereferencing
// ============================================================================

DerefInfo TypeSystem::get_deref_info(const TypeSpec& ts) const {
    DerefInfo info;

    if (!ts.has_single_arg()) {
        info.can_deref = false;
        return info;
    }

    // Default to GPR
    info.reg = RegClass::GPR_64;
    info.mem_deref = true;

    if (ts.base_type() == "pointer") {
        info.can_deref = true;
        info.result_type = ts.get_single_arg();
        auto result_type = lookup_type_allow_partial_def(info.result_type);

        if (result_type->is_reference()) {
            // Array of pointers - ИСПОЛЬЗУЕМ POINTER_SIZE
            info.stride = TypeConfig::pointer_size;
            info.sign_extend = false;
            info.load_size = TypeConfig::pointer_size;
        }
        else {
            // Array of values
            info.stride = result_type->get_size_in_memory();
            info.sign_extend = result_type->get_load_signed();
            info.reg = result_type->get_preferred_reg_class();
            info.load_size = result_type->get_load_size();
        }
    }
    else if (ts.base_type() == "inline-array") {
        auto result_type = lookup_type_allow_partial_def(ts.get_single_arg());
        auto result_structure = dynamic_cast<StructureType*>(result_type);

        if (!result_structure || result_structure->is_dynamic()) {
            info.can_deref = false;
        }
        else {
            info.can_deref = true;
            info.mem_deref = false; // Don't actually dereference, just add stride*idx
            info.result_type = ts.get_single_arg();
            info.sign_extend = false;

            if (result_type->is_reference()) {
                info.stride = result_type->get_size_in_memory();
            }
        }
    }
    else {
        info.can_deref = false;
    }

    return info;
}

// ============================================================================
// BitField Support
// ============================================================================

bool TypeSystem::is_bitfield_type(const std::string& type_name) const {
    return dynamic_cast<BitFieldType*>(lookup_type(type_name)) != nullptr;
}

BitfieldLookupInfo TypeSystem::lookup_bitfield_info(const std::string& type_name,
    const std::string& field_name) const {
    auto type = get_type_of_type<BitFieldType>(type_name);
    BitField field;

    if (!type->lookup_field(field_name, &field)) {
        throw_typesystem_error("Type {} has no bitfield named {}", type_name, field_name);
    }

    BitfieldLookupInfo info;
    info.result_type = field.type();
    info.offset = field.offset();
    info.size = field.size();
    info.sign_extend = lookup_type(info.result_type)->get_load_signed();

    return info;
}

void TypeSystem::add_field_to_bitfield(BitFieldType* type,
    const std::string& field_name,
    const TypeSpec& field_type,
    int offset,
    int field_size,
    bool skip_in_decomp) {
    // Calculate load size in bits
    auto load_size = lookup_type(field_type)->get_load_size() * 8;

    if (field_size == -1) {
        field_size = load_size;
    }

    if (field_size > load_size) {
        throw_typesystem_error(
            "Type {}'s bitfield {}'s set size is {}, which is larger than the actual type: {}",
            type->get_name(), field_name, field_size, load_size);
    }

    if (field_size + offset > type->get_load_size() * 8) {
        throw_typesystem_error(
            "Type {}'s bitfield {} will run off the end of the type (ends at {} bits, type is {} bits)",
            type->get_name(), field_name, field_size + offset, type->get_load_size() * 8);
    }

    BitField field(field_type, field_name, offset, field_size, skip_in_decomp);
    type->m_fields.push_back(field);
}

// ============================================================================
// Code Generation
// ============================================================================

std::string TypeSystem::generate_deftype_footer(const Type* type) const {
    std::string result;

    // Handle structure-specific flags
    auto as_structure = dynamic_cast<const StructureType*>(type);
    if (as_structure) {
        if (as_structure->is_packed()) {
            result.append("  :pack-me\n");
        }
        if (as_structure->is_allowed_misalign()) {
            result.append("  :allow-misaligned\n");
        }
        if (as_structure->is_always_stack_singleton()) {
            result.append("  :always-stack-singleton\n");
        }
    }

    // Handle heap base
    if (type->heap_base() != 0) {
        result.append(fmt::format("  :heap-base #x{:x}\n", type->heap_base()));
    }

    // Handle inspect generation
    if (!type->gen_inspect()) {
        result.append("  :no-inspect\n");
    }

    // Generate methods section
    std::string methods_string;

    // New method
    auto new_info = type->get_new_method_defined_for_type();
    if (new_info) {
        methods_string.append("    (new (");
        for (size_t i = 0; i < new_info->type.get_args_count() - 1; i++) {
            methods_string.append(new_info->type.get_arg(i).print());
            if (i != new_info->type.get_args_count() - 2) {
                methods_string.push_back(' ');
            }
        }
        methods_string.append(
            fmt::format(") {})", new_info->type.last_arg().print()));

        // Add behavior tag if present
        auto behavior = new_info->type.try_get_tag("behavior");
        if (behavior) {
            methods_string.append(fmt::format(" :behavior {}", *behavior));
        }

        methods_string.append("\n");
    }

    // Other methods
    for (const auto& method : type->get_methods_defined_for_type()) {
        if (method.only_overrides_docstring) {
            continue;
        }

        methods_string.append(fmt::format("    ({} (", method.name));
        for (size_t i = 0; i < method.type.get_args_count() - 1; i++) {
            methods_string.append(method.type.get_arg(i).print());
            if (i != method.type.get_args_count() - 2) {
                methods_string.push_back(' ');
            }
        }
        methods_string.append(fmt::format(") {})", method.type.last_arg().print()));

        // Add method modifiers
        if (method.no_virtual) {
            methods_string.append(" :no-virtual");
        }
        if (method.overrides_parent) {
            methods_string.append(" :replace");
        }

        methods_string.append("\n");
    }

    if (!methods_string.empty()) {
        result.append("  (:methods\n");
        result.append(methods_string);
        result.append("    )\n");
    }

    result.append("  )\n");
    return result;
}

std::string TypeSystem::generate_deftype_for_structure(const StructureType* st) const {
    std::string result;
    result += fmt::format("(deftype {} ({})\n", st->get_name(), st->get_parent());

    // Add docstring if present
    if (st->m_metadata.docstring) {
        result += fmt::format("  \"{}\"\n", st->m_metadata.docstring.value());
    }

    result += "  (";

    // Calculate field formatting
    int longest_field_name = 0;
    int longest_type_name = 0;

    for (size_t i = st->first_unique_field_idx(); i < st->fields().size(); i++) {
        const auto& field = st->fields()[i];
        longest_field_name = std::max(longest_field_name, (int)field.name().size());
        longest_type_name = std::max(longest_type_name, (int)field.type().print().size());
    }

    // Generate fields
    for (size_t i = st->first_unique_field_idx(); i < st->fields().size(); i++) {
        const auto& field = st->fields()[i];
        result += "(";
        result += field.name();
        result.append(2 + (longest_field_name - (int)field.name().size()), ' ');
        result += field.type().print();

        // Add field modifiers
        std::string mods;
        if (field.is_array() && !field.is_dynamic()) {
            mods += " ";
            mods += std::to_string(field.array_size());
        }
        if (field.is_inline()) {
            mods += " :inline";
        }
        if (field.is_dynamic()) {
            mods += " :dynamic";
        }

        if (!mods.empty()) {
            result.append(1 + longest_type_name - (int)field.type().print().size(), ' ');
        }
        result.append(mods);

        // Handle user-placed fields
        if (field.user_placed()) {
            result.append(fmt::format(" :offset {:3d}", field.offset()));
        }

        result.append(")\n   ");
    }

    result.append(")\n");
    result.append(generate_deftype_footer(st));

    return result;
}

std::string TypeSystem::generate_deftype_for_bitfield(const BitFieldType* type) const {
    std::string result;
    result += fmt::format("(deftype {} ({})\n", type->get_name(), type->get_parent());

    if (type->m_metadata.docstring) {
        result += fmt::format("  \"{}\"\n", type->m_metadata.docstring.value());
    }

    result += "  (";

    // Calculate field formatting
    int longest_field_name = 0;
    int longest_type_name = 0;

    for (const auto& field : type->fields()) {
        longest_field_name = std::max(longest_field_name, (int)field.name().size());
        longest_type_name = std::max(longest_type_name, (int)field.type().print().size());
    }

    // Generate bitfield entries
    for (const auto& field : type->fields()) {
        result += "(";
        result += field.name();
        result.append(1 + (longest_field_name - (int)field.name().size()), ' ');
        result += field.type().print();
        result.append(1 + (longest_type_name - (int)field.type().print().size()), ' ');

        result.append(fmt::format(":offset {:3d} :size {:3d}", field.offset(), field.size()));
        result.append(")\n   ");
    }

    result.append(")\n");
    result.append(generate_deftype_footer(type));

    return result;
}

std::string TypeSystem::generate_deftype(const Type* type) const {
    auto st = dynamic_cast<const StructureType*>(type);
    if (st) {
        return generate_deftype_for_structure(st);
    }

    auto bf = dynamic_cast<const BitFieldType*>(type);
    if (bf) {
        return generate_deftype_for_bitfield(bf);
    }

    return fmt::format(
        ";; cannot generate deftype for {}, it is not a structure or bitfield (parent {})\n",
        type->get_name(), type->get_parent());
}

// ============================================================================
// Virtual Method Handling
// ============================================================================

bool TypeSystem::should_use_virtual_methods(const Type* type, int method_id) const {
    auto as_basic = dynamic_cast<const BasicType*>(type);
    if (as_basic && !as_basic->final()) {
        auto method_info = lookup_method(type->get_name(), method_id);
        return !method_info.no_virtual;
    }
    return false;
}

bool TypeSystem::should_use_virtual_methods(const TypeSpec& type, int method_id) const {
    auto it = m_types.find(type.base_type());
    if (it != m_types.end()) {
        return should_use_virtual_methods(it->second.get(), method_id);
    }
    else {
        // For partially defined types, be conservative
        auto fwd_dec_type = lookup_type_allow_partial_def(type);
        if (fwd_dec_type->get_name() == "structure") {
            return false; // Structures don't use virtual methods
        }
        else {
            return should_use_virtual_methods(fwd_dec_type, method_id);
        }
    }
}

// ============================================================================
// Type Search and Queries
// ============================================================================

std::vector<std::string> TypeSystem::get_all_type_names() {
    std::vector<std::string> results;
    for (const auto& [name, type] : m_types) {
        results.push_back(name);
    }
    return results;
}

script::Object TypeSystem::get_all_type_names_as_objects() const {
    script::Object result = script::Object::make_null();
    for (const auto& kv : m_types) {
        result = script::Object::make_pair(script::Object::make_string(kv.first.c_str()), result);
    }
    return result;
}

std::vector<std::string> TypeSystem::search_types_by_parent_type(
    const std::string& parent_type,
    const std::optional<std::vector<std::string>>& existing_matches) {

    std::vector<std::string> results;
    const auto& search_space = existing_matches ? *existing_matches : get_all_type_names();

    for (const auto& type_name : search_space) {
        if (typecheck_base_types(parent_type, type_name, false)) {
            results.push_back(type_name);
        }
    }

    return results;
}

std::vector<std::string> TypeSystem::search_types_by_parent_type_strict(
    const std::string& parent_type) {

    std::vector<std::string> results;
    for (const auto& [type_name, type_info] : m_types) {
        if (type_info->has_parent() && type_info->get_parent() == parent_type) {
            results.push_back(type_name);
        }
    }
    return results;
}

std::vector<std::string> TypeSystem::search_types_by_minimum_method_id(
    const int minimum_method_id,
    const std::optional<std::vector<std::string>>& existing_matches) {

    std::vector<std::string> results;
    const auto& search_space = existing_matches ? *existing_matches : get_all_type_names();

    for (const auto& type_name : search_space) {
        if (get_type_method_count(type_name) >= minimum_method_id) {
            results.push_back(type_name);
        }
    }

    return results;
}

std::vector<std::string> TypeSystem::search_types_by_size(
    const int min_size,
    const std::optional<int> max_size,
    const std::optional<std::vector<std::string>>& existing_matches) {

    std::vector<std::string> results;
    const auto& search_space = existing_matches ? *existing_matches : get_all_type_names();

    for (const auto& type_name : search_space) {
        if (dynamic_cast<NullType*>(m_types.at(type_name).get())) {
            continue; // Skip null types
        }

        const auto size = m_types.at(type_name)->get_size_in_memory();
        if (max_size) {
            if (size >= min_size && size <= *max_size) {
                results.push_back(type_name);
            }
        }
        else {
            if (size == min_size) {
                results.push_back(type_name);
            }
        }
    }

    return results;
}

std::vector<std::string> TypeSystem::search_types_by_fields(
    const std::vector<TypeSearchFieldInput>& search_fields,
    const std::optional<std::vector<std::string>>& existing_matches) {

    std::vector<std::string> results;
    const auto& search_space = existing_matches ? *existing_matches : get_all_type_names();

    for (const auto& type_name : search_space) {
        auto structure = dynamic_cast<StructureType*>(m_types.at(type_name).get());
        if (!structure) {
            continue;
        }

        bool type_valid = true;
        for (const auto& req_field : search_fields) {
            bool field_found = false;
            for (const auto& type_field : structure->fields()) {
                if (type_field.offset() == req_field.field_offset &&
                    type_field.type().base_type() == req_field.field_type_name) {
                    field_found = true;
                    break;
                }
            }

            if (!field_found) {
                type_valid = false;
                break;
            }
        }

        if (type_valid) {
            results.push_back(type_name);
        }
    }

    return results;
}

// ============================================================================
// Enum Support
// ============================================================================

EnumType* TypeSystem::try_enum_lookup(const std::string& type_name) const {
    auto it = m_types.find(type_name);
    if (it != m_types.end()) {
        return dynamic_cast<EnumType*>(it->second.get());
    }
    return nullptr;
}

EnumType* TypeSystem::try_enum_lookup(const TypeSpec& type) const {
    return try_enum_lookup(type.base_type());
}

// ============================================================================
// Forward Declaration Support
// ============================================================================

void TypeSystem::forward_declare_type_method_count(const std::string& name, int num_methods) {
    auto existing_fwd = m_forward_declared_method_counts.find(name);
    if (existing_fwd != m_forward_declared_method_counts.end() &&
        existing_fwd->second != num_methods) {
        throw_typesystem_error(
            "Type {} was originally forward declared with {} methods and is now being "
            "forward declared with {} methods", name, existing_fwd->second, num_methods);
    }

    auto existing_type = m_types.find(name);
    if (existing_type != m_types.end()) {
        int existing_count = get_next_method_id(existing_type->second.get());
        if (existing_count != num_methods) {
            throw_typesystem_error(
                "Type {} was defined with {} methods and is now being forward declared with {} methods",
                name, existing_count, num_methods);
        }
    }

    m_forward_declared_method_counts[name] = num_methods;
}

int TypeSystem::get_type_method_count(const std::string& name) const {
    auto result = try_get_type_method_count(name);
    if (result) {
        return *result;
    }
    throw_typesystem_error("Tried to find the number of methods on type `{}`, but it is not defined.", name);
    return -1;
}

std::optional<int> TypeSystem::try_get_type_method_count(const std::string& name) const {
    auto type_it = m_types.find(name);
    if (type_it != m_types.end()) {
        return get_next_method_id(type_it->second.get());
    }

    auto fwd_it = m_forward_declared_method_counts.find(name);
    if (fwd_it != m_forward_declared_method_counts.end()) {
        return fwd_it->second;
    }

    return std::nullopt;
}

// ============================================================================
// Method declaration variants
// ============================================================================

MethodInfo TypeSystem::declare_method(const std::string& type_name,
    const std::string& method_name,
    const std::optional<std::string>& docstring,
    bool no_virtual,
    const TypeSpec& ts,
    bool override_type) {
    return declare_method(lookup_type(make_typespec(type_name)), method_name,
        docstring, no_virtual, ts, override_type);
}

MethodInfo TypeSystem::define_method(const std::string& type_name,
    const std::string& method_name,
    const TypeSpec& ts,
    const std::optional<std::string>& docstring) {
    return define_method(lookup_type(make_typespec(type_name)), method_name, ts, docstring);
}

MethodInfo TypeSystem::define_method(Type* type,
    const std::string& method_name,
    const TypeSpec& ts,
    const std::optional<std::string>& docstring) {
    if (method_name == "new") {
        return add_new_method(type, ts, docstring);
    }

    MethodInfo existing_info;
    bool got_existing = try_lookup_method(type, method_name, &existing_info);

    if (got_existing) {
        // Update docstring and verify compatibility
        existing_info.docstring = *docstring;

        int bad_arg_idx = -1;
        if (!existing_info.type.is_compatible_child_method(ts, type->get_name(), &bad_arg_idx)) {
            throw_typesystem_error(
                "The method {} of type {} was originally defined as {}, but has been "
                "redefined as {} (see argument index {})",
                method_name, type->get_name(), existing_info.type.print(), ts.print(), bad_arg_idx);
        }

        return existing_info;
    }
    else {
        throw_typesystem_error("Cannot add method {} to type {} because it was not declared",
            method_name, type->get_name());
    }
}

MethodInfo TypeSystem::overlay_method(Type* type,
    const std::string& method_name,
    const std::string& method_overlay_name,
    const std::optional<std::string>& docstring,
    const TypeSpec& ts) {
    MethodInfo existing_info;
    bool got_existing = try_lookup_method(type, method_overlay_name, &existing_info);

    if (!got_existing) {
        throw_typesystem_error(
            "Cannot use :overlay-at on method {} of {} because this method was not previously "
            "declared in a parent", method_overlay_name, type->get_name());
    }

    // CORRECTED: Proper construction
    return type->add_method({ existing_info.id,
                           method_name,
                           ts,
                           type->get_name(),
                           type->get_name(),
                           false,
                           true,
                           false,
                           docstring,
                           std::make_optional(method_overlay_name) });
}

MethodInfo TypeSystem::override_method(Type* type,
    const std::string& method_name,
    const std::optional<std::string>& docstring) {
    // Lookup the method from the parent type
    MethodInfo existing_info;
    bool exists = try_lookup_method(type->get_parent(), method_name, &existing_info);
    if (!exists) {
        throw_typesystem_error("Trying to override a method that has no parent declaration");
    }

    // CORRECTED: Use proper MethodInfo construction
    return type->add_method({ existing_info.id,
                           method_name,
                           existing_info.type,
                           type->get_name(),
                           type->get_name(),
                           existing_info.no_virtual,
                           false,  // overrides_parent
                           true,   // only_overrides_docstring  
                           docstring,
                           std::nullopt });
}

// ============================================================================
// Field offset assertion
// ============================================================================

void TypeSystem::assert_field_offset(const std::string& type_name,
    const std::string& field_name,
    int offset) {
    Field field = lookup_field(type_name, field_name);
    if (field.offset() != offset) {
        throw_typesystem_error("assert_field_offset({}, {}, {}) failed - got {}",
            type_name, field_name, offset, field.offset());
    }
}

// ============================================================================
// Load size with partial definition support
// ============================================================================

int TypeSystem::get_load_size_allow_partial_def(const TypeSpec& ts) const {
    auto fully_defined_it = m_types.find(ts.base_type());
    if (fully_defined_it != m_types.end()) {
        return fully_defined_it->second->get_load_size();
    }

    auto partial_def = lookup_type_allow_partial_def(ts);
    if (!tc(TypeSpec("structure"), ts)) {
        throw_typesystem_error("Cannot perform a load or store from partially defined type {}",
            ts.print());
    }

    return partial_def->get_load_size(); // Should be 4 for structures
}

// ============================================================================
// Method lookup by ID
// ============================================================================

bool TypeSystem::try_lookup_method(const std::string& type_name,
    int method_id,
    MethodInfo* info) const {
    auto kv = m_types.find(type_name);
    if (kv == m_types.end()) {
        return false;
    }

    auto* iter_type = kv->second.get();
    while (true) {
        // РЕАЛЬНОЕ ИСПОЛЬЗОВАНИЕ КОНСТАНТ МЕТОДОВ
        if (method_id == SOOT_NEW_METHOD) {
            if (iter_type->get_my_new_method(info)) {
                return true;
            }
        }
        else {
            if (iter_type->get_my_method(method_id, info)) {
                return true;
            }
        }

        if (iter_type->has_parent()) {
            iter_type = lookup_type(iter_type->get_parent());
        }
        else {
            break;
        }
    }
    return false;
}

MethodInfo TypeSystem::lookup_method(const std::string& type_name, int method_id) const {
    // РЕАЛЬНОЕ ИСПОЛЬЗОВАНИЕ КОНСТАНТ МЕТОДОВ
    if (method_id == SOOT_NEW_METHOD) {
        return lookup_new_method(type_name);
    }

    MethodInfo info;
    auto* type = lookup_type(type_name);
    auto* iter_type = type;

    while (true) {
        if (iter_type->get_my_method(method_id, &info)) {
            return info;
        }

        if (iter_type->has_parent()) {
            iter_type = lookup_type(iter_type->get_parent());
        }
        else {
            break;
        }
    }

    throw_typesystem_error("The method with id {} of type {} could not be found",
        method_id, type_name);
}
// ============================================================================
// Method ID assertion
// ============================================================================

void TypeSystem::assert_method_id(const std::string& type_name,
    const std::string& method_name,
    int id) {
    auto info = lookup_method(type_name, method_name);
    if (info.id != id) {
        throw_typesystem_error("Method ID assertion failed: type {}, method {} id was {}, expected {}",
            type_name, method_name, info.id, id);
    }
}

// ============================================================================
// Forward declaration with multiple of 4
// ============================================================================

void TypeSystem::forward_declare_type_method_count_multiple_of_4(const std::string& name, int num_methods) {
    auto existing_fwd = m_forward_declared_method_counts.find(name);
    if (existing_fwd != m_forward_declared_method_counts.end() &&
        existing_fwd->second + 3 < num_methods) {
        throw_typesystem_error(
            "Type {} was originally forward declared with {} methods and is now being "
            "forward declared with {} methods", name, existing_fwd->second, num_methods);
    }

    auto existing_type = m_types.find(name);
    if (existing_type != m_types.end()) {
        int existing_count = get_next_method_id(existing_type->second.get());
        if (existing_count + 3 < num_methods) {
            throw_typesystem_error(
                "Type {} was defined with {} methods and is now being forward declared with {} methods",
                name, existing_count, num_methods);
        }
    }

    m_forward_declared_method_counts[name] = num_methods;
}

// ============================================================================
// Method lookup
// ============================================================================

bool TypeSystem::try_lookup_method(const std::string& type_name,
    const std::string& method_name,
    MethodInfo* info) const {
    auto kv = m_types.find(type_name);
    if (kv == m_types.end()) {
        // Try to look up a forward declared type
        auto fwd_dec_type = lookup_type_allow_partial_def(type_name);
        if (tc(TypeSpec("basic"), TypeSpec(fwd_dec_type->get_name()))) {
            return try_lookup_method(fwd_dec_type, method_name, info);
        }
        return false;
    }

    return try_lookup_method(kv->second.get(), method_name, info);
}

// ============================================================================
// Field alignment and size calculations (упрощенные версии)
// ============================================================================

int align(int value, int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

int TypeSystem::get_alignment_in_type(const Field& field) {
    auto field_type = lookup_type_allow_partial_def(field.type());

    int alignment = TypeConfig::pointer_size;

    if (field.is_inline()) {
        if (field.is_array()) {
            alignment = field_type->get_inline_array_start_alignment();
        }
        else {
            alignment = field_type->get_in_memory_alignment();
        }
    }

    if (!field_type->is_reference()) {
        alignment = field_type->get_in_memory_alignment();
    }
    if (Type::verbose)
        fmt::print("DEBUG: Field {} type {} alignment: {}\n",
            field.name(), field_type->get_name(), alignment);

    // Reference type - ИСПОЛЬЗУЕМ POINTER_SIZE
    return alignment;
}

int TypeSystem::get_size_in_type(const Field& field) const {
    if (field.is_dynamic()) {
        return 0;
    }

    auto field_type = lookup_type_allow_partial_def(field.type());

    if (field.is_array()) {
        if (field.is_inline()) {
            // INLINE ARRAY: элементы хранятся непосредственно в структуре
            int element_size = field_type->get_size_in_memory();
            return field.array_size() * element_size;
        }
        else {
            // Обычные массивы (указатели)
            if (field_type->is_reference()) {
                return field.array_size() * TypeConfig::pointer_size;
            }
            else {
                // ВАЖНО: выравниваем каждый элемент!
                int aligned_element_size = align(field_type->get_size_in_memory(), field_type->get_in_memory_alignment());
                return field.array_size() * aligned_element_size;
            }
        }
    }
    else {
        // Не массив
        if (field.is_inline()) {
            // INLINE OBJECT: объект хранится непосредственно
            return field_type->get_size_in_memory();
        }
        else {
            // Обычное поле
            if (field_type->is_reference()) {
                return TypeConfig::pointer_size;
            }
            else {
                // ВАЖНО: выравниваем размер поля!
                return align(field_type->get_size_in_memory(), field_type->get_in_memory_alignment());
            }
        }
    }
}

// ============================================================================
// Structure inheritance helper
// ============================================================================

void TypeSystem::builtin_structure_inherit(StructureType* st) {
    st->inherit(get_type_of_type<StructureType>(st->get_parent()));
}

// ============================================================================
// Aliases
// ============================================================================

Object TypeSystem::make_step_accessor(const Object& key) {
    // 1. Сначала свойства (мета-данные системы типов)
    Object base_attempt = HeapObject::make_step_accessor(key);
    if (!base_attempt.is_undefined()) return base_attempt;

    // 2. Трактуем ключ как имя типа
    std::string name;
    if (key.is_symbol()) {
        name = key.to_std_string();
    } else if (key.is_string()) {
        name = key.to_std_string();
    } else {
        return Object::make_undefined(); // Или бросай ошибку, если хочешь строгости
    }
    if (name == "types-count") {
        return Object::make_integer(get_types_count());
    }

    if (name == "pointer-size") {
        return Object::make_integer(get_pointer_size());
    }
    // 3. Ищем тип
    // Предполагаем, что lookup_type возвращает какой-то указатель или shared_ptr
    auto type_ptr = lookup_type_no_throw(name);
    
    if (type_ptr) {
        // Если твои типы хранятся как shared_ptr в TypeSystem, просто отдавай его.
        // Если как unique_ptr, то возвращай NativeRef с пустым делетером (но помни о рисках!)
        return Object::make_native_ref(std::shared_ptr<Type>(type_ptr, [](Type*){}));
    }

    return Object::make_undefined();
}

// ============================================================================
// Reverse field lookup (упрощенные заглушки)
// ============================================================================

// Вспомогательная функция для рекурсивного поиска
namespace {

    void find_field_access_paths(const TypeSystem* ts,
        const StructureType* type,
        int target_offset,
        const ReverseLookupNode* current_path,
        std::vector<FieldReverseLookupOutput>& results,
        int depth = 0) {

        if (depth > 10) return;

        // Ищем поля в текущем типе
        for (const auto& field : type->fields()) {
            int field_offset = field.offset();
            int field_size = ts->get_size_in_type(field);
            if (Type::verbose)
                fmt::print("DEBUG: Checking field '{}' at offset {}, size: {}, target: {}\n",
                    field.name(), field_offset, field_size, target_offset);

            // Точное совпадение с полем
            if (field_offset == target_offset && !field.is_array()) {
                ReverseLookupNode new_node{ current_path,
                    {FieldReverseLookupOutput::Token::Kind::FIELD,
                     field.name(), -1, field.field_score()} };

                results.emplace_back(false, field.type(), new_node.to_vector());
                continue;
            }

            // Поле является массивом
            if (field.is_array() && !field.is_dynamic()) {
                if (target_offset >= field_offset && target_offset < field_offset + field_size) {
                    if (Type::verbose)                    
                        fmt::print("DEBUG: Inside array '{}', calculating index...\n", field.name());

                    // ВЫЧИСЛЯЕМ РАЗМЕР ЭЛЕМЕНТА ПРАВИЛЬНО
                    int element_size;
                    if (field.is_inline()) {
                        // Для inline arrays - реальный размер элемента
                        auto element_type = ts->lookup_type_allow_partial_def(field.type());
                        element_size = element_type->get_size_in_memory();
                    }
                    else {
                        // Для обычных массивов - размер указателя или значения
                        element_size = field_size / field.array_size();
                    }
                    if (Type::verbose)
                        fmt::print("DEBUG: Element size: {}, array_size: {}\n", element_size, field.array_size());

                    int index = (target_offset - field_offset) / element_size;
                    int remainder = (target_offset - field_offset) % element_size;
                    if (Type::verbose)
                        fmt::print("DEBUG: Index: {}, remainder: {}\n", index, remainder);

                    ReverseLookupNode array_node{ current_path,
                        {FieldReverseLookupOutput::Token::Kind::FIELD,
                         field.name(), -1, field.field_score()} };

                    ReverseLookupNode index_node{ &array_node,
                        {FieldReverseLookupOutput::Token::Kind::CONSTANT_IDX,
                         "", index, 0.0} };

                    // Если это inline массив И есть remainder - ищем внутри элемента
                    if (field.is_inline() && remainder > 0) {
                        auto element_type = ts->lookup_type_allow_partial_def(field.type());
                        if (auto element_struct = dynamic_cast<const StructureType*>(element_type)) {
                            if (Type::verbose)                            
                                fmt::print("DEBUG: Recursing into inline array element at offset {}\n", remainder);
                            find_field_access_paths(ts, element_struct, remainder, &index_node, results, depth + 1);
                        }
                        else {
                            // Простой тип, но есть remainder - это ошибка?
                            if (Type::verbose)                            
                                fmt::print("DEBUG: Simple type with remainder - skipping\n");
                        }
                    }
                    else {
                        // Нет remainder или не inline - просто доступ к элементу
                        results.emplace_back(false, field.type(), index_node.to_vector());
                    }
                }
            }

            // Inline структура (не массив)
            if (field.is_inline() && !field.is_array()) {
                auto field_type = ts->lookup_type_allow_partial_def(field.type());
                if (auto field_struct = dynamic_cast<const StructureType*>(field_type)) {
                    if (target_offset >= field_offset && target_offset < field_offset + field_size) {
                        ReverseLookupNode inline_node{ current_path,
                            {FieldReverseLookupOutput::Token::Kind::FIELD,
                             field.name(), -1, field.field_score()} };

                        // Рекурсивно ищем во inline структуре
                        int inner_offset = target_offset - field_offset;
                        find_field_access_paths(ts, field_struct, inner_offset,
                            &inline_node, results, depth + 1);
                    }
                }
            }
        }

        // Ищем в родительских типах
        if (type->has_parent()) {
            auto parent_type = ts->lookup_type_allow_partial_def(type->get_parent());
            if (auto parent_struct = dynamic_cast<const StructureType*>(parent_type)) {
                find_field_access_paths(ts, parent_struct, target_offset, current_path, results, depth + 1);
            }
        }
    }

} // namespace

FieldReverseLookupOutput TypeSystem::reverse_field_lookup(const FieldReverseLookupInput& input) const {
    FieldReverseLookupOutput result;
    if (Type::verbose) {
        fmt::print("=== REVERSE LOOKUP START ===\n");
        fmt::print("DEBUG: Reverse lookup for type {} at offset {}, stride: {}, pointer_size: {}\n",
            input.base_type.print(), input.offset, input.stride, TypeConfig::pointer_size);
    }
    Type* base_type = lookup_type_allow_partial_def(input.base_type);
    if (!base_type) {
        if (Type::verbose)
            fmt::print("DEBUG: Base type not found\n");
        result.success = false;
        return result;
    }

    // Применяем stride если есть
    int effective_offset = input.offset;
    if (input.stride != 0) {
        // Для простоты считаем что stride уже применен
    }
    if (Type::verbose)
        fmt::print("DEBUG: Base type: {}, kind: {}\n",
            base_type->get_name(), typeid(*base_type).name());

    // Обрабатываем StructureType
    if (auto structure = dynamic_cast<StructureType*>(base_type)) {
        if (Type::verbose) {
            fmt::print("DEBUG: Processing structure '{}' with {} fields, total size: {}\n",
                structure->get_name(), structure->fields().size(),
                structure->get_size_in_memory());

            // Выводим информацию о всех полях для отладки
            for (const auto& field : structure->fields()) {
                int field_size = get_size_in_type(field);
                    fmt::print("DEBUG:   Field '{}' at offset {}, size: {}, inline: {}, array: {}, array_size: {}\n",
                        field.name(), field.offset(), field_size,
                        field.is_inline(), field.is_array(), field.array_size());
            }
        }
        std::vector<FieldReverseLookupOutput> all_results;

        // Находим все возможные пути доступа
        find_field_access_paths(this, structure, effective_offset, nullptr, all_results);
        if (Type::verbose)
            fmt::print("DEBUG: Found {} possible access paths\n", all_results.size());

        if (!all_results.empty()) {
            // Выбираем результат с наивысшим score
            auto best_result = std::max_element(all_results.begin(), all_results.end(),
                [](const auto& a, const auto& b) {
                    return a.total_score < b.total_score;
                });

            // Вычисляем общий score
            best_result->total_score = 0.0;
            for (const auto& token : best_result->tokens) {
                best_result->total_score += token.score();
            }
            if (Type::verbose) {
                fmt::print("DEBUG: Best result: {} tokens, score: {}\n",
                    best_result->tokens.size(), best_result->total_score);
                for (const auto& token : best_result->tokens) {
                    fmt::print("DEBUG:   Token: {} (kind: {})\n",
                        token.print(), static_cast<int>(token.kind));
                }
            }

            return *best_result;
        }
        else {
            if (Type::verbose)            
                fmt::print("DEBUG: No access paths found for offset {}\n", effective_offset);
        }
    }
    else if (auto bitfield = dynamic_cast<BitFieldType*>(base_type)) {
        if (Type::verbose)        
            fmt::print("DEBUG: Processing bitfield '{}'\n", bitfield->get_name());

        for (const auto& field : bitfield->fields()) {
            if (Type::verbose)
                fmt::print("DEBUG:   BitField '{}' at bit offset {}, size: {} bits\n",
                    field.name(), field.offset(), field.size());

            if (effective_offset >= field.offset() &&
                effective_offset < field.offset() + field.size()) {
                // Для битовых полей offset должен точно совпадать
                if (effective_offset == field.offset()) {
                    result.success = true;
                    result.result_type = field.type();
                    result.tokens.push_back({
                        FieldReverseLookupOutput::Token::Kind::FIELD,
                        field.name(),
                        -1,
                        1.0  // Высокий score для точного совпадения
                        });

                    fmt::print("DEBUG: Found bitfield match: {}\n", field.name());
                    return result;
                }
            }
        }
    }
    else {
    if (Type::verbose)        
        fmt::print("DEBUG: Unsupported type kind for reverse lookup: {}\n",
            typeid(*base_type).name());
    }
    if (Type::verbose)
        fmt::print("DEBUG: Reverse lookup FAILED\n");
    result.success = false;
    return result;
}

FieldReverseMultiLookupOutput TypeSystem::reverse_field_multi_lookup(const FieldReverseLookupInput& input,
    int max_count) const {
    FieldReverseMultiLookupOutput result;

    Type* base_type = lookup_type_allow_partial_def(input.base_type);
    if (!base_type) {
        result.success = false;
        return result;
    }

    if (auto structure = dynamic_cast<StructureType*>(base_type)) {
        find_field_access_paths(this, structure, input.offset, nullptr, result.results);

        // Сортируем по score и ограничиваем количество
        std::sort(result.results.begin(), result.results.end(),
            [](const auto& a, const auto& b) {
                return a.total_score > b.total_score;
            });

        if (result.results.size() > max_count) {
            result.results.resize(max_count);
        }

        result.success = !result.results.empty();
    }
    else {
        result.success = false;
    }

    return result;
}

// Реализация Token::print()
std::string FieldReverseLookupOutput::Token::print() const {
    switch (kind) {
    case Kind::FIELD:
        return name;
    case Kind::CONSTANT_IDX:
        return std::to_string(idx);  
    case Kind::VAR_IDX:
        return "__VAR__"; 
    default:
        return "?";
    }
}

script::Object TypeSystem::inspect() const {
    return pretty_print::build_list(
        Object::make_symbol("type-system"), 
        Object::make_symbol(":size"), 
        Object::make_integer(m_types.size()));
}
