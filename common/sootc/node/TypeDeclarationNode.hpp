#pragma once

#include "Node.hpp"
#include "common/type_system/TypeSpec.hpp"

namespace sootc {

    /// @brief Marker node for a (deftype ...) form.
    /// @details The type is registered in TypeSystem during parsing.
    ///          This node carries no code — it's a compile-time-only declaration.
    class TypeDeclarationNode : public Node {
        TypeSpec m_type;

    public:
        explicit TypeDeclarationNode(TypeSpec type)
            : Node(NodeType::TypeDeclarationNode), m_type(std::move(type)) {}

        const char     *node_type() const override { return "TypeDeclarationNode"; }
        std::string     to_string() const override { return "(deftype " + m_type.print() + ")"; }
        const TypeSpec &type() const { return m_type; }

        ProgramBinaryElement generate(GlobalState &) override {
            return ProgramBinaryElement(0); // no binary output
        }
    };

} // namespace sootc