#pragma once

#include "ExpressionNode.hpp"
#include "common/type_system/TypeSpec.hpp"
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

    public:
        explicit NewNode(TypeSpec type)
            : ExpressionNode(NodeType::NewNode), m_type(std::move(type)) {}

        // ---- allocation ----
        const std::string &allocation() const { return m_allocation; }
        void               set_allocation(std::string alloc) { m_allocation = std::move(alloc); }
        bool               is_static() const { return m_allocation == "static"; }

        // ---- static: fields ----
        void add_field(const std::string &name, std::unique_ptr<ExpressionNode> value) {
            m_fields.push_back({name, std::move(value)});
        }

        // ---- non-static: constructor arguments ----
        void add_argument(std::unique_ptr<ExpressionNode> arg) { m_args.push_back(std::move(arg)); }

        const TypeSpec                                     &type() const { return m_type; }
        const std::vector<FieldInit>                       &fields() const { return m_fields; }
        const std::vector<std::unique_ptr<ExpressionNode>> &arguments() const { return m_args; }

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
        ProgramBinaryElement generate(StringsTable &state) override;
    };

} // namespace sootc