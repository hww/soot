#pragma once

#include "Node.hpp"
#include "file/ProgramBinaryElement.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sootc {

    /// @brief Root of the compiler's global name table.
    ///
    /// @details GlobalNode maps a name (e.g. "foo", "m", "inner") to the
    ///          Node that declared it. In a REPL session, build_define
    ///          binds a new name here so that later inputs can resolve
    ///          it. The binding points at a Node that lives in some
    ///          FileNode's m_children — the Node is not owned by this
    ///          map.
    ///
    ///          Transaction support: the map can be snapshotted before
    ///          an input and rolled back on failure. Without this,
    ///          a build_define that binds a name and then throws
    ///          would leave a dangling pointer here: the Node would be
    ///          destroyed by FileNode::rollback, but the name would
    ///          still be in this map, and the next input's lookup
    ///          would return a dangling Node*.
    class GlobalNode : public Node {
        std::unordered_map<std::string, Node *> m_symbols;
        std::vector<Node *>                     m_ordered_symbols;

    public:
        GlobalNode() : Node(NodeType::GlobalNode) {}

        const char *node_type() const override { return "GlobalNode"; }

        std::string to_string() const override {
            return "GlobalNode(symbols=" + std::to_string(m_symbols.size()) + ")";
        }

        // ====================================================================
        // Symbol management
        // ====================================================================

        Node *lookup(const std::string &name) override {
            auto it = m_symbols.find(name);
            if (it != m_symbols.end()) return it->second;
            return nullptr;
        }

        void bind(const std::string &name, Node *node) {
            if (m_symbols.find(name) == m_symbols.end()) { m_ordered_symbols.push_back(node); }
            m_symbols[name] = node;
        }

        /// @brief Снять привязку по имени.
        /// @details Сам Node НЕ удаляется — им владеет FileNode.
        ///          Убираем и из m_symbols, и из m_ordered_symbols,
        ///          чтобы не оставлять висячих указателей.
        void remove(const std::string &name) {
            auto it = m_symbols.find(name);
            if (it == m_symbols.end()) return;
            Node *dead = it->second;
            m_symbols.erase(it);

            m_ordered_symbols.erase(
                std::remove(m_ordered_symbols.begin(), m_ordered_symbols.end(), dead),
                m_ordered_symbols.end());
        }

        const std::unordered_map<std::string, Node *> &symbols() const { return m_symbols; }

        ProgramBinaryElement generate(DataSegment &state) override {
            (void)state;
            return ProgramBinaryElement{0};
        }

        // ====================================================================
        // Transaction support
        // ====================================================================
        //
        // In REPL mode the same GlobalNode is reused across inputs. If
        // a compilation throws after build_define has bound a name
        // here, the binding points to a Node that is about to be
        // destroyed (the Node lives in FileNode::m_children, and
        // rollback frees it). The next input would then find a
        // dangling pointer in this map.
        //
        // The Compiler takes a snapshot before each input and calls
        // rollback on failure.
        //
        // Cost: copying both maps on every REPL input. For a human
        // typing at a REPL this is negligible. For batch compilation
        // it is not used: each file gets its own GlobalNode (or none
        // at all), which is discarded on error.

        struct Snapshot {
            std::unordered_map<std::string, Node *> symbols;
            std::vector<Node *>                     ordered_symbols;
            bool                                    valid = false;
        };

        [[nodiscard]] Snapshot take_snapshot() const {
            Snapshot s;
            s.symbols = m_symbols;
            s.ordered_symbols = m_ordered_symbols;
            s.valid = true;
            return s;
        }

        void rollback(Snapshot &&s) {
            if (!s.valid) return;
            m_symbols = std::move(s.symbols);
            m_ordered_symbols = std::move(s.ordered_symbols);
            s.valid = false;
        }
    };

} // namespace sootc