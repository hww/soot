#pragma once

#include "Node.hpp"
#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/StoreFieldNode.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sootc {

    /// @brief Result of (define name (new 'static Type ...)) when one or
    ///        more pointer fields of the declaration cannot be resolved
    ///        at compile time.
    ///
    /// @details A purely-static define produces one DataDeclarationNode
    ///          and nothing else. But if some pointer field's value is
    ///          not a compile-time constant (heap object, cross-module
    ///          reference, anything else), the payload slot for that
    ///          field is left at 0 and a StoreFieldNode is created to
    ///          write the real value at runtime, from the module's
    ///          top-level body.
    ///
    ///          StaticDefineNode carries both:
    ///
    ///            1. The DataDeclarationNode — the compile-time
    ///               declaration. It goes into FileNode::m_children,
    ///               survives REPL input boundaries, and is what
    ///               build_variable finds when resolving the name in
    ///               a later input.
    ///
    ///            2. Zero or more StoreFieldNodes — the runtime
    ///               stores. They go into the top-level function's
    ///               body, one per non-constant pointer field.
    ///
    ///          If there are no StoreFieldNodes, the wrapper is not
    ///          needed: NodeBuilder returns the DataDeclarationNode
    ///          directly and the pipeline is unchanged.
    ///
    ///          This mirrors HeapDefineNode for the heap case, but for
    ///          a declaration whose payload is otherwise static.
    class StaticDefineNode : public Node {
        std::unique_ptr<DataDeclarationNode>         m_decl;
        std::vector<std::unique_ptr<StoreFieldNode>> m_stores;

    public:
        StaticDefineNode(std::unique_ptr<DataDeclarationNode>         decl,
                         std::vector<std::unique_ptr<StoreFieldNode>> stores)
            : Node(NodeType::StaticDefineNode), m_decl(std::move(decl)),
              m_stores(std::move(stores)) {}

        const char *node_type() const override { return "StaticDefineNode"; }

        std::string to_string() const override {
            return "(static-define " + (m_decl ? m_decl->name() : std::string("<null>")) + " +" +
                   std::to_string(m_stores.size()) + " stores)";
        }

        std::unique_ptr<DataDeclarationNode> take_decl() { return std::move(m_decl); }

        std::vector<std::unique_ptr<StoreFieldNode>> take_stores() { return std::move(m_stores); }

        /// @brief Never emitted as code directly. compile_internal
        ///        unpacks it before any codegen runs.
        ProgramBinaryElement generate(DataSegment &) override {
            throw std::runtime_error(
                "StaticDefineNode::generate: this node is a compile-time "
                "wrapper; it must be unpacked by compile_internal, not emitted");
        }

        void for_each_child(const std::function<void(Node &)> &f) const override {
            if (m_decl) f(*m_decl);
            for (auto &s : m_stores)
                if (s) f(*s);
        }
    };

} // namespace sootc