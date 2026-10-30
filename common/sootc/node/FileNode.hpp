// FileNode.hpp
#pragma once

#include "Node.hpp"
#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/ProgramBinaryElement.hpp"
#include "sootc/libs/DataSegment.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "sootc/node/StateScriptNode.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <unistd.h> // readlink
#elif defined(__APPLE__)
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

namespace sootc {

    class FunctionNode;

    class FileNode : public Node {
        std::string                             m_name;
        std::unordered_map<std::string, Node *> m_symbols;
        std::vector<Node *>                     m_ordered_symbols;
        std::vector<FileNode *>                 m_imports;
        /// @brief Layouts of data-struct entries produced by make_binary.
        /// @details Read by Compiler after generate() to populate the BinaryFile.
        std::vector<DataStructEntry>            m_dataStructs;
        /// @brief Names that need a placeholder entry in the binary.
        /// @details See add_placeholder() for the rationale.
        std::vector<std::string>                m_placeholders;
        /// @brief Module names this file depends on via (require ...).
        /// @details Serialised into a "requires" entry in the .bin as a
        ///          SymbolArray of SIDs. The loader can read it to walk
        ///          the dependency graph without a .dci side file.
        std::vector<std::string>                m_requires;
        /// @brief Объявления файла (функции, данные, типы). Владеет ими FileNode.
        std::vector<std::unique_ptr<Node>>      m_children;
    public:
        explicit FileNode(const std::string &name);

        const char *node_type() const override { return "FileNode"; }
        std::string to_string() const override;

        // ---- Name ----
        const std::string &name() const { return m_name; }

        // ---- Binary generation (Node interface) ----
        ProgramBinaryElement generate(DataSegment &strings_table) override;

        // ---- Symbol table ----
        /// @brief Lookup binding
        Node *lookup(const std::string &name) override;
        /// @brief Add binding
        void  bind(const std::string &name, Node *node);
        /// @brief Remove any child FunctionNode or DataDeclarationNode
        void  remove_named(const std::string &name);
        /// @brief Check binding
        bool  has_local(const std::string &name) const {
            return m_symbols.find(name) != m_symbols.end();
        }

        // ---- Top level ----
        /// @brief The module's top-level function, or nullptr if no
        ///        top-level expression has been added yet.
        FunctionNode *top_level() const { return m_top_level; }

        /// @brief Install the module's top-level function and its body.
        /// @details Called by the compiler on the first top-level
        ///          expression. The FileNode takes ownership of the
        ///          FunctionNode via add_child; the body pointer is
        ///          stored for later appends.
        void set_top_level(FunctionNode *fn) { m_top_level = fn; }

        void remove_top_level() {
            m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                                            [](const std::unique_ptr<Node> &c) {
                                                auto *f = dynamic_cast<FunctionNode *>(c.get());
                                                return f && f->name() == "top-level";
                                            }),
                             m_children.end());
        }

        // ---- Imports ----
        void                           add_import(FileNode *file);
        const std::vector<FileNode *> &imports() const { return m_imports; }

        static void insert_into_reloctable(u8 *reloc_table, u64 &byte_offset, u64 &bit_offset,
                                           u8 bits, u64 num_bits) noexcept;
        
        const std::vector<DataStructEntry> &data_structs() const { return m_dataStructs; }

        // ---- Children ----

        /// @brief Append a child node. Used by NodeBuilder when hoisting
        ///        declarations out of a (begin ...) form.
        void add_child(std::unique_ptr<Node> child) {
            if (!child) return;
            adopt(child);
            m_children.push_back(std::move(child));
        }

        const std::vector<std::unique_ptr<Node>> &children() const { return m_children; }

        void for_each_child(const std::function<void(Node &)> &f) const override {
            for (auto &c : m_children) f(*c);
        }

        /// @brief Take ownership of children and symbols, leaving this FileNode empty.
        /// Used by the REPL to merge a freshly-built FileNode into the
        /// persistent session FileNode.
        std::vector<std::unique_ptr<Node>> take_children() { return std::move(m_children); }

        /// @brief Remove and return the last child added via add_child.
        /// @details Used by FunctionCompiler::compile_function to temporarily
        ///          attach a FunctionNode to the file while its body is being
        ///          built (so that node->file() resolves top-level names), and
        ///          then detach it so the caller can take ownership back.
        std::unique_ptr<Node> release_last_child() {
            if (m_children.empty()) return nullptr;
            auto last = std::move(m_children.back());
            m_children.pop_back();
            orphan(last.get());
            return last;
        }
        /// @brief Remove and return a specific child, or nullptr if not found.
        /// @details Used by FunctionCompiler::compile_function to detach the
        ///          FunctionNode it temporarily attached, without relying on
        ///          "last child" ordering (the body build may have added other
        ///          children to the file in between).
        std::unique_ptr<Node> release_child(const Node *child) {
            for (auto it = m_children.begin(); it != m_children.end(); ++it) {
                if (it->get() == child) {
                    auto out = std::move(*it);
                    m_children.erase(it);
                    orphan(out.get());
                    return out;
                }
            }
            return nullptr;
        }


        // --- Symbols ---

        std::unordered_map<std::string, Node *> take_symbols() {
            auto out = std::move(m_symbols);
            m_ordered_symbols.clear();
            return out;
        }

        // ---- Transaction support ----
        // In REPL mode the same FileNode is reused across inputs. If a
        // compilation throws (a bad DataDeclarationNode, a NodeBuilder error),
        // the FileNode is left with a broken child that will re-throw on
        // every subsequent compile. To prevent that, the caller takes a
        // snapshot before compiling and rolls back on failure.
        //
        // The snapshot captures:
        //   * the number of children (all children past this index are new),
        //   * the number of ordered symbols,
        //   * the set of top-level body expressions (the top-level function's
        //     body is rebuilt on every input, so we just remember the previous
        //     body and restore it on rollback — but actually the body is
        //     replaced wholesale, so we do not need to snapshot it),
        //   * data_structs (rebuilt on every generate()).
        //
        // What is NOT snapshotted, and why:
        //   * m_symbols (the name -> Node* map) — entries added during the
        //     failed compile are not removed, but they point at nodes that
        //     were also added as children and will be removed by the resize.
        //     Leaving stale keys is harmless: lookup() will find a pointer
        //     to freed memory only if the key was bound to a node that is
        //     now gone. To be safe we also erase the keys added since the
        //     snapshot.
        
        // ---- Placeholders for heap-allocated top-level defines ----
    public: 
        //
        // NodeBuilder::build_define calls add_placeholder(name) for
        // each heap-allocated define. At make_binary time, FileNode
        // emits an extra entry per such name: a valid DCEntry with an
        // 8-byte payload of zeros. Globals::register_symbols_from_file
        // indexes the name pointing at those zeros, so that
        // StoreGlobalNode::emit can resolve it via LookupPointer and
        // overwrite the pointer at runtime.
        //
        // One entry per name; add_placeholder deduplicates.
        void add_placeholder(const std::string &name) {
            if (std::find(m_placeholders.begin(), m_placeholders.end(), name) ==
                m_placeholders.end()) {
                m_placeholders.push_back(name);
            }
        }
        /// @brief Record a module that this file depends on.
        /// @details Called by NodeBuilder::build_require. Deduplicated:
        ///          a file that requires the same module twice lists it once.
        void add_require(const std::string &module_name) {
            if (std::find(m_requires.begin(), m_requires.end(), module_name) == m_requires.end()) {
                m_requires.push_back(module_name);
            }
        }

        /// @brief Remove a dependency recorded by add_require.
        /// @details Used by save_session / save_module to strip a
        ///          self-dependency before generation. A module that
        ///          requires itself cannot be loaded.
        void remove_require(const std::string &module_name) {
            m_requires.erase(std::remove(m_requires.begin(), m_requires.end(), module_name),
                             m_requires.end());
        }

        const std::vector<std::string> & get_requires() const {
            return m_requires;
        }
    private:
        const std::vector<std::string> &placeholders() const { return m_placeholders; }

    public:
        // ---- Transaction support for REPL ----
        //
        // In REPL mode the Compiler reuses the same FileNode across inputs.
        // If a compile throws (NodeBuilder error, DataDeclarationNode error),
        // the FileNode is left with a broken child that re-throws on every
        // subsequent compile, poisoning the session.
        //
        // The Compiler takes a snapshot before compiling an input and calls
        // rollback() if compile_internal throws. This restores the FileNode
        // to exactly the state it had before the input.
        //
        // Cost: copying m_symbols on every REPL input. For a human typing at
        // a REPL this is negligible (< 1ms for thousands of symbols). For
        // batch compilation it is not used at all: each file gets its own
        // FileNode, which is discarded on error.
        struct Snapshot {
            std::size_t                             children = 0;
            std::unordered_map<std::string, Node *> symbols;
            std::vector<Node *>                     ordered_symbols;
            bool                                    valid = false;
            std::vector<std::string>                placeholders; 
        };

        [[nodiscard]] Snapshot take_snapshot() const {
            Snapshot s;
            s.children = m_children.size();
            s.symbols = m_symbols;
            s.ordered_symbols = m_ordered_symbols;
            s.valid = true;
            s.placeholders = m_placeholders;
            return s;
        }

        void rollback(Snapshot &&s) {
            if (!s.valid) return;
            if (s.children < m_children.size()) { m_children.resize(s.children); }
            m_symbols = std::move(s.symbols);
            m_ordered_symbols = std::move(s.ordered_symbols);
            m_dataStructs.clear();
            m_placeholders = std::move(s.placeholders);
            // m_top_level is a raw pointer into m_children. After a resize
            // that removes children, it may dangle. Re-derive it from the
            // children that survive: the top-level function, if it still
            // exists, is the one with name "top-level".
            m_top_level = nullptr;
            for (auto &child : m_children) {
                if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                    if (fn->name() == "top-level") {
                        m_top_level = fn;
                        break;
                    }
                }
            }

            s.valid = false;
        }


    private:


        /// @brief Collect all emitting children (FunctionNode + DataDeclarationNode)
        ///        in the natural order they appear in the file.
        std::vector<ProgramBinaryElement> collect_all(DataSegment &state);

        /// @brief Recursively flatten m_anonChildren of el into out.
        ///
        /// @details Each child gets its DCEntry filled in from its own
        ///          m_structLayout (or "global" if absent), then its own
        ///          nested anonymous children are flattened first, and
        ///          finally the child itself is appended.
        ///
        ///          Called by collect_all for every DataDeclarationNode
        ///          before the parent element is pushed. Order in the
        ///          entry table does not affect name resolution, but
        ///          placing children before the parent makes dumps
        ///          easier to read.
        static void flatten_anon(ProgramBinaryElement &el, std::vector<ProgramBinaryElement> &out);

        /// @brief Assemble the final binary from per-element payloads.
        /// @param program_elements  Payloads (functions, data-structs, ...).
        /// @param state             Global compilation state (strings, etc).
        /// @param out_data_structs  [out] Layouts of data-struct entries, in the
        ///                          order they appear in the entry table. The caller
        ///                          (Compiler) stores them in the resulting BinaryFile.
        ProgramBinaryElement make_binary(std::vector<ProgramBinaryElement> program_elements,
                                         DataSegment                      &state,
                                         std::vector<DataStructEntry>     &out_data_structs);


        /// @brief Build an SsType ProgramBinaryElement for one type.
        /// @details Called by make_binary *after* all function/struct entries have
        ///          been laid out, so that method lambdas can be referenced by
        ///          their absolute file offsets.
        ///
        /// @param type_decl       The deftype node for this type.
        /// @param type_lambdas    Map (method_name -> absolute file offset of its
        ///                        ScriptLambda) for methods of this type that
        ///                        were implemented by defmethod.
        ProgramBinaryElement build_ss_type(const TypeDeclarationNode                  *type_decl,
                      const std::unordered_map<std::string, u64> &type_lambdas);

        /// @brief Build the SsType entry that describes the variable layout
        ///        of a StateScript.
        ///
        /// @details Identical to build_ss_type except:
        ///            * the entry name is "<type>$vars";
        ///            * m_flags has bit 2 (is_state_vars) set.
        ///
        ///          This is the SsType that StateScript::m_pVarType points
        ///          at. It is a separate entry from the StateScript itself
        ///          because layout and behaviour are different concerns.
        ProgramBinaryElement
        build_ss_type_for_script(const StateScriptNode                      *node,
                                 const std::unordered_map<std::string, u64> &type_lambdas);

        /// @brief Build a StateScript ProgramBinaryElement for one type.
        ///
        /// @details Emits:
        ///            - StateScript header (0x50)
        ///            - SsOptions (from Type::options())
        ///            - SsState[]
        ///            - SsOnBlock[] (per state handler)
        ///            - SsTrackGroup[] / SsTrack[] / SsLambda[] (per track)
        ///            - m_pVarDefaults (blob instance from blob pool)
        ///            - the associated SsType (layout) — emitted separately
        ///              by build_ss_type; this method only references it.
        ///
        ///          ScriptLambda pointers inside SsOnBlock / SsLambda are
        ///          resolved via `entry_offsets` (name -> file offset).
        ///
        ///          Naming convention (must match compile_state_script_bodies):
        ///            <type>-<state>-<kind>
        ///            <type>-<state>-event-<event-id>
        ///            <type>-<state>-<kind>-track-<track>-main-<i>
        ///            <type>-<state>-<kind>-track-<track>-frame-<N>
        ProgramBinaryElement
        build_state_script(const StateScriptNode                      *node,
                           const std::unordered_map<std::string, u64> &entry_offsets,
                           u64 ss_type_file_offset, u64 ss_payload_offset);
     
        /// @brief The single synthetic top-level function for this module.
        ///
        /// @details Unlike normal function nodes (which are created per
        ///          `defun` and stored as children), the top-level function
        ///          is a single container for all top-level expressions.
        ///          It is created lazily on the first expression, and all
        ///          subsequent expressions are appended to its body.
        ///
        ///          This is what makes `Globals["top-level"]` a stable
        ///          reference across REPL inputs: there is exactly one
        ///          top-level function per module, and it accumulates
        ///          every expression in order.
        FunctionNode *m_top_level = nullptr;

    public:
        /// @brief Produce a unique name for an anonymous structure.
        ///
        /// @details The base name is "<owner>-<field>". If that name is
        ///          already bound in this file (user-defined or from a
        ///          previous anonymous child), a "#N" suffix is appended,
        ///          where N is a monotonically increasing counter per
        ///          base name. The first collision yields "<base>#1",
        ///          the second "<base>#2", and so on.
        std::string make_unique_anon_name(const std::string &base);

    private:
        std::unordered_map<std::string, u32> m_anon_name_counter;
    };

} // namespace sootc