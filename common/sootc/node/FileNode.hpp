// FileNode.hpp
#pragma once

#include "Node.hpp"
#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/ProgramBinaryElement.hpp"
#include "sootc/libs/StringsTable.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

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
    protected:
        void update_self_cache() override { m_cached_file = this; }

    public:
        explicit FileNode(const std::string &name);

        const char *node_type() const override { return "FileNode"; }
        std::string to_string() const override;

        // ---- Name ----
        const std::string &name() const { return m_name; }

        // ---- Binary generation (Node interface) ----
        ProgramBinaryElement generate(StringsTable &strings_table) override;

        // ---- Symbol table ----
        Node *lookup(const std::string &name) override;
        void  bind(const std::string &name, Node *node);
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
        void add_child(std::unique_ptr<Node> child) { m_children.push_back(std::move(child)); }

        /// @brief Take ownership of children and symbols, leaving this FileNode empty.
        /// Used by the REPL to merge a freshly-built FileNode into the
        /// persistent session FileNode.
        std::vector<std::unique_ptr<Node>> take_children() { return std::move(m_children); }

        std::unordered_map<std::string, Node *> take_symbols() {
            auto out = std::move(m_symbols);
            m_ordered_symbols.clear();
            return out;
        }

        // --- Export ---
    public:

        bool export_all() const { return m_export_all; }
        void set_export_all(bool v) { m_export_all = v; }

    private:
        /// @brief Export all file
        bool m_export_all = false;

    private:
        /// @brief Collect all emitting children (FunctionNode + DataDeclarationNode)
        ///        in the natural order they appear in the file.
        std::vector<ProgramBinaryElement> collect_all(StringsTable &state);
        /// @brief Assemble the final binary from per-element payloads.
        /// @param program_elements  Payloads (functions, data-structs, ...).
        /// @param state             Global compilation state (strings, etc).
        /// @param out_data_structs  [out] Layouts of data-struct entries, in the
        ///                          order they appear in the entry table. The caller
        ///                          (Compiler) stores them in the resulting BinaryFile.
        ProgramBinaryElement make_binary(std::vector<ProgramBinaryElement> program_elements,
                                         StringsTable                      &state,
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
    };

} // namespace sootc