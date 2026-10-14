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
        u8          m_reg = 0xFF; // 0xFF = «регистр ещё не разрешён»

    public:
        VariableNode(const std::string &name, Type *type)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name) {}

        VariableNode(const std::string &name, Type *type, u8 reg)
            : ExpressionNode(NodeType::VariableNode, type), m_name(name), m_reg(reg) {}

        void               set_reg(u8 reg) { m_reg = reg; }
        const std::string &name() const { return m_name; }

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
            }
            fn.set_temp_reg(this, reg);
        }

        std::string to_string() const override { return m_name; }
    };

} // namespace sootc