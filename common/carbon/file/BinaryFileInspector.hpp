// BinaryFileInspector.hpp
#pragma once

#include "file/BinaryFile.hpp"
#include "file/DCHeader.hpp"
#include "util/Formatter.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace carbon {

    class BinaryFileInspector {
    public:
        explicit BinaryFileInspector(BinaryFile& file, u16 indent = 2);
        
        std::string inspect();

    private:
        BinaryFile& m_file;
        u16 m_indent;
        util::Formatter& m_formatter;
        
        // Helper methods
        void ident() { m_formatter.inc_column(m_indent); }
        void unident() { m_formatter.inc_column(-static_cast<int>(m_indent)); }
        
        std::string ptr_str(const void* ptr);
        std::string sid_str(sid64 id);
        std::string type_name(symbol_type type);
        
        // Disassembly helpers
        std::string reg_name(u8 reg);
        std::string format_instruction(const Instruction& ins, const ScriptLambda* lambda);
        std::string resolve_symbol(u16 index, const ScriptLambda* lambda);
        std::string resolve_float(u16 index, const ScriptLambda* lambda);
        
        // Inspection methods
        std::string inspect_header();
        std::string inspect_entry(const DCEntry& entry);
        std::string inspect_state_script(const StateScript& ss);
        std::string inspect_declaration_list(const SsDeclarationList& list);
        std::string inspect_declaration(const SsDeclaration& decl);
        std::string inspect_options(const SsOptions& opts);
        std::string inspect_symbol_array(const SymbolArray& arr, const std::string& name);
        std::string inspect_state(const SsState& state);
        std::string inspect_on_block(const SsOnBlock& block);
        std::string inspect_track_group(const SsTrackGroup& group);
        std::string inspect_track(const SsTrack& track);
        std::string inspect_lambda(const SsLambda& lambda);
        std::string inspect_script_lambda(const ScriptLambda& lambda, const std::string& name = "");
        std::string inspect_symbol(const symbol& sym);
        
        // Disassembly
        std::string disassemble(const ScriptLambda& lambda, const std::string& name);
    };

} // namespace carbon