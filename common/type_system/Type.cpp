#include "common/type_system/Type.hpp"

#include "common/type_system/TypeSystem.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Crc32.hpp"
#include "fmt/format.h"

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

// ============================================================================
// RegClass to string
// ============================================================================

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
// MethodInfo
// ============================================================================

bool MethodInfo::operator==(const MethodInfo &other) const {
    return id == other.id && name == other.name && type == other.type &&
           defined_in_type == other.defined_in_type && no_virtual == other.no_virtual &&
           overrides_parent == other.overrides_parent &&
           only_overrides_docstring == other.only_overrides_docstring;
}

bool MethodInfo::operator!=(const MethodInfo &other) const {
    return !(*this == other);
}

std::string MethodInfo::print() const {
    return fmt::format("#<method-info {} id:{}>", name, id);
}

std::string MethodInfo::print_one_line() const {
    return fmt::format("Method {:3d}: {:20} {}", id, name, type.print());
}

std::string MethodInfo::inspect() const {
    return fmt::format(
        "method-info:\n"
        "  id:                  {}\n"
        "  name:                {}\n"
        "  type:                {}\n"
        "  defined-in-type:     {}\n"
        "  type-name:           {}\n"
        "  no-virtual:          {}\n"
        "  overrides-parent:    {}\n"
        "  only-overrides-doc:  {}\n"
        "  docstring:           {}\n"
        "  overlay-name:        {}",
        id, name, type.print(), defined_in_type, type_name, no_virtual, overrides_parent,
        only_overrides_docstring, docstring.value_or(""), overlay_name.value_or(""));
}

std::string MethodInfo::diff(const MethodInfo &other) const {
    std::string result;
    if (id != other.id)
        result += fmt::format("id: {} vs. {}\n", id, other.id);
    if (name != other.name)
        result += fmt::format("name: {} vs. {}\n", name, other.name);
    if (type != other.type)
        result += fmt::format("type: {} vs. {}\n", type.print(), other.type.print());
    if (defined_in_type != other.defined_in_type)
        result +=
            fmt::format("defined_in_type: {} vs. {}\n", defined_in_type, other.defined_in_type);
    if (no_virtual != other.no_virtual)
        result += fmt::format("no_virtual: {} vs. {}\n", no_virtual, other.no_virtual);
    if (overrides_parent != other.overrides_parent)
        result +=
            fmt::format("overrides_parent: {} vs. {}\n", overrides_parent, other.overrides_parent);
    if (only_overrides_docstring != other.only_overrides_docstring)
        result += fmt::format("only_overrides_docstring: {} vs. {}\n", only_overrides_docstring,
                              other.only_overrides_docstring);
    return result;
}

// ============================================================================
// Field
// ============================================================================

Field::Field(std::string name, TypeSpec ts) : m_name(std::move(name)), m_type(std::move(ts)) {}

Field::Field(std::string name, TypeSpec ts, int offset)
    : m_name(std::move(name)), m_type(std::move(ts)), m_offset(offset) {}

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

void Field::set_override_type(const TypeSpec &new_type) {
    m_type = new_type;
    m_override_type = true;
}

void Field::mark_as_user_placed() {
    m_placed_by_user = true;
}

bool Field::operator==(const Field &other) const {
    return m_name == other.m_name && m_type == other.m_type && m_offset == other.m_offset &&
           m_inline == other.m_inline && m_dynamic == other.m_dynamic && m_array == other.m_array &&
           m_array_size == other.m_array_size && m_alignment == other.m_alignment;
}

bool Field::operator!=(const Field &other) const {
    return !(*this == other);
}

std::string Field::print() const {
    return fmt::format("#<field {} @{} {}>", m_name, m_offset, m_type.print());
}

std::string Field::inspect() const {
    return fmt::format(
        "field:\n"
        "  name:         {}\n"
        "  type:         {}\n"
        "  offset:       {}\n"
        "  inline:       {}\n"
        "  dynamic:      {}\n"
        "  array:        {}\n"
        "  array-size:   {}\n"
        "  alignment:    {}\n"
        "  skip-decomp:  {}",
        m_name, m_type.print(), m_offset, m_inline, m_dynamic, m_array, m_array_size, m_alignment,
        m_skip_in_static_decomp);
}

std::string Field::diff(const Field &other) const {
    std::string result;
    if (m_name != other.m_name)
        result += fmt::format("name: {} vs. {}\n", m_name, other.m_name);
    if (m_type != other.m_type)
        result += fmt::format("type: {} vs. {}\n", m_type.print(), other.m_type.print());
    if (m_offset != other.m_offset)
        result += fmt::format("offset: {} vs. {}\n", m_offset, other.m_offset);
    if (m_inline != other.m_inline)
        result += fmt::format("inline: {} vs. {}\n", m_inline, other.m_inline);
    if (m_dynamic != other.m_dynamic)
        result += fmt::format("dynamic: {} vs. {}\n", m_dynamic, other.m_dynamic);
    if (m_array != other.m_array)
        result += fmt::format("array: {} vs. {}\n", m_array, other.m_array);
    if (m_array_size != other.m_array_size)
        result += fmt::format("array_size: {} vs. {}\n", m_array_size, other.m_array_size);
    if (m_alignment != other.m_alignment)
        result += fmt::format("alignment: {} vs. {}\n", m_alignment, other.m_alignment);
    if (m_skip_in_static_decomp != other.m_skip_in_static_decomp)
        result += fmt::format("skip_in_static_decomp: {} vs. {}\n", m_skip_in_static_decomp,
                              other.m_skip_in_static_decomp);
    return result;
}

// ============================================================================
// BitField
// ============================================================================

BitField::BitField(TypeSpec type, std::string name, int offset, int size, bool skip_in_decomp)
    : m_type(std::move(type)), m_name(std::move(name)), m_offset(offset), m_size(size),
      m_skip_in_static_decomp(skip_in_decomp) {}

bool BitField::operator==(const BitField &other) const {
    return m_type == other.m_type && m_name == other.m_name && m_offset == other.m_offset &&
           m_size == other.m_size;
}

bool BitField::operator!=(const BitField &other) const {
    return !(*this == other);
}

std::string BitField::print() const {
    return fmt::format("#<bitfield {} @{}:{} {}>", m_name, m_offset, m_size, m_type.print());
}

std::string BitField::inspect() const {
    return fmt::format(
        "bit-field:\n"
        "  name:            {}\n"
        "  type:            {}\n"
        "  offset (bits):   {}\n"
        "  size (bits):     {}\n"
        "  skip-in-decomp:  {}",
        m_name, m_type.print(), m_offset, m_size, m_skip_in_static_decomp);
}

std::string BitField::diff(const BitField &other) const {
    std::string result;
    if (m_name != other.m_name)
        result += fmt::format("name: {} vs. {}\n", m_name, other.m_name);
    if (m_type != other.m_type)
        result += fmt::format("type: {} vs. {}\n", m_type.print(), other.m_type.print());
    if (m_offset != other.m_offset)
        result += fmt::format("offset: {} vs. {}\n", m_offset, other.m_offset);
    if (m_size != other.m_size)
        result += fmt::format("size: {} vs. {}\n", m_size, other.m_size);
    return result;
}

// ============================================================================
// Type (base)
// ============================================================================

Type::Type(std::string parent, std::string name, bool is_boxed, int heap_base)
    : m_parent(std::move(parent)), m_name(std::move(name)), m_is_boxed(is_boxed),
      m_heap_base(heap_base) {
    m_runtime_name = m_name;
}

std::string Type::print() const {
    return fmt::format("#<{} {}>", class_name(), get_name());
}

std::string Type::inspect() const {
    return fmt::format(
        "type:\n"
        "  class:   {}\n"
        "  name:    {}\n"
        "  parent:  {}\n"
        "  boxed:   {}",
        class_name(), get_name(), get_parent(), is_boxed());
}

uint32_t Type::get_type_tag() const {
    return util::compute_crc32(get_name());
}

std::string Type::get_runtime_name() const {
    if (!m_allow_in_runtime) {
        throw std::runtime_error(fmt::format("Type {} is not allowed in runtime", get_name()));
    }
    return m_runtime_name;
}

std::string Type::diff(const Type &other) const {
    return common_type_info_diff(other) + diff_impl(other);
}

bool Type::common_type_info_equal(const Type &other) const {
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

    if (m_methods.size() != other.m_methods.size()) {
        result += fmt::format("Method count: {} vs {}\n", m_methods.size(), other.m_methods.size());
    }

    for (size_t i = 0; i < std::min(m_methods.size(), other.m_methods.size()); ++i) {
        if (m_methods[i] != other.m_methods[i]) {
            result += fmt::format("Method {} differs\n", i);
        }
    }

    if (m_parent != other.m_parent)
        result += fmt::format("Parent: {} vs {}\n", m_parent, other.m_parent);
    if (m_name != other.m_name)
        result += fmt::format("Name: {} vs {}\n", m_name, other.m_name);
    if (m_is_boxed != other.m_is_boxed)
        result += fmt::format("Is boxed: {} vs {}\n", m_is_boxed, other.m_is_boxed);

    return result;
}

std::string Type::incompatible_diff(const Type &other) const {
    return fmt::format("Cannot compare {} with {}\n", typeid(*this).name(), typeid(other).name());
}

// ---- Method system ----

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

size_t Type::methods_max_id() const {
    size_t id = static_cast<size_t>(-1);
    if (has_new_method())
        id = 0;
    for (auto it = m_methods.rbegin(); it != m_methods.rend(); ++it) {
        if (it->id > (int)id)
            id = it->id;
    }
    return id;
}

// ---- State system ----

void Type::add_state(const std::string &name, const TypeSpec &type) {
    auto result = m_states.insert({name, type});
    if (!result.second) {
        throw std::runtime_error(fmt::format("State {} is already defined in type", name));
    }
}

// ============================================================================
// NullType
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
    return fmt::format("#<null-type {}>", m_name);
}

std::string NullType::inspect() const {
    return fmt::format("null-type: {}", m_name);
}

bool NullType::operator==(const Type &other) const {
    return this == &other;
}

std::string NullType::diff_impl(const Type &other) const {
    return (*this == other) ? "" : "NullType comparison failed";
}

// ============================================================================
// ValueType
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
    if (m_size != other_value->m_size)
        result += fmt::format("Size: {} vs {}\n", m_size, other_value->m_size);
    if (m_sign_extend != other_value->m_sign_extend)
        result += fmt::format("Sign extend: {} vs {}\n", m_sign_extend, other_value->m_sign_extend);
    if (m_reg_kind != other_value->m_reg_kind)
        result += fmt::format("Register kind: {} vs {}\n", static_cast<int>(m_reg_kind),
                              static_cast<int>(other_value->m_reg_kind));
    if (m_offset != other_value->m_offset)
        result += fmt::format("Offset: {} vs {}\n", m_offset, other_value->m_offset);
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

std::string ValueType::print() const {
    return fmt::format("#<value-type {} size:{}>", m_name, m_size);
}

std::string ValueType::inspect() const {
    return fmt::format(
        "value-type:\n"
        "  name:         {}\n"
        "  parent:       {}\n"
        "  boxed:        {}\n"
        "  size:         {}\n"
        "  sign-extend:  {}\n"
        "  reg-class:    {}",
        m_name, m_parent, m_is_boxed, m_size, m_sign_extend, reg_kind_to_string(m_reg_kind));
}

// ============================================================================
// ReferenceType
// ============================================================================

ReferenceType::ReferenceType(std::string parent, std::string name, bool is_boxed, int heap_base)
    : Type(std::move(parent), std::move(name), is_boxed, heap_base) {}

std::string ReferenceType::print() const {
    return fmt::format("#<reference-type {}>", m_name);
}

std::string ReferenceType::inspect() const {
    return fmt::format(
        "reference-type:\n"
        "  name:      {}\n"
        "  parent:    {}\n"
        "  boxed:     {}\n"
        "  heap-base: {}",
        m_name, m_parent, m_is_boxed, m_heap_base);
}

// ============================================================================
// StructureType
// ============================================================================

StructureType::StructureType(std::string parent, std::string name, bool boxed, bool dynamic,
                             bool pack, int heap_base)
    : ReferenceType(std::move(parent), std::move(name), boxed, heap_base), m_dynamic(dynamic),
      m_pack(pack) {}

void StructureType::inherit(StructureType *parent) {
    if (!parent)
        return;
    if (Type::verbose) {
        fmt::print("DEBUG: Inheriting from {} to {}\n", parent->get_name(), get_name());
    }
    m_fields = parent->fields();
    m_size_in_mem = parent->get_size_in_memory();
    m_dynamic = parent->is_dynamic();
    m_idx_of_first_unique_field = m_fields.size();
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
    if (m_fields != other.m_fields)
        result += "Fields differ\n";
    if (m_dynamic != other.m_dynamic)
        result += fmt::format("Dynamic: {} vs {}\n", m_dynamic, other.m_dynamic);
    if (m_size_in_mem != other.m_size_in_mem)
        result += fmt::format("Size in memory: {} vs {}\n", m_size_in_mem, other.m_size_in_mem);
    if (m_pack != other.m_pack)
        result += fmt::format("Pack: {} vs {}\n", m_pack, other.m_pack);
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

std::string StructureType::print() const {
    return fmt::format("#<structure-type {} size:{}>", m_name, m_size_in_mem);
}

std::string StructureType::inspect() const {
    std::string result =
        fmt::format("structure-type:\n"
                    "  name:     {}\n"
                    "  parent:   {}\n"
                    "  boxed:    {}\n"
                    "  dynamic:  {}\n"
                    "  size:     {}\n"
                    "  packed:   {}\n"
                    "  fields:",
                    m_name, m_parent, m_is_boxed, m_dynamic, m_size_in_mem, m_pack);
    for (const auto &field : m_fields) {
        result += "\n    " + field.print();
    }
    result += "\n  methods:";
    if (m_new_method_info_defined)
        result += "\n    " + m_new_method_info.print_one_line();
    for (const auto &m : m_methods) {
        result += "\n    " + m.print_one_line();
    }
    return result;
}

// ============================================================================
// BasicType
// ============================================================================

BasicType::BasicType(std::string parent, std::string name, bool dynamic, int heap_base)
    : StructureType(std::move(parent), std::move(name), true, dynamic, false, heap_base) {}

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
    if (m_final != other_basic->m_final)
        result += fmt::format("Final: {} vs {}\n", m_final, other_basic->m_final);
    return result;
}

std::string BasicType::print() const {
    return fmt::format("#<basic-type {} size:{}{}>", m_name, m_size_in_mem,
                       m_final ? " final" : "");
}

std::string BasicType::inspect() const {
    std::string result =
        fmt::format("basic-type:\n"
                    "  name:       {}\n"
                    "  parent:     {}\n"
                    "  dynamic:    {}\n"
                    "  size:       {}\n"
                    "  heap-base:  {}\n"
                    "  final:      {}\n"
                    "  fields:",
                    m_name, m_parent, m_dynamic, m_size_in_mem, m_heap_base, m_final);
    for (const auto &field : m_fields) {
        result += "\n    " + field.print();
    }
    return result;
}

// ============================================================================
// BitFieldType
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
    if (m_fields != other_bitfield->m_fields)
        result += "Bitfield fields differ\n";
    return result;
}

std::string BitFieldType::print() const {
    return fmt::format("#<bit-field-type {} size:{}>", m_name, m_size);
}

std::string BitFieldType::inspect() const {
    std::string result = fmt::format("bit-field-type:\n"
                                     "  name:   {}\n"
                                     "  size:   {}\n"
                                     "  fields:",
                                     m_name, m_size);
    for (const auto &bf : m_fields) {
        result += "\n    " + bf.print();
    }
    return result;
}

// ============================================================================
// EnumType
// ============================================================================

EnumType::EnumType(const ValueType *parent, std::string name, bool is_bitfield,
                   const std::unordered_map<std::string, int64_t> &entries)
    : ValueType(parent->get_parent(), std::move(name), parent->is_boxed(), parent->get_load_size(),
                parent->get_load_signed(), parent->get_preferred_reg_class()),
      m_is_bitfield(is_bitfield), m_entries(entries) {}

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
    if (m_is_bitfield != other_enum->m_is_bitfield)
        result += fmt::format("Is bitfield: {} vs {}\n", m_is_bitfield, other_enum->m_is_bitfield);
    if (m_entries != other_enum->m_entries)
        result += "Enum entries differ\n";
    return result;
}

std::string EnumType::print() const {
    return fmt::format("#<enum-type {} entries:{}>", m_name, m_entries.size());
}

std::string EnumType::inspect() const {
    std::string result = fmt::format("enum-type:\n"
                                     "  name:        {}\n"
                                     "  is-bitfield: {}\n"
                                     "  entries:",
                                     m_name, m_is_bitfield);
    for (const auto &[k, v] : m_entries) {
        result += fmt::format("\n    {} = {}", k, v);
    }
    return result;
}

std::string EnumType::get_name_for_value(int64_t value) const {
    for (const auto &[name, val] : m_entries) {
        if (val == value) {
            return name;
        }
    }
    return "";
}