// BinaryFileInspector.cpp
#include "BinaryFileInspector.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/StringIdManager.hpp"
#include "util/Formatter.hpp"
#include <bitset>
#include <memory>
#include <string>

using namespace util;

namespace carbon {

BinaryFileInspector::BinaryFileInspector(BinaryFile* file, int indent)
    : m_file(file), m_indent(indent), m_formatter(std::make_unique<OutputFormatter>()) {}

void BinaryFileInspector::inspect() {
        // Header
        m_formatter->format("=== Binary File: {} ===\n", m_file->m_path.string());
        inspect_header();
        
        // Relocation table
        inspect_relocations();
        
        // String table
        m_formatter->format("\n--- String Table ---\n");
        for (const auto& [id, str] : m_file->m_sidCache) {
            m_formatter->format("  {}: {}\n", sid_str(id), str);
        }
        
        // Entries
        m_formatter->format("\n--- Entries ({} total) ---\n", m_file->m_dcheader->m_numEntries);
        {
            IFormatter::Block block(*m_formatter, m_indent);
            auto header = m_file->m_dcheader;
            auto entries = safe_get_ptr(header->m_pStartOfData, "entries");
            for (u32 i = 0; i < header->m_numEntries; i++) {
                inspect_entry(&entries[i]);
            }
        }
}

void BinaryFileInspector::inspect_relocations(u32 limit_lines) {
    m_formatter->output("\n--- Relocation Table ---\n");
    
    IFormatter::Block indent_block(*m_formatter, m_indent);
    
    location reloc_base = m_file->m_relocTable;
    
    if (reloc_base.m_ptr == nullptr) {
        m_formatter->output("(relocation table is empty)\n");
        return;
    }
    
    const u32 table_size_bytes = reloc_base.get<u32>(-4);
    auto header = m_file->m_dcheader;
    
    m_formatter->output("Relocation table info:\n");
    m_formatter->output("  Size: {} bytes = {} bits\n", table_size_bytes, table_size_bytes * 8);
    m_formatter->output("  m_textSize: {} bytes (0x{:X})\n", header->m_textSize, header->m_textSize);
    m_formatter->output("  m_pStartOfData: {}\n", (void*)header->m_pStartOfData);
    
    if (table_size_bytes == 0 || table_size_bytes > 1024 * 1024) {
        m_formatter->output("  (invalid table size)\n");
        return;
    }
    
    // Подсчёт релокаций
    u32 total_relocs = 0;
    for (u32 i = 0; i < table_size_bytes; i++) {
        total_relocs += std::bitset<8>(reloc_base.get<u8>(i)).count();
    }
    m_formatter->output("  Total relocations: {}\n\n", total_relocs);
    
    if (total_relocs == 0) return;
    
    // Вывод bitmap
    m_formatter->output("Bitmap:\n");
    for (u32 i = 0; i < table_size_bytes; i++) {
        if (i % 8 == 0) m_formatter->output("  ");
        u8 byte = reloc_base.get<u8>(i);
        m_formatter->output("{:02X} ", byte);
        if ((i + 1) % 8 == 0) m_formatter->output("\n");
    }
    m_formatter->output("\n");
    
    // ============================================
    // Читаем релокации ПРЯМО из файла
    // ============================================
    
    m_formatter->output("\nRelocation details (bit_index -> file_offset -> original_value):\n");
    IFormatter::Block details_block(*m_formatter, 2);
    
    u32 relocs_printed = 0;
    
    for (u32 byte_idx = 0; byte_idx < table_size_bytes && (limit_lines == 0 || relocs_printed < limit_lines); byte_idx++) {
        u8 byte = reloc_base.get<u8>(byte_idx);
        if (byte == 0) continue;
        
        for (int bit = 0; bit < 8; bit++) {
            if (byte & (1 << bit)) {
                u64 bit_index = static_cast<u64>(byte_idx) * 8 + bit;
                u64 file_offset = bit_index * 8;  // смещение от начала файла
                
                // Читаем значение ПРЯМО из файла по этому смещению
                const u64* raw_ptr = reinterpret_cast<const u64*>(m_file->m_bytes.get() + file_offset);
                u64 raw_value = *raw_ptr;
                
                m_formatter->output("bit {:4d}: file_offset=0x{:08X} value=0x{:016X}\n",
                                   bit_index, file_offset, raw_value);
                relocs_printed++;
            }
        }
    }
    
    if (relocs_printed < total_relocs) {
        m_formatter->output("\n  ... and {} more\n", total_relocs - relocs_printed);
    }
    m_formatter->output("\n");
}


std::string  BinaryFileInspector::ptr_str(const void* ptr) {
    if (!ptr) {
        return fmt::format("nullptr");
    } else {
        return fmt::format("0x{:016X}", reinterpret_cast<uintptr_t>(ptr));
    }
}

std::string BinaryFileInspector::sid_str(sid64 id) {
    if (id == 0) {
        return "(null)";
    }
    
    try {
        const char* cstr = StringIdManager::instance().get_cstring(id);
        if (cstr && cstr[0] != '\0') {
            return std::string(cstr);
        }
    } catch (const std::exception& e) {
        // Fallback to hex on exception
        return fmt::format("0x{:016X} (error: {})", id, e.what());
    }
    
    return fmt::format("0x{:016X}", id);
}

std::string BinaryFileInspector::type_name(symbol_type type) {
    switch (type) {
        case symbol_type::B8: return "bool";
        case symbol_type::I32: return "int32";
        case symbol_type::F32: return "float32";
        case symbol_type::SS: return "StateScript";
        case symbol_type::HASH: return "hash";
        case symbol_type::LAMBDA: return "lambda";
        default: return "unknown";
    }
}

std::string BinaryFileInspector::reg_name(u8 reg) {
    if (reg < 32) {
        return fmt::format("r{}", reg);
    } else if (reg < 48) {
        return fmt::format("arg_{}", reg - 32);
    } else if (reg == 49) {
        return "r49";
    }
    return fmt::format("r{}", reg);
}

std::string BinaryFileInspector::resolve_symbol(u16 index, const ScriptLambda* lambda) {
    if (!lambda || !lambda->m_pSymbols) return fmt::format("ST[{}]", index);
    
    u64* symbols = lambda->m_pSymbols;
    if (index >= lambda->m_numInstructions) { // symbols table follows instructions
        // Need to calculate actual symbol offset
        return fmt::format("ST[{}]", index);
    }
    
    // Try to resolve from symbol cache
    sid64 sid = symbols[index];
    auto it = m_file->m_sidCache.find(sid);
    if (it != m_file->m_sidCache.end()) {
        return fmt::format("{} [{}]", it->second, sid_str(sid));
    }
    return fmt::format("ST[{}] -> {}", index, sid_str(sid));
}

std::string BinaryFileInspector::resolve_float(u16 index, const ScriptLambda* lambda) {
    if (!lambda || !lambda->m_pSymbols) return "?";
    
    // Float values are stored in the symbol table
    u64* symbols = lambda->m_pSymbols;
    if (index < lambda->m_numInstructions) {
        float f;
        std::memcpy(&f, &symbols[index], sizeof(float));
        return fmt::format("{:.6f}", f);
    }
    return "?";
}

std::string BinaryFileInspector::format_instruction(const Instruction& ins, const ScriptLambda* lambda) {
    auto* info = get_instruction_info(ins.opcode);
    if (!info) return "<unknown instruction>";
    
    std::string result = info->name;
    
    switch (info->oprands_count()) {
        case 1:
            if (info->a_type == OperandType::REG) {
                return fmt::format(" {}", reg_name(ins.destination));
            }
            break;
            
        case 2:
            if (info->a_type == OperandType::REG) {
                return fmt::format(" {}", reg_name(ins.destination));
            } else if (info->a_type == OperandType::IMM_U16) {
                return fmt::format(" {}", ins.destination);
            }
            
            if (info->b_type == OperandType::REG) {
                return fmt::format(", {}", reg_name(ins.operand1));
            } else if (info->b_type == OperandType::IMM_U16) {
                u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
                return fmt::format(", {}", imm);
            } else if (info->b_type == OperandType::IMM_I16) {
                i16 imm = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
                return fmt::format(", {}", imm);
            }
            break;
            
        case 3:
            if (info->a_type == OperandType::REG) {
                return fmt::format(" {}", reg_name(ins.destination));
            }
            if (info->b_type == OperandType::REG) {
                return fmt::format(", {}", reg_name(ins.operand1));
            } else if (info->b_type == OperandType::IMM_U16) {
                return fmt::format(", {}", ins.operand1);
            }
            if (info->c_type == OperandType::REG) {
                return fmt::format(", {}", reg_name(ins.operand2));
            } else if (info->c_type == OperandType::IMM_U8) {
                return fmt::format(", {}", ins.operand2);
            } else if (info->c_type == OperandType::IMM_I16) {
                i16 imm = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
                return fmt::format(", {}", imm);
            }
            break;
    }
    
    // Add comments for special instructions
    if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt || ins.opcode == Opcode::LookupFloat) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        return fmt::format("  ; = {}", resolve_symbol(idx, lambda));
    } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        return fmt::format("  ; = {}", resolve_float(idx, lambda));
    } else if (ins.opcode == Opcode::BranchIfNot || ins.opcode == Opcode::BranchIf || ins.opcode == Opcode::Branch) {
        i16 offset = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
        return fmt::format("  ; => L_{:X}", offset);
    }
    
    return result;
}

void BinaryFileInspector::inspect_header() {
    auto* hdr = m_file->m_dcheader;
    if (!hdr) {
        m_formatter->format("<null header>");
        return;
    }
    
    std::string result;
    m_formatter->format("Magic:          0x{:08X} ({})\n", hdr->m_magic, 
                                  hdr->m_magic == DC_MAGIC ? "DC00" : "INVALID");
    m_formatter->format("Version:        {}\n", hdr->m_versionNumber);
    m_formatter->format("Text Size:      0x{:X} ({} bytes)\n", hdr->m_textSize, hdr->m_textSize);
    m_formatter->format("Strings Offset: 0x{:X}\n", hdr->m_stringsOffset);
    m_formatter->format("Field 10:       {}\n", hdr->field_10);
    m_formatter->format("Num Entries:    {}\n", hdr->m_numEntries);
    m_formatter->format("Data Start:     {}\n", ptr_str(hdr->m_pStartOfData));
}

void BinaryFileInspector::inspect_entry(const DCEntry* entry) {
    if (!entry) {
        m_formatter->output("Entry: NULL\n");
        return;
    }
    
    m_formatter->output("Entry:\n");
    
    IFormatter::Block block(*m_formatter, m_indent);
    m_formatter->output("Address: {:p}\n", (void*)entry);  // Исправлено: entry, а не &entry
    
    // Безопасное получение строк с проверкой
    std::string name_str = sid_str(entry->m_nameID);
    m_formatter->output("Name: {}\n", name_str);
    
    std::string type_str = sid_str(entry->m_typeId);
    m_formatter->output("Type: {}\n", type_str);
    
    m_formatter->output("Ptr: {}\n", ptr_str(entry->m_entryPtr));
    
    // Try to inspect based on type - ТОЛЬКО если ptr не нулевой
    if (entry->m_entryPtr != nullptr && entry->m_typeId != 0) {
        // Дополнительная проверка: указатель должен быть в пределах файла
        uintptr_t ptr_val = reinterpret_cast<uintptr_t>(entry->m_entryPtr);
        uintptr_t base_val = reinterpret_cast<uintptr_t>(m_file->m_bytes.get());
        uintptr_t max_val = base_val + m_file->m_dcheader->m_textSize + sizeof(DC_Header);
        
        if (ptr_val >= base_val && ptr_val < max_val) {
            try {
                if (entry->m_typeId == StringId("state-script").value) {
                    auto* ss = static_cast<const StateScript*>(entry->m_entryPtr);
                    inspect_state_script(ss);
                }
            } catch (const std::exception& e) {
                m_formatter->output("Error inspecting entry: {}\n", e.what());
            }
        } else {
            m_formatter->output("Warning: Ptr points outside file bounds (0x{:X} not in [0x{:X}, 0x{:X}])\n",
                               ptr_val, base_val, max_val);
        }
    }
}

void BinaryFileInspector::inspect_state_script(const StateScript* ss) {
    if (!ss) {
        m_formatter->output("StateScript: NULL\n");
        return;
    }
    
    m_formatter->output("StateScript:\n");
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->output("ID: {}\n", sid_str(ss->m_stateScriptId));
    m_formatter->output("Initial State: {}\n", sid_str(ss->m_initialStateId));
    m_formatter->output("State Count: {}\n", ss->m_stateCount);
    m_formatter->output("Line: {}\n", ss->m_line);
    m_formatter->output("Debug File: {}\n", ss->m_pDebugFileName ? ss->m_pDebugFileName : "(null)");
    m_formatter->output("Error Name: {}\n", ss->m_pErrorName ? ss->m_pErrorName : "(null)");
    
    if (ss->m_pSsDeclList) {
        inspect_declaration_list(ss->m_pSsDeclList);
    } else {
        m_formatter->output("Declaration List: NULL\n");
    }
    
    if (ss->m_pSsOptions) {
        inspect_options(ss->m_pSsOptions);
    } else {
        m_formatter->output("Options: NULL\n");
    }
    
    if (ss->m_pSsStateTable && ss->m_stateCount > 0) {
        m_formatter->output("States:\n");
        IFormatter::Block state_block(*m_formatter, m_indent);
        for (i16 i = 0; i < ss->m_stateCount; i++) {
            inspect_state(&ss->m_pSsStateTable[i]);
        }
    }
}

void BinaryFileInspector::inspect_declaration_list(const SsDeclarationList* list) {
    if (!list) {
        m_formatter->output("Declaration List: NULL\n");
        return;
    }
    
    m_formatter->output("Declaration List:\n");
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->output("Total Size: {} bytes\n", list->m_totalDeclarationSize);
    m_formatter->output("Num Declarations: {}\n", list->m_numDeclarations);
    
    if (list->m_pDeclarations && list->m_numDeclarations > 0) {
        m_formatter->output("Declarations:\n");
        IFormatter::Block decl_block(*m_formatter, m_indent);
        for (u32 i = 0; i < list->m_numDeclarations; i++) {
            inspect_declaration(&list->m_pDeclarations[i]);
        }
    }
}

void BinaryFileInspector::inspect_declaration(const SsDeclaration* decl) {
    std::string result;
    m_formatter->format("Declaration:\n");
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->format("ID: {}\n", sid_str(decl->m_declId));
    m_formatter->format("Type: {}\n", sid_str(decl->m_declTypeId));
    m_formatter->format("Size: {} bytes\n", decl->m_varSizeSum);
    m_formatter->format("Is Var: {}\n", decl->m_isVar);
    m_formatter->format("Value Ptr: {}\n", ptr_str(decl->m_pDeclValue));
}

void BinaryFileInspector::inspect_options(const SsOptions* opts) {
    std::string result;
    m_formatter->format("Options:\n");
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->format("Option String: {}\n", opts->m_optionString ? opts->m_optionString : "(null)");
    m_formatter->format("Unknown Flags: 0x{:X}\n", opts->m_unknownFlags);
    
    if (opts->m_pSymbolArray) {
        inspect_symbol_array(opts->m_pSymbolArray, "SymbolArray");
    }
    if (opts->m_symbolArray2) {
        inspect_symbol_array(opts->m_symbolArray2, "SymbolArray2");
    }
    if (opts->m_symbolArray3) {
        inspect_symbol_array(opts->m_symbolArray3, "SymbolArray3");
    }
    if (opts->m_symbolArray4) {
        inspect_symbol_array(opts->m_symbolArray4, "SymbolArray4");
    }
    
    m_formatter->format("Field 0x38: {}\n", opts->m_always5);
    m_formatter->format("Field 0x3C: {}\n", opts->m_mostly0);
}

void BinaryFileInspector::inspect_symbol_array(const SymbolArray* arr, const std::string& name) {
    std::string result;
    m_formatter->format("{}:\n", name);
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->format("Num Entries: {}\n", arr->m_numEntries);
    m_formatter->format("Unknown: {}\n", arr->m_unk);
    
    if (arr->m_pSymbols && arr->m_numEntries > 0) {
        m_formatter->format("Symbols:\n");
        IFormatter::Block sym_block(*m_formatter, m_indent);
        for (u32 i = 0; i < arr->m_numEntries; i++) {
            m_formatter->format("[{}] {}\n", i, sid_str(arr->m_pSymbols[i]));
        }
    }
}

void BinaryFileInspector::inspect_state(const SsState* state) {
    std::string result;
    m_formatter->format("State: {}\n", sid_str(state->m_stateId));
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    m_formatter->format("Num OnBlocks: {}\n", state->m_numSsOnBlocks);
    
    if (state->m_pSsOnBlocks && state->m_numSsOnBlocks > 0) {
        m_formatter->format("OnBlocks:\n");
        IFormatter::Block onblock_block(*m_formatter, m_indent);
        for (i64 i = 0; i < state->m_numSsOnBlocks; i++) {
            inspect_on_block(&state->m_pSsOnBlocks[i]);
        }
    }
}

void BinaryFileInspector::inspect_on_block(const SsOnBlock* block) {
    std::string result;
    std::string type_str;
    
    switch (block->m_blockType) {
        case BlockType::Start: type_str = "Start"; break;
        case BlockType::End: type_str = "End"; break;
        case BlockType::Event: type_str = "Event"; break;
        case BlockType::Update: type_str = "Update"; break;
        case BlockType::Virtual: type_str = "Virtual"; break;
        case BlockType::Code: type_str = "Code"; break;
        case BlockType::Exit: type_str = "Exit"; break;
        case BlockType::Post: type_str = "Post"; break;
        default: type_str = fmt::format("Unknown({})", static_cast<int>(block->m_blockType));
    }
    
    if (block->m_blockType == BlockType::Event) {
        m_formatter->format("OnBlock: {} ({})\n", type_str, sid_str(block->m_blockEventId));
    } else {
        m_formatter->format("OnBlock: {}\n", type_str);
    }
    
    IFormatter::Block inner_block(*m_formatter, m_indent);
    
    if (block->m_pScriptLambda) {
        inspect_script_lambda(block->m_pScriptLambda, block->name());
    }
    
    // Track group inspection
    m_formatter->format("TrackGroup:\n");
    IFormatter::Block track_block(*m_formatter, m_indent);
    m_formatter->format("Name: {}\n", block->m_trackGroup.m_name ? block->m_trackGroup.m_name : "(null)");
    m_formatter->format("Num Tracks: {}\n", block->m_trackGroup.m_numTracks);
    
    if (block->m_trackGroup.m_aTracks && block->m_trackGroup.m_numTracks > 0) {
        m_formatter->format("Tracks:\n");
        IFormatter::Block track_list_block(*m_formatter, m_indent);
        for (i16 i = 0; i < block->m_trackGroup.m_numTracks; i++) {
            inspect_track(&block->m_trackGroup.m_aTracks[i]);
        }
    }
}

void BinaryFileInspector::inspect_track(const SsTrack* track) {
    std::string result;
    m_formatter->format("Track: {}\n", sid_str(track->m_trackId));
    
    IFormatter::Block block(*m_formatter, m_indent);
    m_formatter->format("Index: {}\n", track->m_trackIdx);
    m_formatter->format("Lambda Count: {}\n", track->m_totalLambdaCount);
    
    if (track->m_pSsLambda && track->m_totalLambdaCount > 0) {
        m_formatter->format("Lambdas:\n");
        IFormatter::Block lambda_block(*m_formatter, m_indent);
        for (i16 i = 0; i < track->m_totalLambdaCount; i++) {
            inspect_lambda(&track->m_pSsLambda[i]);
        }
    }
}

void BinaryFileInspector::inspect_lambda(const SsLambda* lambda) {
    std::string result;
    m_formatter->format("SsLambda:\n");
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    if (lambda->m_pScriptLambda) {
        inspect_script_lambda(lambda->m_pScriptLambda);
    }
    m_formatter->format("Counter: 0x{:X}\n", lambda->m_someSortOfCounter);
}

void BinaryFileInspector::disassemble(const ScriptLambda* lambda, const std::string& name) {
    std::string result;
    
    u32 num_ins = lambda->m_numInstructions;
    auto* code = lambda->get_code_ptr();
    auto* symbols = lambda->get_symbols_ptr();
    
    m_formatter->format("script-lambda {} {{\n", name.empty() ? "" : fmt::format("[{}] ", name));
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    // Count arguments (registers >= 32 that are read before being written)
    std::set<u8> args;
    for (u32 i = 0; i < num_ins; i++) {
        const auto& ins = code[i];
        auto* info = get_instruction_info(ins.opcode);
        if (!info) continue;
        
        // Check destination register (write)
        if (info->a_type == OperandType::REG && ins.destination >= 32 && ins.destination < 48) {
            // This is writing to an arg register - might be a param being set
        }
        // Check source registers (reads)
        if (info->b_type == OperandType::REG && ins.operand1 >= 32 && ins.operand1 < 48) {
            args.insert(ins.operand1);
        }
        if (info->c_type == OperandType::REG && ins.operand2 >= 32 && ins.operand2 < 48) {
            args.insert(ins.operand2);
        }
    }
    
    if (!args.empty()) {
        m_formatter->format("[{} args] ", args.size());
        bool first = true;
        for (u8 arg : args) {
            if (!first) m_formatter->format(", ");
            m_formatter->format("arg_{}", arg - 32);
            first = false;
        }
        m_formatter->format("\n");
    }
    m_formatter->format("\n");
    
    // Disassemble instructions
    for (u32 i = 0; i < num_ins; i++) {
        const auto& ins = code[i];
        
        // Check if this is a target of a branch
        bool is_target = false;
        for (u32 j = 0; j < num_ins; j++) {
            const auto& check_ins = code[j];
            if (check_ins.opcode == Opcode::BranchIfNot || check_ins.opcode == Opcode::BranchIf || check_ins.opcode == Opcode::Branch) {
                i16 offset = static_cast<i16>((static_cast<u16>(check_ins.operand2) << 8) | check_ins.operand1);
                if (j + offset == i) {
                    is_target = true;
                    break;
                }
            }
        }
        
        if (is_target) {
            m_formatter->format("L_{:X}:\n", i);
        }
        
        m_formatter->format("{:04X}   {:08X}   {:02X} {:02X} {:02X}   {:<20}",
            i,
            reinterpret_cast<uintptr_t>(&code[i]),
            static_cast<u8>(ins.opcode),
            ins.destination,
            ins.operand1,
            ins.operand2,
            format_instruction(ins, lambda));
        
        // Add comment with resolved values
        if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt || ins.opcode == Opcode::LookupFloat) {
            u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            m_formatter->format("  ; {}", resolve_symbol(idx, lambda));
        } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
            u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            m_formatter->format("  ; = {}", resolve_float(idx, lambda));
        } else if (ins.opcode == Opcode::Move && ins.destination == 49) {
            m_formatter->format("  ; saving result");
        } else if (ins.opcode == Opcode::Call || ins.opcode == Opcode::CallFf) {
            m_formatter->format("  ; call with {} args", ins.operand2);
        }
        
        m_formatter->format("\n");
    }
    
    // Symbol table
    if (symbols && num_ins > 0) {
        m_formatter->format("\nSYMBOL TABLE:\n");
        IFormatter::Block sym_block(*m_formatter, m_indent);
        
        for (u32 i = 0; i < num_ins; i++) {
            sid64 sid = symbols[i];
            auto it = m_file->m_sidCache.find(sid);
            if (it != m_file->m_sidCache.end()) {
                m_formatter->format("{:04X}   {:08X}   {}: {}\n", 
                    i, reinterpret_cast<uintptr_t>(&symbols[i]), it->second, sid_str(sid));
            } else {
                m_formatter->format("{:04X}   {:08X}   {}\n", 
                    i, reinterpret_cast<uintptr_t>(&symbols[i]), sid_str(sid));
            }
        }
    }
    
    m_formatter->format("}}\n\n");
}

void BinaryFileInspector::inspect_script_lambda(const ScriptLambda* lambda, const std::string& name) {
    return disassemble(lambda, name);
}

void BinaryFileInspector::inspect_symbol(const symbol* sym) {
    std::string result;
    m_formatter->format("Symbol: ID={}, Type={}\n", sid_str(sym->id), type_name(sym->type));
    
    IFormatter::Block block(*m_formatter, m_indent);
    
    switch (sym->type) {
        case symbol_type::B8:
            m_formatter->format("Value: {}\n", sym->b8_ptr ? *sym->b8_ptr : false);
            break;
        case symbol_type::I32:
            m_formatter->format("Value: {}\n", sym->i32_ptr ? *sym->i32_ptr : 0);
            break;
        case symbol_type::F32:
            m_formatter->format("Value: {:.6f}\n", sym->f32_ptr ? *sym->f32_ptr : 0.0f);
            break;
        case symbol_type::SS:
            if (sym->ss_ptr) {
                inspect_state_script(sym->ss_ptr);
            }
            break;
        case symbol_type::LAMBDA:
            if (sym->lambda_ptr) {
                inspect_script_lambda(sym->lambda_ptr);
            }
            break;
        default:
            break;
    }
}

} // namespace carbon