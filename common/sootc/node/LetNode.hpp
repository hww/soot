#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sootc {

    // (let ((a 1) (b 2)) body...)
    //
    // Семантика: все bindings вычисляются в порядке перечисления
    // и кладутся в локальные регистры. body — последнее выражение
    // даёт результат.
    class LetNode : public ExpressionNode {
    public:
        struct Binding {
            std::string                     name;
            std::unique_ptr<ExpressionNode> value;
        };

    private:
        std::vector<Binding>            m_bindings;
        std::unique_ptr<ExpressionNode> m_body;

    public:
        LetNode() : ExpressionNode(NodeType::LetNode) {}

        void add_binding(const std::string &name, std::unique_ptr<ExpressionNode> value) {
            m_bindings.push_back({name, std::move(value)});
        }

        void set_body(std::unique_ptr<ExpressionNode> body) { m_body = std::move(body); }

        const std::vector<Binding> &bindings() const { return m_bindings; }
        const ExpressionNode       *body() const { return m_body.get(); }

        void        emit(FunctionNode &fn) override;
        std::string to_string() const override;

    private:
        // Регистры, выделенные под локальные переменные этого let.
        // Заполняется в emit() — нужно, чтобы VariableNode ниже по дереву
        // знал, куда смотреть. Но пока — просто через FunctionNode::get_variable_reg.
    };

} // namespace sootc