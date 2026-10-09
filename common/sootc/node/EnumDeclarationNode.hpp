#pragma once

#include "Node.hpp"

namespace sootc {

    /// @brief Marker node for a (defenum ...) form.
    class EnumDeclarationNode : public Node {
        std::string m_name;

    public:
        explicit EnumDeclarationNode(std::string name)
            : Node(NodeType::EnumDeclarationNode), m_name(std::move(name)) {}

        const char        *node_type() const override { return "EnumDeclarationNode"; }
        std::string        to_string() const override { return "(defenum " + m_name + ")"; }
        const std::string &name() const { return m_name; }

        ProgramBinaryElement generate(StringsTable &) override { return ProgramBinaryElement(0); }
    };

} // namespace sootc