#pragma once

#include "Node.hpp"
#include "ExpressionNode.hpp"
#include <vector>

namespace sootc {

class CallNode : public ExpressionNode {
    std::string m_function_name;
    std::vector<std::unique_ptr<ExpressionNode>> m_args;
    Node                                        *m_target = nullptr;

public:
    CallNode(const std::string& name, Type* return_type)
        : ExpressionNode(NodeType::CallNode, return_type), m_function_name(name) {}
    
    void add_argument(std::unique_ptr<ExpressionNode> arg) {
        m_args.push_back(std::move(arg));
    }
    
    void  set_target(Node *t) { m_target = t; }
    Node *target() const { return m_target; }

    void CallNode::emit(FunctionNode &fn) override {
        // 1. Вычислить аргументы (они окажутся во временных регистрах)
        for (auto &arg : m_args) { arg->emit(fn); }

        // 2. Загрузить SID функции через LookupPointer
        u16 func_idx = fn.add_constant(static_cast<u64>(StringId(m_function_name).value),
                                       FunctionNode::ConstKind::STRING);
        u8  func_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, func_reg, func_idx);

        // 3. Переложить аргументы в r24, r25, ... (ARG_REGISTERS_OFFSET)
        for (size_t i = 0; i < m_args.size(); ++i) {
            u8 arg_reg = fn.get_temp_reg(m_args[i].get());
            fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + i, arg_reg, 0);
        }

        // 4. Вызвать
        u8 result_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction(Opcode::Call, result_reg, func_reg, static_cast<u8>(m_args.size()));
        fn.set_temp_reg(this, result_reg);
    }
    
    std::string to_string() const override {
        std::string result = "(call " + m_function_name;
        for (auto& arg : m_args) {
            result += " " + arg->to_string();
        }
        return result + ")";
    }
};

} // namespace sootc