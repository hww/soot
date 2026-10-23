#pragma once

#include "ExpressionNode.hpp"
#include "type_system/TypeSpec.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sootc {

    /// @brief (new [allocation] Type [:field value | arg] ...)
    /// @details Two branches, selected by m_allocation:
    ///
    ///          'static'  — field initializer. Keyword-args are field names.
    ///                       The value is baked into the binary at compile
    ///                       time. Only valid at top level.
    ///          'global' / 'heap' / 'stack'
    ///                    — constructor call. The compiler looks up the
    ///                       type's `new` method and passes the positional
    ///                       arguments to it. No keyword-args allowed.
    ///
    ///          If allocation is omitted, it defaults to 'static'.
    class NewNode : public ExpressionNode {
    public:
        struct FieldInit {
            std::string                     name;
            std::unique_ptr<ExpressionNode> value;
        };

    private:
        std::string                                  m_allocation = "static";
        TypeSpec                                     m_type;
        std::vector<FieldInit>                       m_fields; ///< only for 'static'
        std::vector<std::unique_ptr<ExpressionNode>> m_args;   ///< only for non-static
        // ---- Inline-array mode ----
        //
        // When the second token after the allocation is 'inline-array,
        // the form is:
        //
        //     (new 'static 'inline-array T N e0 e1 ... eK)
        //
        // and this NewNode represents an anonymous inline array, not a
        // structure. In that case:
        //   * m_type is set to T (element type), not to a structure,
        //   * m_elem_type_name repeats T as a string,
        //   * m_array_size is N (the declared capacity),
        //   * m_elements holds the K+1 init expressions (K+1 <= N).
        //
        // Both 'static and 'heap are allowed. 'static writes the
        // elements inline into the parent's payload; 'heap emits a
        // runtime allocation of sizeof(T) * N bytes with alignof(T) * N.
        bool                                         m_is_inline_array = false;
        std::string                                  m_elem_type_name;
        u32                                          m_array_size = 0;
        std::vector<std::unique_ptr<ExpressionNode>> m_elements;

    public:
        explicit NewNode(TypeSpec type)
            : ExpressionNode(NodeType::NewNode), m_type(std::move(type)) {}

        // ---- allocation ----
        const std::string &allocation() const { return m_allocation; }
        void               set_allocation(std::string alloc) { m_allocation = std::move(alloc); }
        bool               is_static() const { return m_allocation == "static"; }
        bool               is_gloabal() const {
            return m_allocation == "global" || m_allocation == "heap" || m_allocation == "stack";
        }

        // ---- static: fields ----
        void add_field(const std::string &name, std::unique_ptr<ExpressionNode> value) {
            adopt(value);
            m_fields.push_back({name, std::move(value)});
        }

        // ---- non-static: constructor arguments ----
        void add_argument(std::unique_ptr<ExpressionNode> arg) {
            adopt(arg);
            m_args.push_back(std::move(arg));
        }

        /// @brief Append one element init expression.
        void add_element(std::unique_ptr<ExpressionNode> e) {
            adopt(e);
            m_elements.push_back(std::move(e));
        }

        const TypeSpec                                     &type() const { return m_type; }
        const std::vector<FieldInit>                       &fields() const { return m_fields; }
        const std::vector<std::unique_ptr<ExpressionNode>> &arguments() const { return m_args; }

        // ---- Inline-array accessors ----

        bool               is_inline_array() const noexcept { return m_is_inline_array; }
        const std::string &elem_type_name() const noexcept { return m_elem_type_name; }
        u32                array_size() const noexcept { return m_array_size; }

        const std::vector<std::unique_ptr<ExpressionNode>> &elements() const noexcept {
            return m_elements;
        }

        /// @brief Configure this node as an inline-array.
        /// @details Called by NodeBuilder::build_new when it sees
        ///          'inline-array as the second token.
        void set_inline_array(std::string elem_type, u32 size) {
            m_is_inline_array = true;
            m_elem_type_name = std::move(elem_type);
            m_array_size = size;
        }

        /// @brief Emit code for a constructor call ('global' / 'heap' / 'stack').
        /// @details
        ///          1. Look up "<type>-new" in the symbol table.
        ///          2. Build the full argument list: allocation SID,
        ///             type-to-make SID, then the user's positional arguments.
        ///          3. Move them into r24+ and emit Call.
        ///          The result (a pointer) is stored in a temp register for
        ///          this node.
        ///
        ///          Calling emit on a 'static' NewNode is a compilation error:
        ///          static initialization is only valid at top level.
        void emit(FunctionNode &fn) override;

        std::string to_string() const override;

        /// @brief Static initialization: serialize into a data-instance entry.
        /// @details Only valid for m_allocation == "static". Returns an empty
        ///          element otherwise (the top-level compiler reports an error).
        ProgramBinaryElement generate(DataSegment &state) override;

        void for_each_child(const std::function<void(Node &)> &f) const override {
            for (auto &fi : m_fields)
                if (fi.value) f(*fi.value);
            for (auto &a : m_args)
                if (a) f(*a);
            for (auto &e : m_elements)
                if (e) f(*e);
        }
    private:
        void emit_field_init_stores(FunctionNode &fn, u8 base_ptr_reg, StructureType *st);
    };

} // namespace sootc