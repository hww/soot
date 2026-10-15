#pragma once

#include "CommonTypes.hpp"
#include "ExpressionNode.hpp"
#include "FunctionNode.hpp"
#include "sootc/node/Node.hpp"
#include <stdexcept>
#include <string>

namespace sootc {

    /// @brief Reference to a named value.
    ///
    /// @details Three cases for emit():
    ///
    ///            1. Local variable or parameter — reg from the frame.
    ///
    ///            2. Top-level static data — LookupPointer gives the
    ///               address of the slot inside the owning module's
    ///               bytes. For a scalar or an inline struct, that
    ///               address IS the value's address.
    ///
    ///            3. Top-level heap data — the slot holds a pointer to
    ///               the heap object. We dereference once with
    ///               LoadPointer, so the rest of the expression sees
    ///               the heap object, not the 8-byte slot.
    ///
    ///          m_elem_type is set when the variable names an inline
    ///          array (declared via (define name (new 'static 'inline-array
    ///          T N ...)) or (new 'heap 'inline-array T N ...)). The
    ///          AtNode uses it to know what element type to load. For
    ///          non-array variables it is nullptr.
    class VariableNode : public ExpressionNode {
        std::string m_name;
        u8          m_reg = 0xFF;
        bool        m_is_heap = false;
        Type       *m_elem_type = nullptr; // non-null if this names an inline array

    public:
        VariableNode(const std::string &name, Type *type, bool is_heap = false,
                     Type *elem_type = nullptr)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_is_heap(is_heap),
              m_elem_type(elem_type) {}

        VariableNode(const std::string &name, Type *type, u8 reg)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_reg(reg) {}

        void               set_reg(u8 reg) { m_reg = reg; }
        const std::string &name() const { return m_name; }

        /// @brief Non-null if this variable names an inline array.
        Type *elem_type() const { return m_elem_type; }

        void emit(FunctionNode &fn) override {
            // Do NOT cache m_reg across emits. A single VariableNode may be
            // emitted multiple times if its enclosing AST is reused (which
            // happens when FileNode is reused across REPL inputs). The
            // register allocation for each emit is independent.
            u8 reg;
            if (auto *info = fn.lookup_variable(m_name)) {
                reg = info->reg();
            } else {
                // Top-level name: emit LookupPointer by SID.
                u16 sid_idx = fn.add_constant(static_cast<u64>(StringId(m_name).value),
                                              FunctionNode::ConstKind::SID);
                reg = fn.alloc_temp_reg(m_type);
                fn.add_instruction_imm_u16(Opcode::LookupPointer, reg, sid_idx);

                // For a heap-declared top-level name, the slot holds a
                // pointer to the heap object, not the object itself.
                // Dereference the slot once, so that subsequent field
                // accesses (Load/Store on reg + offset) operate on the
                // heap object, not on the 8-byte slot.
                if (m_is_heap) {
                    u8 deref_reg = fn.alloc_temp_reg(m_type);
                    fn.add_instruction(Opcode::LoadPointer, deref_reg, reg, 0);
                    reg = deref_reg;
                }
            }
            fn.set_temp_reg(this, reg);
        }

        std::string to_string() const override { return m_name; }
    };

} // namespace sootc