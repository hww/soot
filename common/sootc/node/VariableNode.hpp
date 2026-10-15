#pragma once

#include "CommonTypes.hpp"
#include "ExpressionNode.hpp"
#include "FunctionNode.hpp"
#include "sootc/node/Node.hpp"
#include <stdexcept>
#include <string>

namespace sootc {

    class VariableNode : public ExpressionNode {
        std::string m_name;
        u8          m_reg = 0xFF;
        bool        m_is_heap = false; // ← NEW

    public:
        VariableNode(const std::string &name, Type *type, bool is_heap = false)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_is_heap(is_heap) {}

        VariableNode(const std::string &name, Type *type, u8 reg)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_reg(reg) {}

        void emit(FunctionNode &fn) override {
            u8 reg;
            if (auto *info = fn.lookup_variable(m_name)) {
                reg = info->reg();
            } else {
                // Top-level name.
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