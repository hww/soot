#pragma once

#include "Node.hpp"
#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/StoreGlobalNode.hpp"

#include <memory>
#include <string>

namespace sootc {

    /// @brief Result of (define name (new 'heap ...)) at top level.
    ///
    /// @details This form is special: it needs to produce TWO nodes that
    ///          go to TWO different places in the compiler's tree.
    ///
    ///            1. A DataDeclarationNode — the compile-time
    ///               declaration. It goes into FileNode::m_children,
    ///               survives REPL input boundaries, and is what
    ///               build_variable finds when resolving the name in
    ///               a later input. Its generate() emits nothing (the
    ///               file slot is a placeholder handled by make_binary).
    ///
    ///            2. A StoreGlobalNode — the runtime action. It goes
    ///               into the top-level function's body, so it runs
    ///               once when the module's top-level executes and
    ///               writes the constructed pointer into the slot.
    ///
    ///          HeapDefineNode exists solely to carry those two nodes
    ///          from NodeBuilder::build_define back to
    ///          Compiler::compile_internal, which is the only place
    ///          that knows about m_children and top_level_body. It is
    ///          never emitted, never generated, and never stored long
    ///          term: compile_internal immediately takes both children
    ///          and discards the wrapper.
    ///
    ///          This avoids the alternatives:
    ///
    ///            * pending_runtime queue on FileNode — mixes the
    ///              module tree with a compiler-internal channel.
    ///
    ///            * shared_ptr / non-owning pointers — changes the
    ///              ownership model across the whole compiler.
    ///
    ///            * a Placement flag on Node — describes the problem
    ///              but does not solve ownership: a node cannot be in
    ///              two unique_ptr lists at once.
    class HeapDefineNode : public Node {
        std::unique_ptr<DataDeclarationNode> m_decl;
        std::unique_ptr<StoreGlobalNode>     m_store;

    public:
        HeapDefineNode(std::unique_ptr<DataDeclarationNode> decl,
                       std::unique_ptr<StoreGlobalNode>     store)
            : Node(NodeType::HeapDefineNode), m_decl(std::move(decl)), m_store(std::move(store)) {}

        const char *node_type() const override { return "HeapDefineNode"; }

        std::string to_string() const override {
            return "(heap-define " + (m_decl ? m_decl->name() : std::string("<null>")) + ")";
        }

        // ---- Take the two children. After both calls, this node is empty. ----

        std::unique_ptr<DataDeclarationNode> take_decl() { return std::move(m_decl); }
        std::unique_ptr<ExpressionNode>      take_store() {
            return std::unique_ptr<ExpressionNode>(m_store.release());
        }

        /// @brief Not emittable. compile_internal takes the children and
        ///        drops the wrapper before any codegen step runs.
        ProgramBinaryElement generate(StringsTable &) override {
            throw std::runtime_error(
                "HeapDefineNode::generate: this node is a compile-time wrapper; "
                "it must be unpacked by compile_internal, not emitted");
        }
    };

} // namespace sootc