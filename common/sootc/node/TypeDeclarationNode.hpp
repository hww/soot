#pragma once

#include "Node.hpp"
#include "common/type_system/TypeSpec.hpp"

namespace sootc {

    /// @brief Node for a (deftype ...) form.
    /// @details The type itself is registered in TypeSystem during parsing.
    ///          This node is responsible for emitting the *self-describing*
    ///          SsType entry into the binary, so that loaders and inspectors
    ///          can interpret data-instance entries without a C++ header.
    class TypeDeclarationNode : public Node {
        TypeSpec m_type;

    public:
        explicit TypeDeclarationNode(TypeSpec type)
            : Node(NodeType::TypeDeclarationNode), m_type(std::move(type)) {}

        const char     *node_type() const override { return "TypeDeclarationNode"; }
        std::string     to_string() const override { return "(deftype " + m_type.print() + ")"; }
        const TypeSpec &type() const { return m_type; }

        /// @brief Emit an SsType entry (plus its SsField[] payload) for this type.
        /// @details Layout of the emitted element:
        ///            - SsType header (8 slots),
        ///            - SsField[numFields] (4 slots per field),
        ///          followed by the relocation bits: slots 4 and 5 of SsType
        ///          (m_pFields and m_pMethods) are relocated.
        ///
        ///          Only StructureType is supported for now. Enums, value-types,
        ///          and basic types return an empty element (no emission) until
        ///          the format for them is defined.
        ProgramBinaryElement generate(GlobalState &state) override;
    };

} // namespace sootc