#pragma once

#include "Node.hpp"
#include "sootc/node/NewNode.hpp"
#include <memory>
#include <string>

namespace sootc {

    /// @brief (define name (new Type :field value ...))
    /// @details Top-level data declaration. Creates a data instance
    ///          and binds it to a name. Currently no serialization.
    class DataDeclarationNode : public Node {
        std::string              m_name;
        std::unique_ptr<NewNode> m_instance;
        bool                     m_exported;

    public:
        DataDeclarationNode(std::string name, std::unique_ptr<NewNode> instance,
                            bool exported = false)
            : Node(NodeType::DataDeclarationNode), m_name(std::move(name)),
              m_instance(std::move(instance)), m_exported(exported) {}

        const char *node_type() const override { return "DataDeclarationNode"; }

        std::string to_string() const override {
            return (m_exported ? "(define-export " : "(define ") + m_name + " " +
                   (m_instance ? m_instance->to_string() : "none") + ")";
        }

        const std::string &name() const { return m_name; }
        const NewNode     *instance() const { return m_instance.get(); }
        bool               is_exported() const { return m_exported; }

        ProgramBinaryElement generate(GlobalState &) override;
    };

} // namespace sootc