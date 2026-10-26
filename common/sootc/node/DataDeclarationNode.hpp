#pragma once

#include "Node.hpp"
#include "sootc/node/NewNode.hpp"
#include <memory>
#include <string>

namespace sootc {

    /// @brief (define name (new [allocation] Type ...))
    ///
    /// @details Two modes, distinguished by m_heap:
    ///
    ///            static (m_heap = false) — the payload is baked into
    ///              the file. generate() writes the fields at compile
    ///              time. The slot in the file IS the value.
    ///
    ///            heap (m_heap = true) — the payload is a placeholder
    ///              (8 zero bytes). generate() emits nothing. The
    ///              pointer is written at runtime by a StoreGlobalNode
    ///              that lives in the top-level body, via StorePointer
    ///              into the file slot.
    ///
    ///          Ownership: m_instance is shared_ptr, not unique_ptr,
    ///          because in heap mode the same NewNode is also
    ///          referenced by a StoreGlobalNode that lives in the
    ///          top-level body. On REPL redefinition the old
    ///          DataDeclarationNode is destroyed, but the old
    ///          StoreGlobalNode may still hold the shared_ptr; the
    ///          NewNode must stay alive until BOTH are gone. With
    ///          unique_ptr the first destructor would free it and the
    ///          second would dangle.
    class DataDeclarationNode : public Node {
        std::string              m_name;
        std::shared_ptr<NewNode> m_instance; ///< shared with StoreGlobalNode in heap mode
        bool                     m_heap;

    public:
        DataDeclarationNode(std::string name, std::shared_ptr<NewNode> instance,
                            bool heap = false)
            : Node(NodeType::DataDeclarationNode), m_name(std::move(name)),
              m_instance(std::move(instance)), m_heap(heap) {
            adopt(m_instance);
        }

        const char *node_type() const override { return "DataDeclarationNode"; }

        std::string to_string() const override {
            return "(define " + m_name + " " + (m_instance ? m_instance->to_string() : "none") + ")";
        }

        const std::string &name() const { return m_name; }
        const NewNode     *instance() const { return m_instance.get(); }
        bool               is_heap() const { return m_heap; }

        /// @brief Return the shared NewNode.
        /// @details Used by build_define to hand the same instance to
        ///          StoreGlobalNode. The shared_ptr is what makes REPL
        ///          redefinition safe.
        std::shared_ptr<NewNode> instance_shared() const { return m_instance; }

        ProgramBinaryElement generate(DataSegment &) override;

        void for_each_child(const std::function<void(Node &)> &f) const override {
            if (m_instance) f(*m_instance);
        }
    };

} // namespace sootc