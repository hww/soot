#pragma once

/*!
 * @file TypeSystem.h
 * The GOAL Type System.
 * Stores types, symbol types, methods, etc, and does typechecking,
 * lowest-common-ancestor, field access types, and reverse type lookups.
 */

#include "common/sooti/Object.hpp"
#include "common/type_system/Type.hpp"
#include "common/type_system/TypeSpec.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// Forward Declarations and Common Structures
// ============================================================================

struct ReverseLookupNode;

struct TypeFlags {
    union {
        uint64_t flag = 0;
        struct {
            uint16_t size;
            uint16_t heap_base;
            uint16_t methods;
            uint16_t pad;
        };
    };
};

struct FieldLookupInfo {
    Field    field;
    TypeSpec type;
    bool     needs_deref = true;
    int      array_size = -1;
};

struct BitfieldLookupInfo {
    TypeSpec result_type;
    int      offset = -1;
    int      size = -1;
    bool     sign_extend = false;
};

struct DerefInfo {
    bool     can_deref = false;
    bool     mem_deref = false;
    bool     sign_extend = false;
    RegClass reg = RegClass::INVALID;
    int      stride = -1;
    int      load_size = -1;
    TypeSpec result_type;
};

struct DerefKind {
    bool     is_store = false;
    int      size = -1;
    bool     sign_extend = false;
    RegClass reg_kind = RegClass::INVALID;
};

struct FieldReverseLookupInput {
    std::optional<DerefKind> deref = std::nullopt;
    int                      offset = 0;
    int                      stride = 0;
    TypeSpec                 base_type;
};

struct FieldReverseLookupOutput {
    struct Token {
        enum class Kind { FIELD, CONSTANT_IDX, VAR_IDX } kind;
        std::string name;
        int         idx = -1;
        double      field_score = 0.0;

        double score() const {
            switch (kind) {
            case Kind::FIELD:
                return field_score;
            case Kind::CONSTANT_IDX:
                return -10.0; // CONSTANT_INDEX_SCORE
            case Kind::VAR_IDX:
            default:
                return 0.;
            }
        }

        std::string print() const;
    };

    FieldReverseLookupOutput() = default;
    FieldReverseLookupOutput(bool addr, TypeSpec type, std::vector<Token> tok)
        : success(true), addr_of(addr), result_type(std::move(type)), tokens(std::move(tok)) {}

    bool               success = false;
    bool               addr_of = false;
    double             total_score = 0.;
    TypeSpec           result_type;
    std::vector<Token> tokens;

    bool has_variable_token() const {
        for (const auto &token : tokens) {
            if (token.kind == Token::Kind::VAR_IDX) {
                return true;
            }
        }
        return false;
    }
};

struct ReverseLookupNode {
    const ReverseLookupNode        *prev = nullptr;
    FieldReverseLookupOutput::Token token;

    std::vector<FieldReverseLookupOutput::Token> to_vector() const {
        std::vector<FieldReverseLookupOutput::Token> result;
        const ReverseLookupNode                     *current = this;
        while (current) {
            result.push_back(current->token);
            current = current->prev;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }
};

struct FieldReverseMultiLookupOutput {
    std::vector<FieldReverseLookupOutput> results;
    bool                                  success = false;
};

struct TypeSearchFieldInput {
    std::string field_type_name;
    int         field_offset;
};

// ============================================================================
// Main TypeSystem Class
// ============================================================================

class TypeSystem : public HeapObject {
    TypeSystem() = default; // Закрытый конструктор
  public:
    // Запрещаем копирование и присваивание
    TypeSystem(const TypeSystem &) = delete;
    TypeSystem &operator=(const TypeSystem &) = delete;

    // Разрешаем перемещение (если нужно)
    TypeSystem(TypeSystem &&) = default;
    TypeSystem &operator=(TypeSystem &&) = default;

    ~TypeSystem() = default;

    // Точка доступа к единственному экземпляру
    static TypeSystem &instance() {
        static std::shared_ptr<TypeSystem> inst = std::shared_ptr<TypeSystem>(new TypeSystem());
        return *inst;
    }

    // ========================================================================
    // Type Management
    // ========================================================================

    Type *add_type(const std::string &name, std::unique_ptr<Type> type);

    void forward_declare_type_as_type(const std::string &name);
    void forward_declare_type_as(const std::string &new_type, const std::string &parent_type);
    void forward_declare_type_method_count(const std::string &name, int num_methods);
    void forward_declare_type_method_count_multiple_of_4(const std::string &name, int num_methods);
    int  get_type_method_count(const std::string &name) const;
    std::optional<int> try_get_type_method_count(const std::string &name) const;
    std::string        get_runtime_type(const TypeSpec &ts);

    // ========================================================================
    // Type Lookup and Information
    // ========================================================================

    bool fully_defined_type_exists(const std::string &name) const;
    bool fully_defined_type_exists(const TypeSpec &type) const;
    bool partially_defined_type_exists(const std::string &name) const;

    TypeSpec make_typespec(const std::string &name) const;
    TypeSpec make_array_typespec(const std::string &array_type, const TypeSpec &element_type) const;
    TypeSpec make_function_typespec(const std::vector<std::string> &arg_types,
                                    const std::string              &return_type) const;

    TypeSpec make_pointer_typespec(const std::string &type) const;
    TypeSpec make_pointer_typespec(const TypeSpec &type) const;
    TypeSpec make_inline_array_typespec(const std::string &type) const;
    TypeSpec make_inline_array_typespec(const TypeSpec &type) const;

    Type *lookup_type(const TypeSpec &ts) const;
    Type *lookup_type(const std::string &name) const;

    Type *lookup_type_no_throw(const TypeSpec &ts) const;
    Type *lookup_type_no_throw(const std::string &name) const;

    Type *lookup_type_allow_partial_def(const TypeSpec &ts) const;
    Type *lookup_type_allow_partial_def(const std::string &name) const;

    int get_load_size_allow_partial_def(const TypeSpec &ts) const;

    bool is_array_data_access(const TypeSpec &array_type, int offset) const {
        if (array_type.base_type() == "array" && array_type.has_single_arg()) {
            return offset >= TypeConfig::array_data_offset;
        }
        return false;
    }

    int get_array_data_offset() const {
        return TypeConfig::array_data_offset;
    }

    int get_pointer_size() const {
        return TypeConfig::pointer_size;
    }
    // ========================================================================
    // Method System
    // ========================================================================

    MethodInfo override_method(Type *type, const std::string &method_name,
                               const std::optional<std::string> &docstring);

    MethodInfo declare_method(const std::string &type_name, const std::string &method_name,
                              const std::optional<std::string> &docstring, bool no_virtual,
                              const TypeSpec &ts, bool override_type);

    MethodInfo declare_method(Type *type, const std::string &method_name,
                              const std::optional<std::string> &docstring, bool no_virtual,
                              const TypeSpec &ts, bool override_type);

    MethodInfo overlay_method(Type *type, const std::string &method_name,
                              const std::string                &method_overlay_name,
                              const std::optional<std::string> &docstring, const TypeSpec &ts);

    MethodInfo define_method(const std::string &type_name, const std::string &method_name,
                             const TypeSpec &ts, const std::optional<std::string> &docstring);

    MethodInfo define_method(Type *type, const std::string &method_name, const TypeSpec &ts,
                             const std::optional<std::string> &docstring);

    MethodInfo add_new_method(Type *type, const TypeSpec &ts,
                              const std::optional<std::string> &docstring);

    MethodInfo lookup_method(const std::string &type_name, const std::string &method_name) const;
    MethodInfo lookup_method(const std::string &type_name, int method_id) const;

    bool try_lookup_method(const Type *type, const std::string &method_name,
                           MethodInfo *info) const;
    bool try_lookup_method(const std::string &type_name, const std::string &method_name,
                           MethodInfo *info) const;
    bool try_lookup_method(const std::string &type_name, int method_id, MethodInfo *info) const;

    MethodInfo lookup_new_method(const std::string &type_name) const;
    void assert_method_id(const std::string &type_name, const std::string &method_name, int id);

    // ========================================================================
    // Field System
    // ========================================================================

    FieldLookupInfo lookup_field_info(const std::string &type_name,
                                      const std::string &field_name) const;

    BitfieldLookupInfo lookup_bitfield_info(const std::string &type_name,
                                            const std::string &field_name) const;

    void assert_field_offset(const std::string &type_name, const std::string &field_name,
                             int offset);

    int add_field_to_type(StructureType *type, const std::string &field_name,
                          const TypeSpec &field_type, bool is_inline = false,
                          bool is_dynamic = false, int array_size = -1, int offset_override = -1,
                          bool skip_in_static_decomp = false, double score = 0.0,
                          const std::optional<TypeSpec> decomp_as_ts = std::nullopt);

    // ========================================================================
    // BitField System
    // ========================================================================

    bool is_bitfield_type(const std::string &type_name) const;
    void add_field_to_bitfield(BitFieldType *type, const std::string &field_name,
                               const TypeSpec &field_type, int offset, int field_size,
                               bool skip_in_decomp);

    // ========================================================================
    // Memory Access and Dereferencing
    // ========================================================================

    DerefInfo                     get_deref_info(const TypeSpec &ts) const;
    FieldReverseLookupOutput      reverse_field_lookup(const FieldReverseLookupInput &input) const;
    FieldReverseMultiLookupOutput reverse_field_multi_lookup(const FieldReverseLookupInput &input,
                                                             int max_count = 100) const;

    // ========================================================================
    // Virtual Method Handling
    // ========================================================================

    bool should_use_virtual_methods(const Type *type, int method_id) const;
    bool should_use_virtual_methods(const TypeSpec &type, int method_id) const;

    // ========================================================================
    // Type Checking and Inheritance
    // ========================================================================

    bool typecheck_and_throw(const TypeSpec &expected, const TypeSpec &actual,
                             const std::string &error_source_name = "", bool print_on_error = true,
                             bool throw_on_error = true, bool allow_type_alias = false) const;

    bool tc(const TypeSpec &less_specific, const TypeSpec &more_specific) const;

    TypeSpec lowest_common_ancestor(const TypeSpec &a, const TypeSpec &b) const;
    TypeSpec lowest_common_ancestor_reg(const TypeSpec &a, const TypeSpec &b) const;
    TypeSpec lowest_common_ancestor(const std::vector<TypeSpec> &types) const;

    // ========================================================================
    // Code Generation
    // ========================================================================

    std::string generate_deftype(const Type *type) const;
    std::string generate_deftype_for_structure(const StructureType *type) const;
    std::string generate_deftype_for_bitfield(const BitFieldType *type) const;
    std::string generate_deftype_footer(const Type *type) const;

    // ========================================================================
    // Built-in Types
    // ========================================================================

    void add_builtin_types();
    void verify_type_sizes();
    void add_builtin_types_z80();
    void verify_type_sizes_z80();

    void clear() {
        m_types.clear();
        m_forward_declared_types.clear();
        m_forward_declared_method_counts.clear();
        m_old_types.clear();
        m_types_allowed_to_be_redefined.clear();
        m_allow_redefinition = false;
    }

    // ========================================================================
    // Debugging and Inspection
    // ========================================================================

    std::string print() const override {
        return fmt::format("#<type-system {}>", m_variant.print());
    }
    script::Object           inspect() const override;
    std::string              print_all_type_information() const;
    script::Object           get_all_type_information() const;
    std::vector<std::string> get_path_up_tree(const std::string &type) const;
    int                      get_next_method_id(const Type *type) const;

    // ========================================================================
    // Type Search and Queries
    // ========================================================================

    void add_type_to_allowed_redefinition_list(const std::string &type_name) {
        m_types_allowed_to_be_redefined.push_back(type_name);
    }

    script::Object           get_all_type_names_as_objects() const;
    std::vector<std::string> get_all_type_names();
    std::vector<std::string> search_types_by_parent_type(
        const std::string                             &parent_type,
        const std::optional<std::vector<std::string>> &existing_matches = {});

    std::vector<std::string> search_types_by_parent_type_strict(const std::string &parent_type);

    std::vector<std::string> search_types_by_minimum_method_id(
        const int                                      minimum_method_id,
        const std::optional<std::vector<std::string>> &existing_matches = {});

    std::vector<std::string>
    search_types_by_size(const int min_size, const std::optional<int> max_size,
                         const std::optional<std::vector<std::string>> &existing_matches = {});

    std::vector<std::string>
    search_types_by_fields(const std::vector<TypeSearchFieldInput>       &search_fields,
                           const std::optional<std::vector<std::string>> &existing_matches = {});

    // ========================================================================
    // Template Helpers for Type Casting
    // ========================================================================

    template <typename T> T *get_type_of_type(const std::string &type_name) const {
        auto type = lookup_type(type_name);
        auto result = dynamic_cast<T *>(type);
        if (!result) {
            throw std::runtime_error("Type " + type_name + " is not of expected kind");
        }
        return result;
    }

    EnumType *try_enum_lookup(const std::string &type_name) const;
    EnumType *try_enum_lookup(const TypeSpec &type) const;

    int get_types_count() {
        return m_types.size();
    }
    const std::unordered_map<std::string, std::unique_ptr<Type>> &get_types() const {
        return m_types;
    }

    // ========================================================================
    // Utility Functions
    // ========================================================================

    int get_size_in_type(const Field &field) const;

    BasicType *add_builtin_basic(const std::string &parent, const std::string &type_name);

    ValueType *add_builtin_value_type(const std::string &parent, const std::string &type_name,
                                      int size, bool boxed = false, bool sign_extend = false,
                                      RegClass reg = RegClass::GPR_64);

    // Built-in type factories
    StructureType *add_builtin_structure(const std::string &parent, const std::string &type_name,
                                         bool boxed = false);

    Field lookup_field(const std::string &type_name, const std::string &field_name) const;

    // ========================================================================
    // Aliases Functions
    // ========================================================================

    // Переопределяем шаг, чтобы искать типы по их именам прямо в корне!
    Object make_step_accessor(const Object &key) override;

    Object to_alias() {
        return Object::make_native_ref(shared_from_this());
    }

    // ========================================================================
    // Check Arguments
    // ========================================================================
  public:
    Object build_typespec_from_env(const std::shared_ptr<EnvironmentObject> &env,
                                   const std::string                        &ret_type_name);

  private:
    // ========================================================================
    // Private Implementation
    // ========================================================================

    std::string lca_base(const std::string &a, const std::string &b) const;
    bool        typecheck_base_types(const std::string &expected, const std::string &actual,
                                     bool allow_alias) const;
    int         get_alignment_in_type(const Field &field);

    void builtin_structure_inherit(StructureType *st);

    // ========================================================================
    // Private Data Members
    // ========================================================================

    std::unordered_map<std::string, std::unique_ptr<Type>> m_types;
    std::unordered_map<std::string, std::string>           m_forward_declared_types;
    std::unordered_map<std::string, int>                   m_forward_declared_method_counts;

    std::vector<std::unique_ptr<Type>> m_old_types;
    std::vector<std::string>           m_types_allowed_to_be_redefined;
    bool                               m_allow_redefinition = false;

    Object m_variant;

  public:
    static int verbose;
};

// ============================================================================
// Global Utility Functions
// ============================================================================

TypeSpec coerce_to_reg_type(const TypeSpec &in);
