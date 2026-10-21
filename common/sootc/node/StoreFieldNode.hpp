#pragma once

#include "ExpressionNode.hpp"
#include "sootc/node/Node.hpp"

#include <memory>
#include <string>

namespace sootc {

    /// @brief (store-field owner offset value)
    ///
    /// @details Runtime store into a field of a named top-level value:
    ///
    ///             r_owner = LookupPointer(SID(owner))
    ///             r_addr  = r_owner + field_offset
    ///             r_val   = value->emit()
    ///             StorePointer r_addr, r_val
    ///
    ///          m_value is any ExpressionNode. It does not matter whether
    ///          it is a NewNode (heap allocation), a VariableNode
    ///          (symbol lookup), a ConstNode, or anything else — the
    ///          emitter handles them uniformly via value->emit().
    ///
    ///          This node exists because a static data declaration can
    ///          have a pointer field whose value is not known at compile
    ///          time (heap object, cross-module reference, ...). In that
    ///          case DataDeclarationNode leaves the slot at 0 and
    ///          NodeBuilder::build_define creates one StoreFieldNode per
    ///          such field, placing it in the module's top-level body.
    ///
    ///          No three-scenario detection here. No distinction between
    ///          "static in this file" and "heap". If the value is not a
    ///          compile-time constant, it is computed at top-level and
    ///          stored.
    class StoreFieldNode : public ExpressionNode {
        std::string                     m_owner_name;   ///< name of the owning value ("m")
        u32                             m_field_offset; ///< offset of the field inside the owner
        std::unique_ptr<ExpressionNode> m_value;        ///< value to store

    public:
        StoreFieldNode(std::string owner_name, u32 field_offset,
                       std::unique_ptr<ExpressionNode> value)
            : ExpressionNode(NodeType::StoreFieldNode), m_owner_name(std::move(owner_name)),
              m_field_offset(field_offset), m_value(std::move(value)) {}

        const char *node_type() const override { return "StoreFieldNode"; }

        std::string to_string() const override {
            return "(store-field " + m_owner_name + " +" + std::to_string(m_field_offset) + " " +
                   (m_value ? m_value->to_string() : "none") + ")";
        }

        void emit(FunctionNode &fn) override;
    };

} // namespace sootc