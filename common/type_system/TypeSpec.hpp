#pragma once

/*!
 * @file TypeSpec.h
 * A GOAL TypeSpec is a reference to a type or compound type.
 */

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/soot/ListBuilder.hpp"
#include "common/soot/Object.hpp"

namespace soot {
class Object;
}; // namespace soot

using namespace soot;

// Forward declaration
class Type;

// ============================================================================
// Type Tag
// ============================================================================

struct TypeTag {
    std::string name;
    std::string value;

    TypeTag() = default;
    TypeTag(std::string name, std::string value);

    bool operator==(const TypeTag &other) const;
    bool operator!=(const TypeTag &other) const;
};

// ============================================================================
// TypeSpec
// ============================================================================

class TypeSpec : public NativeObject {
  public:
    // Constructors
    TypeSpec() = default;
    explicit TypeSpec(std::string type);
    TypeSpec(std::string type, std::vector<TypeSpec> arguments);

    // Rule of Five
    TypeSpec(const TypeSpec &other);
    TypeSpec(TypeSpec &&other) noexcept;
    TypeSpec &operator=(const TypeSpec &other);
    TypeSpec &operator=(TypeSpec &&other) noexcept;
    ~TypeSpec() = default;

    std::string class_name() const override {
        return "type-spec";
    }
    std::string type_name_obj() const override {
        return class_name();
    }

    bool is_class_name(const std::string &name) const override {
        return name == TypeSpec::type_name_obj() || NativeObject::is_class_name(name);
    }
    // Comparison
    bool operator==(const TypeSpec &other) const;
    bool operator!=(const TypeSpec &other) const;

    // Method compatibility checking
    bool is_compatible_child_method(const TypeSpec &implementation, const std::string &child_type,
                                    int *bad_arg_idx_out = nullptr) const;

    // Printing
    std::string print() const override;
    Object      inspect(SymbolTable* st) const override;

    // Argument management
    void add_arg(const TypeSpec &ts);
    void add_arg(TypeSpec &&ts);

    // Tag management
    void add_new_tag(const std::string &tag_name, const std::string &tag_value);
    std::optional<std::string> try_get_tag(const std::string &tag_name) const;
    const std::string         &get_tag(const std::string &tag_name) const;
    void modify_tag(const std::string &tag_name, const std::string &tag_value);
    void add_or_modify_tag(const std::string &tag_name, const std::string &tag_value);
    void delete_tag(const std::string &tag_name);
    int  get_tags_count() const {
        return m_tags.size();
    }
    const std::vector<TypeTag> &get_tags() const {
        return m_tags;
    }
    bool has_tag(const std::string &tag_name) const {
        return try_get_tag(tag_name).has_value();
    }

    // Type substitution for method calls
    TypeSpec substitute_for_method_call(const std::string &method_type) const;

    // HeapObjects
    const std::string &base_type() const {
        return m_type;
    }

    Object to_sexpr_typspec() const;
    Object to_sexpr_type_names() const;
    Object to_sexpr_type_objects() const;
    void   append_to_sexpr(ListBuilder &builder, int mode) const;

    bool                        has_single_arg() const;
    const TypeSpec             &get_single_arg() const;
    TypeSpec                   &get_single_arg();
    size_t                      get_args_count() const;
    const TypeSpec             &get_arg(int idx) const;
    TypeSpec                   &get_arg(int idx);
    const TypeSpec             &last_arg() const;
    TypeSpec                   &last_arg();
    bool                        empty() const;
    const std::vector<TypeTag> &tags() const {
        return m_tags;
    }
    std::vector<TypeTag> &tags() {
        return m_tags;
    }

    Object get_at(const Object &key) override;
    Type  *get() const;

  private:
    std::string                            m_type;
    std::unique_ptr<std::vector<TypeSpec>> m_arguments;
    std::vector<TypeTag>                   m_tags;
};

// ============================================================================
// Utility Functions
// ============================================================================

// extern TypeSpec coerce_to_reg_type(const TypeSpec& in);

// ============================================================================
// Common TypeSpec Constants
// ============================================================================

namespace typespec {
TypeSpec object();
TypeSpec int32();
TypeSpec int64();
TypeSpec float_();
TypeSpec string();
TypeSpec symbol();
TypeSpec function();
TypeSpec pointer(const TypeSpec &element);
TypeSpec inline_array(const TypeSpec &element);
}; // namespace typespec
