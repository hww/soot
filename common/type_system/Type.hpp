#pragma once

/*!
 * @file Type.h
 * Representation of a GOAL type in the type system.
 */

#include "Config.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Archive.hpp"
#include "common/type_system/TypeSpec.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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

using namespace script;

// ============================================================================
// Definition Metadata
// ============================================================================

struct DefinitionMetadata {
    // Близко к оригиналу, но с удобными методами
    std::optional<ShortInfo>   definition_info;
    std::optional<std::string> docstring;

    // Добавляем только convenience методы без изменения структуры данных
    bool has_location() const {
        return definition_info.has_value();
    }
    bool has_docstring() const {
        return docstring.has_value();
    }
    std::string get_docstring_or_empty() const {
        return docstring.value_or("");
    }

    // Для совместимости с тестами
    bool operator==(const DefinitionMetadata &other) const {
        return definition_info == other.definition_info && docstring == other.docstring;
    }
    bool operator!=(const DefinitionMetadata &other) const {
        return !(*this == other);
    }
};

// ============================================================================
// Method Information
// ============================================================================

class MethodInfo : public NativeObject {

  public:
    MethodInfo() {}
    MethodInfo(int id, std::string name, TypeSpec type, std::string defined_in,
               std::string type_name, bool no_virtual, bool overrides, bool only_doc,
               std::optional<std::string> doc, std::optional<std::string> overlay)
        : id(id), name(std::move(name)), type(std::move(type)),
          defined_in_type(std::move(defined_in)), type_name(std::move(type_name)),
          no_virtual(no_virtual), overrides_parent(overrides), only_overrides_docstring(only_doc),
          docstring(std::move(doc)), overlay_name(std::move(overlay)) {}

    int                        id = -1;
    std::string                name;
    TypeSpec                   type;
    std::string                defined_in_type;
    std::string                type_name;
    bool                       no_virtual = false;
    bool                       overrides_parent = false;
    bool                       only_overrides_docstring = false;
    std::optional<std::string> docstring;
    std::optional<std::string> overlay_name;

    bool        operator==(const MethodInfo &other) const;
    bool        operator!=(const MethodInfo &other) const;
    std::string print_one_line() const;
    std::string diff(const MethodInfo &other) const;

    std::string print() const override {
        return "#<method-info>";
    }

    std::string full_class_name() const override {
        return "MethodInfo";
    }
    std::string class_name() const override {
        return "method-info";
    }
    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }
    bool is_class_name(const Object &name) const override {
        return name == MethodInfo::type_name_obj() || NativeObject::is_class_name(name);
    }

    Object inspect() const override {
        ListBuilder builder;
        builder.add_key_value("id", Object::make_integer(id));
        builder.add_key_value("name", Object::make_string(name));
        builder.add_key_value("type", type.inspect());
        builder.add_key_value("defined-in-type", Object::make_string(defined_in_type));
        builder.add_key_value("type-name", Object::make_string(type_name));
        builder.add_key_value("no-virtual", Object::make_boolean(no_virtual));
        builder.add_key_value("overrides-parent", Object::make_boolean(overrides_parent));
        builder.add_key_value("only-overrides-docstring",
                              Object::make_boolean(only_overrides_docstring));
        builder.add_key_value("docstring", Object::make_string(docstring.value_or("")));
        builder.add_key_value("overlay-name", Object::make_string(overlay_name.value_or("")));
        return builder.build();
    }

    Object get_at(const Object &key) override;
};

// ============================================================================
// Field Definition
// ============================================================================

class Field : public NativeObject {
  public:
    Field() {};
    Field(std::string name, TypeSpec type);
    Field(std::string name, TypeSpec type, int offset);

    void set_dynamic();
    void set_array(int size);
    void set_inline();
    void set_override_type(const TypeSpec &new_type);
    void mark_as_user_placed();

    std::string print() const override;
    Object      inspect() const override;

    std::string full_class_name() const override {
        return "Field";
    }
    std::string class_name() const override {
        return "field";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == Field::type_name_obj() || NativeObject::is_class_name(name);
    }

    const TypeSpec &type() const {
        return m_type;
    }
    TypeSpec &type() {
        return m_type;
    }
    bool is_inline() const {
        return m_inline;
    }
    bool is_array() const {
        return m_array;
    }
    bool is_dynamic() const {
        return m_dynamic;
    }
    const std::string &name() const {
        return m_name;
    }
    int offset() const {
        return m_offset;
    }
    bool skip_in_decomp() const {
        return m_skip_in_static_decomp;
    }
    bool user_placed() const {
        return m_placed_by_user;
    }
    const std::optional<TypeSpec> decomp_as_type() const {
        return m_decomp_as_ts;
    }

    void set_comment(const std::string &comment) {
        m_comment = comment;
    }
    const std::string &comment() const {
        return m_comment;
    }
    bool has_comment() const {
        return !m_comment.empty();
    }

    bool        operator==(const Field &other) const;
    bool        operator!=(const Field &other) const;
    std::string diff(const Field &other) const;

    int alignment() const {
        // ASSERT(m_alignment != -1); // Раскомментировать когда будет ASSERT
        return m_alignment;
    }

    int array_size() const {
        // ASSERT(is_array() && !is_dynamic());
        return m_array_size;
    }

    double field_score() const {
        return m_field_score;
    }
    void set_field_score(double value) {
        m_field_score = value;
    }
    void set_decomp_as_ts(const TypeSpec &ts) {
        m_decomp_as_ts = ts;
    }

    Object get_at(const Object &key) override;

  private:
    friend class TypeSystem;
    void set_alignment(int alignment) {
        m_alignment = alignment;
    }
    void set_offset(int offset) {
        m_offset = offset;
    }
    void set_skip_in_static_decomp() {
        m_skip_in_static_decomp = true;
    }

    std::string             m_name;
    TypeSpec                m_type;
    bool                    m_override_type = false;
    int                     m_offset = -1;
    bool                    m_inline = false;
    bool                    m_dynamic = false;
    bool                    m_array = false;
    int                     m_array_size = 0;
    int                     m_alignment = -1;
    bool                    m_skip_in_static_decomp = false;
    bool                    m_placed_by_user = false;
    std::string             m_comment;
    double                  m_field_score = 0.0;
    std::optional<TypeSpec> m_decomp_as_ts = std::nullopt;
};

// ============================================================================
// BitField Definition
// ============================================================================

class BitField : public NativeObject {
  public:
    BitField() {};
    BitField(TypeSpec type, std::string name, int offset, int size, bool skip_in_decomp);

    std::string full_class_name() const override {
        return "BitField";
    }
    std::string class_name() const override {
        return "bit-field";
    }
    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == BitField::type_name_obj() || NativeObject::is_class_name(name);
    }

    const std::string name() const {
        return m_name;
    }
    int offset() const {
        return m_offset;
    }
    int size() const {
        return m_size;
    }
    const TypeSpec &type() const {
        return m_type;
    }
    bool skip_in_decomp() const {
        return m_skip_in_static_decomp;
    }

    bool        operator==(const BitField &other) const;
    bool        operator!=(const BitField &other) const;
    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_key_value("name", Object::make_string(name()));
        builder.add_key_value("type", type().inspect());
        builder.add_key_value("offset", Object::make_integer(offset()));
        builder.add_key_value("size", Object::make_integer(size()));
        builder.add_key_value("skip-in-decomp", Object::make_boolean(skip_in_decomp()));
        return builder.build();
    }
    std::string diff(const BitField &other) const;

    Object get_at(const Object &key) override;

  private:
    TypeSpec    m_type;
    std::string m_name;
    int         m_offset = -1;
    int         m_size = -1;
    bool        m_skip_in_static_decomp = false;
};

// ============================================================================
// Base Type Definition
// ============================================================================

class Type : public NativeObject {
  public:
    static int verbose;

  public:
    Type(std::string parent, std::string name, bool is_boxed, int heap_base);
    virtual ~Type() = default;

    std::string full_class_name() const override {
        return "Type";
    }
    std::string class_name() const override {
        return "type";
    }
    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == Type::type_name_obj() || NativeObject::is_class_name(name);
    }

    uint32_t get_type_tag() {
        return util::compute_crc32(get_name());
    }

    // Core type properties - PURE VIRTUAL
    virtual bool     is_reference() const = 0;
    virtual int      get_load_size() const = 0;
    virtual bool     get_load_signed() const = 0;
    virtual int      get_size_in_memory() const = 0;
    virtual RegClass get_preferred_reg_class() const = 0;
    virtual int      get_offset() const = 0;
    virtual int      get_in_memory_alignment() const = 0;
    virtual int      get_inline_array_stride_alignment() const = 0;
    virtual int      get_inline_array_start_alignment() const = 0;

    // Comparison
    virtual bool operator==(const Type &other) const = 0;
    bool         operator!=(const Type &other) const {
        return !(*this == other);
    }

    // Printing and debugging
    virtual std::string print() const override {
        return "#<" + class_name() + " " + get_name() + ">";
    }
    virtual Object inspect() const override {
        ListBuilder builder;
        builder.add_symbol(class_name());
        builder.add_key_value("name", Object::make_string(get_name()));
        return builder.build();
    }

    std::string diff(const Type &other) const;
    Object      get_at(const Object &key) override;

    // Method system
    bool              get_my_method(const std::string &name, MethodInfo *out) const;
    bool              get_my_method(int id, MethodInfo *out) const;
    bool              get_my_last_method(MethodInfo *out) const;
    bool              get_my_new_method(MethodInfo *out) const;
    int               get_method_id(const std::string &name) const;
    int               get_num_methods() const;
    const MethodInfo &add_method(const MethodInfo &info);
    const MethodInfo &add_new_method(const MethodInfo &info);
    std::string       print_method_info() const;

    // New method access
    const MethodInfo *get_new_method_defined_for_type() const {
        if (m_new_method_info_defined) {
            return &m_new_method_info;
        } else {
            return nullptr;
        }
    }

    bool has_new_method() const {
        return m_new_method_info_defined;
    }

    const std::vector<MethodInfo> &get_methods_defined_for_type() const {
        return m_methods;
    }

    size_t methods_max_id() const;
    size_t get_methods_count() const { return methods_max_id() + 1; } 

    // State system
    void add_state(const std::string &name, const TypeSpec &type);
    bool get_my_state(const std::string &name, TypeSpec *out) const;
    size_t states_count() const { return m_states.size(); }

    const std::map<std::string, TypeSpec> &get_states_declared_for_type() const {
        return m_states;
    }

    // NativeObjects
    void set_runtime_type(std::string name) {
        m_runtime_name = std::move(name);
    }
    std::string get_name() const {
        return m_name;
    }
    std::string get_runtime_name() const;
    
    std::string get_parent() const {
        return m_parent;
    }
    void set_runtime_name(std::string name) {
        m_runtime_name = std::move(name);
    }
    bool has_parent() const {
        return !m_parent.empty() && m_name != "object";
    }

    bool is_boxed() const {
        return m_is_boxed;
    }
    int heap_base() const {
        return m_heap_base;
    }
    bool gen_inspect() const {
        return m_generate_inspect;
    }

    void disallow_in_runtime() {
        m_allow_in_runtime = false;
    }

    // Metadata
    DefinitionMetadata& get_metadata() { return m_metadata;}

    // State metadata
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>> &
    get_state_definition_meta() {
        return m_state_definition_meta;
    }
    DefinitionMetadata& get_metadata(const std::string& state_name, const std::string& handler_name) {
        return m_state_definition_meta[state_name][handler_name];
    }

    // Virtual state metadata
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>> &
    get_virtual_state_definition_meta() {
        return m_virtual_state_definition_meta;
    }
    DefinitionMetadata& get_virtual_metadata(const std::string& state_name, const std::string& handler_name) {
        return m_virtual_state_definition_meta[state_name][handler_name];
    }

    // Metadata
    DefinitionMetadata m_metadata;
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>
        m_virtual_state_definition_meta = {};
    std::unordered_map<std::string, std::unordered_map<std::string, DefinitionMetadata>>
        m_state_definition_meta = {};

    // Serialization
    virtual bool serialize_obj(Archive &ar, Object &data) = 0;
    
  protected:
    virtual std::string diff_impl(const Type &other) const = 0;
    std::string         incompatible_diff(const Type &other) const;
    bool                common_type_info_equal(const Type &other) const;
    std::string         common_type_info_diff(const Type &other) const;

    std::string m_parent;
    std::string m_name;
    std::string m_runtime_name;
    bool        m_allow_in_runtime = true;
    bool        m_is_boxed = false;
    bool        m_generate_inspect = true;
    int         m_heap_base = 0;

    // Method system
    std::vector<MethodInfo> m_methods;
    MethodInfo              m_new_method_info;
    bool                    m_new_method_info_defined = false;

    // State system
    std::map<std::string, TypeSpec> m_states;
};

// ============================================================================
// Null Type
// ============================================================================

class NullType : public Type {
  public:
    NullType(std::string name);

    std::string full_class_name() const override {
        return "NullType";
    }
    std::string class_name() const override {
        return "null-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == NullType::type_name_obj() || Type::is_class_name(name);
    }

    bool     is_reference() const override;
    int      get_load_size() const override;
    bool     get_load_signed() const override;
    int      get_size_in_memory() const override;
    RegClass get_preferred_reg_class() const override;
    int      get_offset() const override;
    int      get_in_memory_alignment() const override;
    int      get_inline_array_stride_alignment() const override;
    int      get_inline_array_start_alignment() const override;

    std::string print() const override;
    Object      inspect() const override {
        ListBuilder lb;
        lb.add_symbol("null-type");
        lb.add_symbol("null");
        return lb.build();
    }
    bool operator==(const Type &other) const override;

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override {
        (void)ar;
        (void)data;
        return false;
    }

  protected:
    std::string diff_impl(const Type &other) const override;
};

// ============================================================================
// Value Types
// ============================================================================

class ValueType : public Type {
  public:
    ValueType(std::string parent, std::string name, bool is_boxed, int size, bool sign_extend,
              RegClass reg);

    std::string full_class_name() const override {
        return "ValueType";
    }
    std::string class_name() const override {
        return "value-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == ValueType::type_name_obj() || Type::is_class_name(name);
    }

    bool     is_reference() const override;
    int      get_load_size() const override;
    bool     get_load_signed() const override;
    int      get_size_in_memory() const override;
    RegClass get_preferred_reg_class() const override;
    int      get_offset() const override;
    int      get_in_memory_alignment() const override;
    int      get_inline_array_stride_alignment() const override;
    int      get_inline_array_start_alignment() const override;

    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_symbol("value-type");
        builder.add_key_value("size", Object::make_integer(m_size));
        builder.add_key_value("offset", Object::make_integer(m_offset));
        builder.add_key_value("sign-extend", Object::make_boolean(m_sign_extend));
        builder.add_key_value("reg-kind", Object::make_symbol(reg_kind_to_string(m_reg_kind)));
        return builder.build();
    }
    bool operator==(const Type &other) const override;

    void inherit(const ValueType *parent);

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

  protected:
    friend class TypeSystem;
    void set_offset(int offset) {
        m_offset = offset;
    }
    std::string diff_impl(const Type &other) const override;

    int      m_size = -1;
    int      m_offset = 0;
    bool     m_sign_extend = false;
    RegClass m_reg_kind = RegClass::INVALID;
};

// ============================================================================
// Reference Types
// ============================================================================

class ReferenceType : public Type {
  public:
    ReferenceType(std::string parent, std::string name, bool is_boxed, int heap_base);

    std::string full_class_name() const override {
        return "ReferenceType";
    }
    std::string class_name() const override {
        return "reference-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == ReferenceType::type_name_obj() || Type::is_class_name(name);
    }
    bool is_reference() const override {
        return true;
    }
    int get_load_size() const override {
        return TypeConfig::pointer_size;
    } // pointers are 4 bytes
    bool get_load_signed() const override {
        return false;
    }
    RegClass get_preferred_reg_class() const override {
        return TypeConfig::pointer_reg_class;
    }

    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_symbol("reference-type");
        return builder.build();
    }

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

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
    StructureType(std::string parent, std::string name, bool boxed, bool dynamic, bool pack,
                  int heap_base);

    std::string full_class_name() const override {
        return "StructureType";
    }
    std::string class_name() const override {
        return "structure-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == StructureType::type_name_obj() || ReferenceType::is_class_name(name);
    }
    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_symbol("structure-type");
        return builder.build();
    }

    void inherit(StructureType *parent);
    bool operator==(const Type &other) const override;

    int get_size_in_memory() const override {
        return m_size_in_mem;
    }
    int get_offset() const override {
        return m_offset;
    }
    int get_in_memory_alignment() const override {
        return TypeConfig::struct_alignment;
    }

    // STRUCTURE_ALIGNMENT
    int get_inline_array_stride_alignment() const override {
        return m_pack ? 1 : TypeConfig::struct_array_stride_alignment;
    }
    int get_inline_array_start_alignment() const override {
        return (m_pack || m_allow_misalign) ? 1 : TypeConfig::struct_array_start_alignment;
    }

    bool lookup_field(const std::string &name, Field *out);

    bool is_dynamic() const {
        return m_dynamic;
    }
    const std::vector<Field> &fields() const {
        return m_fields;
    }
    bool is_packed() const {
        return m_pack;
    }
    bool is_allowed_misalign() const {
        return m_allow_misalign;
    }
    bool is_always_stack_singleton() const {
        return m_always_stack_singleton;
    }

    void set_pack(bool pack) {
        m_pack = pack;
    }
    void set_always_stack_singleton() {
        m_always_stack_singleton = true;
    }
    void set_heap_base(int hb) {
        m_heap_base = hb;
    }
    void set_allow_misalign(bool misalign) {
        m_allow_misalign = misalign;
    }
    void set_gen_inspect(bool gen_inspect) {
        m_generate_inspect = gen_inspect;
    }
    int size() const {
        return m_size_in_mem;
    }
    void override_field_type(const std::string &field_name, const TypeSpec &new_type);

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

  protected:
    friend class TypeSystem;

    void override_offset(int offset) {
        m_offset = offset;
    }
    void override_size_in_memory(int size) {
        m_size_in_mem = size;
    }
    void add_field(const Field &f, int new_size_in_mem) {
        m_fields.push_back(f);
        m_size_in_mem = new_size_in_mem;
    }
    void set_dynamic() {
        m_dynamic = true;
    }
    size_t first_unique_field_idx() const {
        return m_idx_of_first_unique_field;
    }
    std::string diff_impl(const Type &other) const override;
    std::string diff_structure_common(const StructureType &other) const;

    std::vector<Field> m_fields;
    std::vector<int>   m_overriden_fields;
    bool               m_dynamic = false;
    int                m_size_in_mem = 0;
    bool               m_pack = false;
    bool               m_allow_misalign = false;
    int                m_offset = 0;
    bool               m_always_stack_singleton = false;
    size_t             m_idx_of_first_unique_field = 0;
};

// ============================================================================
// Basic Types
// ============================================================================

class BasicType : public StructureType {
  public:
    BasicType(std::string parent, std::string name, bool dynamic, int heap_base);

    std::string full_class_name() const override {
        return "BasicType";
    }
    std::string class_name() const override {
        return "basic-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == BasicType::type_name_obj() || StructureType::is_class_name(name);
    }
    int get_offset() const override {
        return 0;
    } // BASIC_OFFSET
    int get_inline_array_start_alignment() const override {
        return TypeConfig::basic_array_start_alignment;
    }
    std::string print() const override;
    Object      inspect() const override {
        ListBuilder lb;
        lb.add_symbol("basic-type");
        lb.add_string(get_name());
        return lb.build();
    }
    bool operator==(const Type &other) const override;

    bool final() const {
        return m_final;
    }
    void set_final() {
        m_final = true;
    }

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

  protected:
    std::string diff_impl(const Type &other) const override;

    bool m_final = false;
};

// ============================================================================
// BitField Types
// ============================================================================

class BitFieldType : public ValueType {
  public:
    BitFieldType(std::string parent, std::string name, int size, bool sign_extend);

    std::string full_class_name() const override {
        return "BitFieldType";
    }
    std::string class_name() const override {
        return "bit-field-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == BitFieldType::type_name_obj() || ValueType::is_class_name(name);
    }
    bool        lookup_field(const std::string &name, BitField *out) const;
    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_symbol("bitfield-type");
        builder.add_string(get_name());
        return builder.build();
    }
    bool operator==(const Type &other) const override;

    const std::vector<BitField> &fields() const {
        return m_fields;
    }
    void set_gen_inspect(bool gen_inspect) {
        m_generate_inspect = gen_inspect;
    }

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

  protected:
    friend class TypeSystem;
    std::string diff_impl(const Type &other) const override;

    std::vector<BitField> m_fields;
};

// ============================================================================
// Enum Types
// ============================================================================

class EnumType : public ValueType {
  public:
    EnumType(const ValueType *parent, std::string name, bool is_bitfield,
             const std::unordered_map<std::string, int64_t> &entries);

    std::string full_class_name() const override {
        return "EnumType";
    }
    std::string class_name() const override {
        return "enum-type";
    }
    bool is_class_name(const Object &name) const override {
        return name == EnumType::type_name_obj() || ValueType::is_class_name(name);
    }
    std::string print() const override;
    Object      inspect() const override {
        ListBuilder builder;
        builder.add_symbol("enum-type");
        builder.add_string(get_name());
        builder.add_key_value("size", Object::make_integer(m_entries.size()));
        return builder.build();
    }
    bool operator==(const Type &other) const override;

    const std::unordered_map<std::string, int64_t> &entries() const {
        return m_entries;
    }
    bool is_bitfield() const {
        return m_is_bitfield;
    }

    std::string get_name_for_value(int64_t value) const;

    Object get_at(const Object &key) override;

    bool serialize_obj(Archive &ar, Object &data) override;

  protected:
    friend class TypeSystem;
    std::string diff_impl(const Type &other) const override;

  protected:
    bool                                     m_is_bitfield = false;
    std::unordered_map<std::string, int64_t> m_entries;
};
