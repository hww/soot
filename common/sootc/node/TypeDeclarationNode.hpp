#pragma once

#include "Node.hpp"
#include "common/type_system/TypeSpec.hpp"

namespace sootc {

    /// @brief Marker node for a (deftype ...) form.
    /// @details The type itself is registered in TypeSystem during parsing.
    ///          Emitting the SsType entry is deferred to FileNode::make_binary,
    ///          which is the only place that sees *all* methods of the type
    ///          (including those defined later via defmethod). This node carries
    ///          no binary output on its own.
    class TypeDeclarationNode : public Node {
        TypeSpec m_type;

    public:
        explicit TypeDeclarationNode(TypeSpec type)
            : Node(NodeType::TypeDeclarationNode), m_type(std::move(type)) {}

        const char     *node_type() const override { return "TypeDeclarationNode"; }
        std::string     to_string() const override { return "(deftype " + m_type.print() + ")"; }
        const TypeSpec &type() const { return m_type; }

        ProgramBinaryElement generate(GlobalState &) override {
            return ProgramBinaryElement(0); // deferred to FileNode::make_binary
        }
    };

} // namespace sootc