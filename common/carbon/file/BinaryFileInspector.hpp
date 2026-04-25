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
        explicit BinaryFileInspector(BinaryFile* file, int indent = 2);
        
        void inspect();

    private:
        BinaryFile*       m_file;
        int               m_indent;
        std::unique_ptr<IFormatter> m_formatter;
        
        // Helper methods
        void ident()   { m_formatter->inc_column(m_indent); }
        void unident() { m_formatter->inc_column(-static_cast<int>(m_indent)); }
        
        std::string ptr_str(const void* ptr);
        std::string sid_str(sid64 id);
        std::string type_name(symbol_type type);
        
        // Disassembly helpers
        std::string reg_name(u8 reg);
        std::string format_instruction(const Instruction& ins, const ScriptLambda* lambda);
        std::string resolve_symbol(u16 index, const ScriptLambda* lambda);
        std::string resolve_float(u16 index, const ScriptLambda* lambda);
        std::string symbol_str(const symbol* sym);
        std::string static_str(StaticType type, u64 value);

        // Inspection methods
        void inspect_header();
        void inspect_entry(const DCEntry* entry);
        void inspect_relocations(u32 limit_lines = 0);
        void inspect_state_script(const StateScript* ss);
        void inspect_declaration_list(const SsDeclarationList* list);
        void inspect_declaration(const SsDeclaration* decl);
        void inspect_options(const SsOptions* opts);
        void inspect_symbol_array(const SymbolArray* arr, const std::string& name);
        void inspect_state(const SsState* state);
        void inspect_on_block(const SsOnBlock* block);
        void inspect_track_group(const SsTrackGroup* group);
        void inspect_track(const SsTrack* track);
        void inspect_lambda(const SsLambda* lambda);
        void inspect_script_lambda(const ScriptLambda* lambda, const std::string& name = "");
        void inspect_symbol(const symbol* sym, int idx);
        
        // Disassembly
        void disassemble(const ScriptLambda* lambda, const std::string& name);

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

        std::string format_instruction(const Instruction& ins, const InstructionInfo* info, const ScriptLambda* lambda);
        std::string format_instruction(const ShortInstruction& ins, const InstructionInfo* info, const ScriptLambda* lambda);
    };

} // namespace carbon