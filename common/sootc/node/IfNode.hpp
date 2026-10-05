#pragma once

#include "ControlNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>

namespace sootc {

class IfNode : public ControlNode {
    std::unique_ptr<ExpressionNode> m_condition;
    std::unique_ptr<ExpressionNode> m_then_branch;
    std::unique_ptr<ExpressionNode> m_else_branch;
    
public:
    IfNode(std::unique_ptr<ExpressionNode> cond,
           std::unique_ptr<ExpressionNode> then_branch,
           std::unique_ptr<ExpressionNode> else_branch = nullptr)
        : ControlNode(NodeType::IfNode)
        , m_condition(std::move(cond))
        , m_then_branch(std::move(then_branch))
        , m_else_branch(std::move(else_branch)) {}
    
   void emit(FunctionNode &fn) override {
        // 1. Условие
        m_condition->emit(fn);
        u8 cond_reg = fn.get_temp_reg(m_condition.get());

        // 2. Метки
        std::string else_label = fn.create_unique_label("else");
        std::string end_label = fn.create_unique_label("endif");

        // 3. Условный переход: если cond == 0 → goto else
        fn.add_instruction(Opcode::BranchIfNot, cond_reg, 0, 0);
        fn.add_branch_reference(else_label);

        // 4. Then-ветка
        m_then_branch->emit(fn);
        u8 then_reg = fn.get_temp_reg(m_then_branch.get());

        // 5. Безусловный переход в конец if
        //    (это ОБЯЗАТЕЛЬНО, иначе then провалится в else)
        fn.add_instruction(Opcode::Branch, 0, 0, 0);
        fn.add_branch_reference(end_label);

        // 6. Else-ветка
        fn.add_label(else_label);
        u8 result_reg = then_reg;
        if (m_else_branch) {
            m_else_branch->emit(fn);
            u8 else_reg = fn.get_temp_reg(m_else_branch.get());

            // Копируем результат else в общий регистр результата
            if (else_reg != result_reg) {
                fn.add_instruction(Opcode::Move, result_reg, else_reg, 0);
            }
        }

        // 7. Конец
        fn.add_label(end_label);

        // 8. Регистрируем результат if как temp_reg
        fn.set_temp_reg(this, result_reg);
    }
    
    std::string to_string() const override {
        std::string result = "(if " + m_condition->to_string() + " " + m_then_branch->to_string();
        if (m_else_branch) {
            result += " " + m_else_branch->to_string();
        }
        return result + ")";
    }
};

} // namespace sootc