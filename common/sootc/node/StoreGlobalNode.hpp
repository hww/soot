#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/NewNode.hpp"
#include <memory>
#include <string>

namespace sootc {

    class NewNode;

    /// @brief (define name (new 'heap ...)) — the runtime half.
    ///
    /// @details Emits:
    ///            r_val  = <m_new_node->emit()>
    ///            r_slot = LookupPointer SID(name)
    ///            StorePointer r_slot, r_val
    ///
    ///          Ownership: m_new_node is a shared_ptr, shared with
    ///          the DataDeclarationNode that lives in FileNode::
    ///          m_children. The shared_ptr is what makes REPL
    ///          redefinition safe: destroying the old
    ///          DataDeclarationNode does not free the NewNode as
    ///          long as any StoreGlobalNode still references it.
    class StoreGlobalNode : public ExpressionNode {
        std::string              m_name;
        std::shared_ptr<NewNode> m_new_node;

    public:
        StoreGlobalNode(const std::string &name, std::shared_ptr<NewNode> new_node)
            : ExpressionNode(NodeType::StoreGlobalNode), m_name(name),
              m_new_node(std::move(new_node)) {}

        const std::string &name() const { return m_name; }
        NewNode           *new_node() const { return m_new_node.get(); }

        void emit(FunctionNode &fn) override;

        std::string to_string() const override { return "(store-global " + m_name + ")"; }

        void for_each_child(const std::function<void(Node &)> &f) const override {
            if (m_new_node) f(*m_new_node);
        }
    };

} // namespace sootc