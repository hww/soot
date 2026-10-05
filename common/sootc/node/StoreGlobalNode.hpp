#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/Node.hpp"
#include <memory>

namespace sootc {

    // (define name value) на верхнем уровне:
    // вычисляет value и записывает в глобальный символ name.
    class StoreGlobalNode : public ExpressionNode {
        std::string                     m_name;
        std::unique_ptr<ExpressionNode> m_value;

    public:
        StoreGlobalNode(const std::string &name, std::unique_ptr<ExpressionNode> value)
            : ExpressionNode(NodeType::StoreGlobalNode), m_name(name), m_value(std::move(value)) {}

        void emit(FunctionNode &fn) override;

        std::string to_string() const override {
            return "(store-global " + m_name + " " + (m_value ? m_value->to_string() : "none") +
                   ")";
        }
    };

} // namespace sootc