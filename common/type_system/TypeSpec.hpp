#pragma once

/*!
 * @file TypeSpec.h
 * A GOAL TypeSpec is a reference to a type or compound type.
 *
 * NOTE: This is a pure C++ class. It is NOT a Lisp Object and does not
 * inherit from NativeObject/HeapObject. All Lisp-facing functionality
 * (get_at, inspect-as-sexpr, serialization into Archive) has been removed.
 */

#include <memory>
#include <optional>
#include <string>
#include <vector>

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

class TypeSpec : public std::enable_shared_from_this<TypeSpec>{
  public:
    // ---- Constructors / Rule of Five ----
    TypeSpec() = default;
    explicit TypeSpec(std::string type);
    TypeSpec(std::string type, std::vector<TypeSpec> arguments);

    TypeSpec(const TypeSpec &other);
    TypeSpec(TypeSpec &&other) noexcept;
    TypeSpec &operator=(const TypeSpec &other);
    TypeSpec &operator=(TypeSpec &&other) noexcept;
    ~TypeSpec() = default;

    // ---- Comparison ----
    bool operator==(const TypeSpec &other) const;
    bool operator!=(const TypeSpec &other) const;

    // ---- Base type access ----
    const std::string &base_type() const {
        return m_type;
    }

    Type *get() const;

    // ---- Arguments ----
    void add_arg(const TypeSpec &ts);
    void add_arg(TypeSpec &&ts);

    bool            has_single_arg() const;
    const TypeSpec &get_single_arg() const;
    TypeSpec       &get_single_arg();
    size_t          get_args_count() const;
    const TypeSpec &get_arg(int idx) const;
    TypeSpec       &get_arg(int idx);
    const TypeSpec &last_arg() const;
    TypeSpec       &last_arg();
    bool            empty() const;

    // ---- Tags ----
    void                       add_new_tag(const std::string &tag_name, const std::string &tag_value);
    std::optional<std::string> try_get_tag(const std::string &tag_name) const;
    const std::string         &get_tag(const std::string &tag_name) const;
    void                       modify_tag(const std::string &tag_name, const std::string &tag_value);
    void                       add_or_modify_tag(const std::string &tag_name,
                                                 const std::string &tag_value);
    void                       delete_tag(const std::string &tag_name);

    int get_tags_count() const {
        return m_tags.size();
    }
    const std::vector<TypeTag> &get_tags() const {
        return m_tags;
    }
    const std::vector<TypeTag> &tags() const {
        return m_tags;
    }
    std::vector<TypeTag> &tags() {
        return m_tags;
    }
    bool has_tag(const std::string &tag_name) const {
        return try_get_tag(tag_name).has_value();
    }

    // ---- Method compatibility ----
    bool is_compatible_child_method(const TypeSpec &implementation, const std::string &child_type,
                                    int *bad_arg_idx_out = nullptr) const;
    TypeSpec substitute_for_method_call(const std::string &method_type) const;

    // ---- Printing ----
    // Compact, one-line S-expression form: "(function int int)" or "int".
    std::string print() const;

    // Multi-line structural dump for debugging.
    std::string inspect() const;

  private:
    std::string                            m_type;
    std::unique_ptr<std::vector<TypeSpec>> m_arguments;
    std::vector<TypeTag>                   m_tags;
};

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