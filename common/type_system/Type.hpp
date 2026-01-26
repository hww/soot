#pragma once

/*!
 * @file Type.h
 * Representation of a GOAL type in the type system.
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <optional>
#include "common/CommonTypes.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/sooti/Export.hpp"

// Forward declarations
class Type;
class ValueType;
class ReferenceType;
class StructureType;
class BasicType;
class BitFieldType;
class EnumType;
class Field;
class BitField;
class MethodInfo;
class TypeSpec;

// ============================================================================
// Common Constants
// ============================================================================

enum class RegClass {
    GPR_8,
    GPR_16,
    GPR_32,
    GPR_64,
    FPR,
    INVALID
};

constexpr u32 SOOT_NEW_METHOD = 0;       // method ID of GOAL new
constexpr u32 SOOT_DEL_METHOD = 1;       // method ID of GOAL delete
constexpr u32 SOOT_PRINT_METHOD = 2;     // method ID of GOAL print
constexpr u32 SOOT_INSPECT_METHOD = 3;   // method ID of GOAL inspect
constexpr u32 SOOT_LENGTH_METHOD = 4;    // method ID of GOAL length
constexpr u32 SOOT_ASIZE_METHOD = 5;     // method ID of GOAL size
constexpr u32 SOOT_COPY_METHOD = 6;      // method ID of GOAL copy
constexpr u32 SOOT_RELOC_METHOD = 7;     // method ID of GOAL relocate
constexpr u32 SOOT_MEMUSAGE_METHOD = 8;  // method ID of GOAL mem-usage

struct TargetConfig {
    int pointer_size = 4;
    int array_data_offset = 12;
    int default_alignment = 4;
    // Можно добавить порядок байт (Endianness) и т.д.
};

// ============================================================================
// Definition Metadata
// ============================================================================

struct DefinitionMetadata {
    // Близко к оригиналу, но с удобными методами
    std::optional<script::ShortInfo> definition_info;
    std::optional<std::string> docstring;

    // Добавляем только convenience методы без изменения структуры данных
    bool has_location() const { return definition_info.has_value(); }
    bool has_docstring() const { return docstring.has_value(); }
    std::string get_docstring_or_empty() const { return docstring.value_or(""); }

    // Для совместимости с тестами
    bool operator==(const DefinitionMetadata& other) const {
        return definition_info == other.definition_info && docstring == other.docstring;
    }
    bool operator!=(const DefinitionMetadata& other) const { return !(*this == other); }
};

// ============================================================================
// Method Information
// ============================================================================

class MethodInfo {
    
public:
    int id = -1;
    std::string name;
    TypeSpec type;
    std::string defined_in_type;
    std::string type_name;
    bool no_virtual = false;
    bool overrides_parent = false;
    bool only_overrides_docstring = false;
    std::optional<std::string> docstring;  
    std::optional<std::string> overlay_name; 

    bool operator==(const MethodInfo& other) const;
    bool operator!=(const MethodInfo& other) const;
    std::string print_one_line() const;
    std::string diff(const MethodInfo& other) const;
};

// ============================================================================
// Field Definition
// ============================================================================

class Field {
public:
    Field() = default;
    Field(std::string name, TypeSpec type);
    Field(std::string name, TypeSpec type, int offset);

    void set_dynamic();
    void set_array(int size);
    void set_inline();
    void set_override_type(const TypeSpec& new_type);
    void mark_as_user_placed();

    std::string print() const;
    const TypeSpec& type() const { return m_type; }
    TypeSpec& type() { return m_type; }
    bool is_inline() const { return m_inline; }
    bool is_array() const { return m_array; }
    bool is_dynamic() const { return m_dynamic; }
    const std::string& name() const { return m_name; }
    int offset() const { return m_offset; }
    bool skip_in_decomp() const { return m_skip_in_static_decomp; }
    bool user_placed() const { return m_placed_by_user; }
    const std::optional<TypeSpec> decomp_as_type() const { return m_decomp_as_ts; }

    void set_comment(const std::string& comment) { m_comment = comment; }
    const std::string& comment() const { return m_comment; }
    bool has_comment() const { return !m_comment.empty(); }

    bool operator==(const Field& other) const;
    bool operator!=(const Field& other) const;
    std::string diff(const Field& other) const;

    int alignment() const {
        // ASSERT(m_alignment != -1); // Раскомментировать когда будет ASSERT
        return m_alignment;
    }

    int array_size() const {
        // ASSERT(is_array() && !is_dynamic());
        return m_array_size;
    }

    double field_score() const { return m_field_score; }
    void set_field_score(double value) { m_field_score = value; }
    void set_decomp_as_ts(const TypeSpec& ts) { m_decomp_as_ts = ts; }

private:
    friend class TypeSystem;
    void set_alignment(int alignment) { m_alignment = alignment; }
    void set_offset(int offset) { m_offset = offset; }
    void set_skip_in_static_decomp() { m_skip_in_static_decomp = true; }

    std::string m_name;
    TypeSpec m_type;
    bool m_override_type = false;
    int m_offset = -1;
    bool m_inline = false;
    bool m_dynamic = false;
    bool m_array = false;
    int m_array_size = 0;
    int m_alignment = -1;
    bool m_skip_in_static_decomp = false;
    bool m_placed_by_user = false;
    std::string m_comment;
    double m_field_score = 0.0;
    std::optional<TypeSpec> m_decomp_as_ts = std::nullopt;
};

// ============================================================================
// BitField Definition
// ============================================================================

class BitField {
public:
    BitField() = default;
    BitField(TypeSpec type, std::string name, int offset, int size, bool skip_in_decomp);

    const std::string name() const { return m_name; }
    int offset() const { return m_offset; }
    int size() const { return m_size; }
    const TypeSpec& type() const { return m_type; }
    bool skip_in_decomp() const { return m_skip_in_static_decomp; }

    bool operator==(const BitField& other) const;
    bool operator!=(const BitField& other) const;
    std::string print() const;
    std::string diff(const BitField& other) const;

private:
    TypeSpec m_type;
    std::string m_name;
    int m_offset = -1;
    int m_size = -1;
    bool m_skip_in_static_decomp = false;
};

// ============================================================================
// Base Type Definition
// ============================================================================

class Type {
public:
    static int verbose;
public:
    Type(std::string parent, std::string name, bool is_boxed, int heap_base);
    virtual ~Type() = default;

    // Core type properties - PURE VIRTUAL
    virtual bool is_reference() const = 0;
    virtual int get_load_size() const = 0;
    virtual bool get_load_signed() const = 0;
    virtual int get_size_in_memory() const = 0;
    virtual RegClass get_preferred_reg_class() const = 0;
    virtual int get_offset() const = 0;
    virtual int get_in_memory_alignment() const = 0;
    virtual int get_inline_array_stride_alignment() const = 0;
    virtual int get_inline_array_start_alignment() const = 0;

    // Comparison
    virtual bool operator==(const Type& other) const = 0;
    bool operator!=(const Type& other) const { return !(*this == other); }

    // Printing and debugging
    virtual std::string print() const = 0;
    std::string diff(const Type& other) const;

    // Method system
    bool get_my_method(const std::string& name, MethodInfo* out) const;
    bool get_my_method(int id, MethodInfo* out) const;
    bool get_my_last_method(MethodInfo* out) const;
    bool get_my_new_method(MethodInfo* out) const;
    int get_num_methods() const;
    const MethodInfo& add_method(const MethodInfo& info);
    const MethodInfo& add_new_method(const MethodInfo& info);
    std::string print_method_info() const;

    // New method access
    const MethodInfo* get_new_method_defined_for_type() const {
        if (m_new_method_info_defined) {
            return &m_new_method_info;
        }
        else {
            return nullptr;
        }
    }

    bool has_new_method() const { return m_new_method_info_defined; }

    // State system
    void add_state(const std::string& name, const TypeSpec& type);
    const std::vector<MethodInfo>& get_methods_defined_for_type() const { return m_methods; }
    const std::map<std::string, TypeSpec>& get_states_declared_for_type() const { return m_states; }

    // Accessors
    void set_runtime_type(std::string name) { m_runtime_name = std::move(name); }
    std::string get_name() const { return m_name; }
    std::string get_runtime_name() const;
    std::string get_parent() const { return m_parent; }
    void set_runtime_name(std::string name) { m_runtime_name = std::move(name); }
    bool has_parent() const { return !m_parent.empty() && m_name != "object"; }

    bool is_boxed() const { return m_is_boxed; }
    int heap_base() const { return m_heap_base; }
    bool gen_inspect() const { return m_generate_inspect; }

    void disallow_in_runtime() { m_allow_in_runtime = false; }

    // Metadata

    // Virtual state metadata
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>&
        get_virtual_state_definition_meta() { return m_virtual_state_definition_meta; }

    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>&
        get_state_definition_meta() { return m_state_definition_meta; }

    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>
        m_virtual_state_definition_meta = {};
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>
        m_state_definition_meta = {};

    // Metadata
    DefinitionMetadata m_metadata;

protected:
    virtual std::string diff_impl(const Type& other) const = 0;
    std::string incompatible_diff(const Type& other) const;
    bool common_type_info_equal(const Type& other) const;
    std::string common_type_info_diff(const Type& other) const;

    std::string m_parent;
    std::string m_name;
    std::string m_runtime_name;
    bool m_allow_in_runtime = true;
    bool m_is_boxed = false;
    int m_heap_base = 0;
    bool m_generate_inspect = true;

    // Method system
    std::vector<MethodInfo> m_methods;
    MethodInfo m_new_method_info;
    bool m_new_method_info_defined = false;

    // State system
    std::map<std::string, TypeSpec> m_states;

};

// ============================================================================
// Null Type
// ============================================================================

class NullType : public Type {
public:
    NullType(std::string name);

    bool is_reference() const override;
    int get_load_size() const override;
    bool get_load_signed() const override;
    int get_size_in_memory() const override;
    RegClass get_preferred_reg_class() const override;
    int get_offset() const override;
    int get_in_memory_alignment() const override;
    int get_inline_array_stride_alignment() const override;
    int get_inline_array_start_alignment() const override;

    std::string print() const override;
    bool operator==(const Type& other) const override;

protected:
    std::string diff_impl(const Type& other) const override;
};

// ============================================================================
// Value Types
// ============================================================================

class ValueType : public Type {
public:
    ValueType(std::string parent, std::string name, bool is_boxed, int size, bool sign_extend, RegClass reg);

    bool is_reference() const override;
    int get_load_size() const override;
    bool get_load_signed() const override;
    int get_size_in_memory() const override;
    RegClass get_preferred_reg_class() const override;
    int get_offset() const override;
    int get_in_memory_alignment() const override;
    int get_inline_array_stride_alignment() const override;
    int get_inline_array_start_alignment() const override;

    std::string print() const override;
    bool operator==(const Type& other) const override;

    void inherit(const ValueType* parent);

protected:
    friend class TypeSystem;
    void set_offset(int offset) { m_offset = offset; }
    std::string diff_impl(const Type& other) const override;

    int m_size = -1;
    int m_offset = 0;
    bool m_sign_extend = false;
    RegClass m_reg_kind = RegClass::INVALID;
};

// ============================================================================
// Reference Types
// ============================================================================

class ReferenceType : public Type {
public:
    ReferenceType(std::string parent, std::string name, bool is_boxed, int heap_base);

    bool is_reference() const override { return true; }
    int get_load_size() const override { return 4; } // pointers are 4 bytes
    bool get_load_signed() const override { return false; }
    RegClass get_preferred_reg_class() const override { return RegClass::GPR_64; }

    std::string print() const override;

    // These remain pure virtual - must be implemented by derived classes
    int get_size_in_memory() const override = 0;
    int get_offset() const override = 0;
    int get_in_memory_alignment() const override = 0;
    int get_inline_array_stride_alignment() const override = 0;
    int get_inline_array_start_alignment() const override = 0;
};

// ============================================================================
// Structure Types
// ============================================================================

class StructureType : public ReferenceType {
public:
    StructureType(std::string parent, std::string name, bool boxed, bool dynamic, bool pack, int heap_base);

    std::string print() const override;
    void inherit(StructureType* parent);
    bool operator==(const Type& other) const override;

    int get_size_in_memory() const override { return m_size_in_mem; }
    int get_offset() const override { return m_offset; }
    int get_in_memory_alignment() const override { return 16; } // STRUCTURE_ALIGNMENT
    int get_inline_array_stride_alignment() const override { return m_pack ? 1 : 16; }
    int get_inline_array_start_alignment() const override { return (m_pack || m_allow_misalign) ? 1 : 16; }

    bool lookup_field(const std::string& name, Field* out);
    bool is_dynamic() const { return m_dynamic; }

    const std::vector<Field>& fields() const { return m_fields; }
    bool is_packed() const { return m_pack; }
    bool is_allowed_misalign() const { return m_allow_misalign; }
    bool is_always_stack_singleton() const { return m_always_stack_singleton; }

    void set_pack(bool pack) { m_pack = pack; }
    void set_always_stack_singleton() { m_always_stack_singleton = true; }
    void set_heap_base(int hb) { m_heap_base = hb; }
    void set_allow_misalign(bool misalign) { m_allow_misalign = misalign; }
    void set_gen_inspect(bool gen_inspect) { m_generate_inspect = gen_inspect; }
    int size() const { return m_size_in_mem; }
    void override_field_type(const std::string& field_name, const TypeSpec& new_type);

protected:
    friend class TypeSystem;
    void override_offset(int offset) { m_offset = offset; }
    void override_size_in_memory(int size) { m_size_in_mem = size; }
    void add_field(const Field& f, int new_size_in_mem) {
        m_fields.push_back(f);
        m_size_in_mem = new_size_in_mem;
    }
    void set_dynamic() { m_dynamic = true; }
    size_t first_unique_field_idx() const { return m_idx_of_first_unique_field; }
    std::string diff_impl(const Type& other) const override;
    std::string diff_structure_common(const StructureType& other) const;

    std::vector<Field> m_fields;
    std::vector<int> m_overriden_fields;
    bool m_dynamic = false;
    int m_size_in_mem = 0;
    bool m_pack = false;
    bool m_allow_misalign = false;
    int m_offset = 0;
    bool m_always_stack_singleton = false;
    size_t m_idx_of_first_unique_field = 0;
};

// ============================================================================
// Basic Types
// ============================================================================

class BasicType : public StructureType {
public:
    BasicType(std::string parent, std::string name, bool dynamic, int heap_base);

    int get_offset() const override { return 0; } // BASIC_OFFSET
    int get_inline_array_start_alignment() const override { return 16; }
    std::string print() const override;
    bool operator==(const Type& other) const override;

    bool final() const { return m_final; }
    void set_final() { m_final = true; }

protected:
    std::string diff_impl(const Type& other) const override;

    bool m_final = false;
};

// ============================================================================
// BitField Types
// ============================================================================

class BitFieldType : public ValueType {
public:
    BitFieldType(std::string parent, std::string name, int size, bool sign_extend);

    bool lookup_field(const std::string& name, BitField* out) const;
    std::string print() const override;
    bool operator==(const Type& other) const override;

    const std::vector<BitField>& fields() const { return m_fields; }
    void set_gen_inspect(bool gen_inspect) { m_generate_inspect = gen_inspect; }

protected:
    friend class TypeSystem;
    std::string diff_impl(const Type& other) const override;

    std::vector<BitField> m_fields;
};

// ============================================================================
// Enum Types
// ============================================================================

class EnumType : public ValueType {
public:
    EnumType(const ValueType* parent, std::string name, bool is_bitfield,
        const std::unordered_map<std::string, int64_t>& entries);

    std::string print() const override;
    bool operator==(const Type& other) const override;

    const std::unordered_map<std::string, int64_t>& entries() const { return m_entries; }
    bool is_bitfield() const { return m_is_bitfield; }

protected:
    friend class TypeSystem;
    std::string diff_impl(const Type& other) const override;

    bool m_is_bitfield = false;
    std::unordered_map<std::string, int64_t> m_entries;
};
