// BinaryFileInspector.hpp
#pragma once

#include "file/BinaryFile.hpp"
#include "file/DCHeader.hpp"
#include "util/Formatter.hpp"
#include <memory>
#include <string>


using  namespace util;

namespace carbon {

    class BinaryFileInspector {
    public:
        /// @brief How much detail to print for declarations, states, and lambdas.
        enum class InspectMode {
            Summary, ///< compact tables only (headers, entries, summaries)
            Full,    ///< summary tables + detailed per-declaration / per-state dump
        };

    explicit BinaryFileInspector(BinaryFile *file, int indent = 2,
                                     InspectMode mode = InspectMode::Summary);

        void inspect();

    private:
        BinaryFile*                 m_file;
        int                         m_indent;
        InspectMode                 m_mode;
        std::unique_ptr<IFormatter> m_formatter;
        
        
        std::string ptr_str(const void* ptr);
        std::string sid_str(sid64 id);
        std::string type_name(symbol_type type);
        
        // Disassembly helpers
        std::string reg_name(u8 reg);
        std::string resolve_symbol(u16 index, const ScriptLambda *lambda);
        std::string resolve_float(u16 index, const ScriptLambda *lambda);
        std::string static_str(StaticType type, u64 value);
        std::string format_instruction(const Instruction &ins, const ScriptLambda *lambda);

    public:
        // Inspection methods
        void inspect_header();
        void inspect_relocations(u32 limit_lines = 64);
        void inspect_entry(const DCEntry *entry);
        /// @brief Print the first `max_bytes` of an entry's payload as raw hex.
        /// @details Used for entry types the inspector does not understand
        ///          (Map, DataStruct, unknown types).
        void inspect_raw_payload(const DCEntry *entry, u32 max_bytes);
        /// @brief Print a compact one-line-per-entry summary table.
        /// @details Columns: index, name, type, kind, payload offset.
        ///          Use this before inspect_entry() for a quick overview.
        void inspect_entry_summary();
        /// @brief Print an overview of the entire file: entries by kind, totals.
        /// @details Called once at the start of inspect() to give a bird's-eye view.
        ///          For each kind of entry, prints the count and the names.
        void inspect_overview();
        /// @brief Print a summary of all types used in the file.
        /// @details For each category (entry types, declaration types, block types),
        ///          prints the distinct SID64 values and their occurrence counts.
        ///          Useful for understanding which data types a file uses without
        ///          dumping the whole structure.
        void inspect_type_summary();
        /// @brief Print the entire file structure as JSON.
        /// @details Produces a machine-readable dump: header, entries, violations.
        ///          Not a full AST dump — only metadata. Used by `dcinspect --json`.
        void inspect_json(std::ostream &os);
        void BinaryFileInspector::inspect_ss_type(const SsType *st);
        void inspect_state_script(const StateScript *ss);
        /// @brief Print a compact one-line summary of a state script.
        /// @details Columns: id, initial state, num declarations, num states,
        ///          num options, options list.
        void inspect_state_script_summary(const StateScript *ss);
        void inspect_declaration_list(const SsDeclarationList* list);
        void inspect_declaration(const SsDeclaration* decl);
        /// @brief Print a compact one-line-per-declaration summary table.
        /// @details Columns: index, name, type, size, value (or ??? for unknown types).
        void inspect_declaration_summary(const SsDeclarationList *list);
        /// @brief Print only the declarations of all state-scripts in the file.
        /// @details Iterates entries, finds all StateScripts, prints their
        ///          declaration tables, and skips everything else (headers, entries,
        ///          relocations). Used by `dcinspect --decls-only`.
        void inspect_declarations_only();
        void inspect_options(const SsOptions* opts);
        void inspect_symbol_array(const SymbolArray* arr, const std::string& name);
        void inspect_state(const SsState *state);
        void inspect_on_block(const SsOnBlock *block);
        /// @brief Print a compact one-line-per-state summary table.
        /// @details Columns: index, name, num-blocks, block types.
        void inspect_state_summary(const StateScript *ss);
        /// @brief Print a detailed table of states, blocks, and tracks.
        /// @details One line per block, with the track names and lambda counts.
        void inspect_state_detail_summary(const StateScript *ss);
        void inspect_track_group(const SsTrackGroup *group);
        void inspect_track(const SsTrack *track);
        void inspect_lambda(const SsLambda *lambda);
        /// @brief Print a compact one-line-per-lambda summary.
        /// @details Columns: idx, lambda kind (script-lambda or null), num instructions,
        ///          num symbols, func-name SID (if any), global-scope SID.
        void inspect_lambda_summary(const SsTrack *track);
        void inspect_script_lambda(const ScriptLambda *lambda, const std::string &name = "");
        void inspect_symbol(const symbol *sym);
        
        // Disassembly
        void disassemble(const ScriptLambda* lambda, const std::string& name);

    protected:
        // Safe pointer conversion method
        bool is_valid_ptr(const void* ptr, size_t size = 1) const {
            if (!ptr) return false;
            
            uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
            uintptr_t base_val = reinterpret_cast<uintptr_t>(m_file->m_bytes.get());
            uintptr_t max_val = base_val + m_file->m_dcheader->m_textSize + sizeof(DC_Header);
            
            return (ptr_val >= base_val && ptr_val + size <= max_val);
        }
        
        template<typename T>
        const T* safe_get_ptr(const T* ptr, const char* name) const {
            if (!ptr) {
                m_formatter->format("WARNING: {} is NULL\n", name);
                return nullptr;
            }
            
            if (!is_valid_ptr(ptr, sizeof(T))) {
                m_formatter->format("WARNING: {} points outside file bounds (ptr=0x{:X})\n", 
                                name, reinterpret_cast<uintptr_t>(ptr));
                return nullptr;
            }
            
            return ptr;
        }

        std::string format_instruction(const LongInstruction& ins, const InstructionInfo* info, const ScriptLambda* lambda);
        std::string format_instruction(const ShortInstruction& ins, const InstructionInfo* info, const ScriptLambda* lambda);
    };

} // namespace carbon

