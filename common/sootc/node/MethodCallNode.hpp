#pragma once

#include "ExpressionNode.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sootc {

    /// @brief (-> obj method arg...) — call a method by full name.
    /// @details At compile time the method is resolved to a full name
    ///          "<type>-<method>", which is registered in Globals by defmethod.
    ///          Codegen emits:
    ///              lookupPointer r_fn, ST[<type>-<method>]
    ///              move          r24+, receiver, args...
    ///              call          r_ret, r_fn, argc
    class MethodCallNode : public ExpressionNode {
        std::unique_ptr<ExpressionNode>              m_obj;       ///< receiver (pointer)
        std::string                                  m_full_name; ///< "<type>-<method>"
        std::vector<std::unique_ptr<ExpressionNode>> m_args;

    public:
        MethodCallNode(std::unique_ptr<ExpressionNode> obj, std::string full_name,
                       std::vector<std::unique_ptr<ExpressionNode>> args, Type *return_type);

        const char *node_type() const override { return "MethodCallNode"; }
        std::string to_string() const override;

        void emit(FunctionNode &fn) override;
    };

} // namespace sootc