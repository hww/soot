#pragma once

#include "ExpressionNode.hpp"
#include <memory>
#include <string>

namespace sootc {

    /// @brief (-> expr field) — read a field at a known byte offset.
    /// @details The field offset is resolved at compile time from the type of
    ///          `expr`. Codegen emits:
    ///              intAddImm  r_addr, r_expr, offset
    ///              load<T>    r_dest, (r_addr)
    ///          where T is the field's load size and signedness.
    class DerefNode : public ExpressionNode {
        std::unique_ptr<ExpressionNode> m_expr;   ///< pointer expression (base)
        std::string                     m_field;  ///< field name (resolved to offset)
        u32                             m_offset; ///< resolved byte offset

    public:
        DerefNode(std::unique_ptr<ExpressionNode> expr, std::string field, u32 offset,
                  Type *field_type);

        const char *node_type() const override { return "DerefNode"; }
        std::string to_string() const override;

        /// @brief Emit code to read the field. The result is placed in a new
        ///        temp register assigned to this node.
        void emit(FunctionNode &fn) override;
    };

} // namespace sootc