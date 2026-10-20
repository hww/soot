#pragma once

#include "Node.hpp"
#include "type_system/TypeSpec.hpp"

namespace sootc {

    /// @brief Marker node for a (deftype ...) form that carries states.
    ///
    /// @details The type itself is registered in TypeSystem during parsing
    ///          (by parse_deftype). This node carries no payload of its own:
    ///          FileNode::make_binary reads the Type via TypeSystem and emits
    ///          the StateScript entry from it.
    ///
    ///          The handler bodies and track steps are compiled separately,
    ///          by Compiler::compile_state_script_bodies, into FunctionNode
    ///          children of the FileNode. make_binary assigns them file
    ///          offsets, and build_state_script links them into the
    ///          SsOnBlock / SsTrack / SsLambda arrays.
    class StateScriptNode : public Node {
        TypeSpec m_type;

    public:
        explicit StateScriptNode(TypeSpec type)
            : Node(NodeType::StateScriptNode), m_type(std::move(type)) {}

        const char *node_type() const override { return "StateScriptNode"; }
        std::string to_string() const override { return "(state-script " + m_type.print() + ")"; }
        const TypeSpec &type() const { return m_type; }

        ProgramBinaryElement generate(DataSegment &) override {
            // No payload of its own. make_binary reads the Type and emits
            // the StateScript entry itself.
            return ProgramBinaryElement(0);
        }
    };

} // namespace sootc