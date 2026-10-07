// FileNode.hpp
#pragma once

#include "Node.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/sootc/libs/GlobalState.hpp"
#include "common/sootc/node/TypeDeclarationNode.hpp"
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
        ProgramBinaryElement generate(GlobalState &state) override;

        // ---- Symbol table ----
        Node *lookup(const std::string &name) override;
        void  bind(const std::string &name, Node *node);

        // ---- Imports ----
        void                           add_import(FileNode *file);
        const std::vector<FileNode *> &imports() const { return m_imports; }

        static void insert_into_reloctable(u8 *reloc_table, u64 &byte_offset, u64 &bit_offset,
                                           u8 bits, u64 num_bits) noexcept;
        
        const std::vector<DataStructEntry> &data_structs() const { return m_dataStructs; }

        /// @brief Append a child node. Used by NodeBuilder when hoisting
        ///        declarations out of a (begin ...) form.
        void add_child(std::unique_ptr<Node> child) { m_children.push_back(std::move(child)); }

    private:
        /// @brief Collect all emitting children (FunctionNode + DataDeclarationNode)
        ///        in the natural order they appear in the file.
        std::vector<ProgramBinaryElement> collect_all(GlobalState &state);
        /// @brief Assemble the final binary from per-element payloads.
        /// @param program_elements  Payloads (functions, data-structs, ...).
        /// @param state             Global compilation state (strings, etc).
        /// @param out_data_structs  [out] Layouts of data-struct entries, in the
        ///                          order they appear in the entry table. The caller
        ///                          (Compiler) stores them in the resulting BinaryFile.
        ProgramBinaryElement make_binary(std::vector<ProgramBinaryElement> program_elements,
                                         GlobalState                      &state,
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
    };

} // namespace sootc