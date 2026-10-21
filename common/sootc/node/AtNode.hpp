#pragma once

#include "sootc/node/ExpressionNode.hpp"
#include "sootc/node/Node.hpp"

#include <memory>
#include <string>

namespace sootc {

    /// @brief (at base index) — read the i-th element of an inline array.
    ///
    /// @details base must evaluate to a pointer to the first element
    ///          (the array's payload). index is an integer expression.
    ///          The element type comes from the array's declaration; it
    ///          is passed in at construction time by NodeBuilder.
    ///
    ///          emit():
    ///              r_base = base.emit()
    ///              r_idx  = index.emit()          ; as i64
    ///              r_addr = r_base + r_idx * elem_size
    ///              r_val  = Load* by element type
    ///
    ///          For scalar element types, Load* is the appropriate
    ///          LoadI8/I16/I32/I64/LoadFloat. For structure element
    ///          types, emit() returns the element's address (r_addr);
    ///          the caller is expected to DerefNode into it.
    ///
    ///          Bounds are NOT checked at runtime. That is a later step.
    class AtNode : public ExpressionNode {
        std::unique_ptr<ExpressionNode> m_base;
        std::unique_ptr<ExpressionNode> m_index;
        Type                           *m_elem_type;

    public:
        AtNode(std::unique_ptr<ExpressionNode> base, std::unique_ptr<ExpressionNode> index,
               Type *elem_type)
            : ExpressionNode(NodeType::AtNode, elem_type), m_base(std::move(base)),
              m_index(std::move(index)), m_elem_type(elem_type) {}

        const char *node_type() const override { return "AtNode"; }

        Type       *elem_type() const { return m_elem_type; }

        /// @brief Emit code that computes the address of the i-th element.
        /// @details Same as emit(), but without the final load. The
        ///          resulting address register is assigned to this node
        ///          so SetNode can write through it. Used for
        ///          (set! (at arr i) value).
        ///
        /// @return the register holding the element's address.
        u8          emit_lvalue(FunctionNode &fn);

        std::string to_string() const override {
            return "(at " + m_base->to_string() + " " + m_index->to_string() + ")";
        }

        void emit(FunctionNode &fn) override;
    };

} // namespace sootc