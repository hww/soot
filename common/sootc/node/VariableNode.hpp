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
            if (m_reg == 0xFF) {
                if (auto *info = fn.lookup_variable(m_name)) {
                    // Local variable or parameter: use its register.
                    m_reg = info->reg();
                } else {
                    // Not a local. Treat as a top-level name (data declaration,
                    // function, or native symbol) and load it by SID at runtime.
                    //
                    // This mirrors what CallNode::emit does for named calls:
                    // put the SID in the constant pool, then LookupPointer.
                    u16 sid_idx = fn.add_constant(static_cast<u64>(StringId(m_name).value),
                                                  FunctionNode::ConstKind::SID);

                    m_reg = fn.alloc_temp_reg(m_type);
                    fn.add_instruction_imm_u16(Opcode::LookupPointer, m_reg, sid_idx);
                }
            }
            fn.set_temp_reg(this, m_reg);
        }

        std::string to_string() const override { return m_name; }
    };

} // namespace sootc