#pragma once

#include "ControlNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>

namespace sootc {

class WhileNode : public ControlNode {
    std::unique_ptr<ExpressionNode> m_condition;
    std::unique_ptr<ExpressionNode> m_body;
    
public:
    WhileNode(std::unique_ptr<ExpressionNode> cond,
              std::unique_ptr<ExpressionNode> body)
        : ControlNode(NodeType::WhileNode)
        , m_condition(std::move(cond))
        , m_body(std::move(body)) {}
    
    void emit(FunctionNode& fn) override {
        std::string start_label = fn.create_unique_label("while_start");
        std::string end_label = fn.create_unique_label("while_end");
        
        fn.add_label(start_label);
        
        // Условие само генерирует результат в регистр
        m_condition->emit(fn);
        u8 cond_reg = fn.get_temp_reg(m_condition.get());
        
        // Выход если условие ложно
        fn.add_instruction(Opcode::BranchIfNot, cond_reg, 0, 0);
        fn.add_branch_reference(end_label);
        
        // Тело цикла
        m_body->emit(fn);
        
        fn.add_branch_reference(start_label);
        fn.add_label(end_label);
    }
    
    std::string to_string() const override {
        return "(while " + m_condition->to_string() + " " + m_body->to_string() + ")";
    }
};

} // namespace sootc