#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>
#include <vector>

namespace sootc {

    // Последовательность выражений: вычисляет их по очереди,
    // результат — последнее.
    // Используется для тела top-level функции и (позже) для begin/let.
    class SequenceNode : public ExpressionNode {
        std::vector<std::unique_ptr<ExpressionNode>> m_exprs;

    public:
        SequenceNode() : ExpressionNode(NodeType::SequenceNode) {}

        void add(std::unique_ptr<ExpressionNode> expr) { m_exprs.push_back(std::move(expr)); }

        size_t size() const { return m_exprs.size(); }

        void emit(FunctionNode &fn) override {
            for (auto &e : m_exprs) { e->emit(fn); }
            if (!m_exprs.empty()) {
                // Прокинуть temp_reg последнего выражения
                u8 last_reg = fn.get_temp_reg(m_exprs.back().get());
                fn.set_temp_reg(this, last_reg);
                m_type = m_exprs.back()->get_type();
            }
        }

        std::string to_string() const override {
            std::string result = "(sequence";
            for (auto &e : m_exprs) { result += " " + e->to_string(); }
            return result + ")";
        }
    };

} // namespace sootc