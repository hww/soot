#pragma once

#include "sootc/node/ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/FunctionNode.hpp"
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

        void remove_where(const std::function<bool(const ExpressionNode *)> &pred) {
            m_exprs.erase(std::remove_if(m_exprs.begin(), m_exprs.end(),
                                         [&](const std::unique_ptr<ExpressionNode> &e) {
                                             return pred(e.get());
                                         }),
                          m_exprs.end());
        }

        size_t size() const { return m_exprs.size(); }

        bool empty() const { return m_children.empty(); }

        void emit(FunctionNode &func) override {
            if (m_exprs.empty()) {
                // Empty sequence: no value. Leave the caller to handle it.
                return;
            }

            // Emit each expression. Only the last one carries the sequence's value.
            for (size_t i = 0; i < m_exprs.size(); ++i) { m_exprs[i]->emit(func); }

            // The sequence's value is the value of the last expression. Forward the
            // temp register so that the caller (e.g. FunctionNode::emit_body) can use
            // it for the implicit return.
            const Node *last = m_exprs.back().get();
            if (func.has_temp_reg(last)) { func.set_temp_reg(this, func.get_temp_reg(last)); }
        }

        std::string to_string() const override {
            std::string result = "(sequence";
            for (auto &e : m_exprs) { result += " " + e->to_string(); }
            return result + ")";
        }
    };

} // namespace sootc