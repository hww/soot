// BinaryFileInspector.cpp
#include "BinaryFileInspector.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "file/DCScript.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/StringIdManager.hpp"
#include "util/Formatter.hpp"
#include "vm/Instructions.hpp"
#include <bitset>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace util;

namespace carbon {

BinaryFileInspector::BinaryFileInspector(BinaryFile* file, int indent)
    : m_file(file), m_indent(indent), m_formatter(std::make_unique<OutputFormatter>()) {}

void BinaryFileInspector::inspect() {
    m_formatter->print("=== Binary File: {} ===\n", m_file->m_path.string());

    if (!m_file->m_dcheader) {
        m_formatter->print("<null header>\n");
        return;
    }

    inspect_header();
    inspect_relocations(64);

    m_formatter->print("\n--- String Table ---\n");
    for (const auto &[id, str] : m_file->m_sidCache) {
        m_formatter->print("  0x{:016X}: {}\n", id, str);
    }

    m_formatter->print("\n--- Entries ({} total) ---\n", m_file->m_dcheader->m_numEntries);
    IFormatter::Block block(*m_formatter, m_indent);

    auto       *header = m_file->m_dcheader;
    const auto *entries = safe_get_ptr(header->m_pStartOfData, "entries");
    if (entries) {
        for (u32 i = 0; i < header->m_numEntries; i++) { inspect_entry(entries + i); }
    }
}

void BinaryFileInspector::inspect_relocations(u32 limit_lines) {
    m_formatter->print("\n--- Relocation Table ---\n");

    IFormatter::Block indent_block(*m_formatter, m_indent);

    const location reloc_base = m_file->m_relocTable;

    if (reloc_base.m_ptr == nullptr) {
        m_formatter->print("(relocation table is empty)\n");
        return;
    }

    const u32   table_size_bytes = reloc_base.get<u32>(-4);
    const auto *header = m_file->m_dcheader;

    m_formatter->print("Relocation table info:\n");
    m_formatter->print("  Size: {} bytes = {} bits\n", table_size_bytes, table_size_bytes * 8);
    m_formatter->print("  m_textSize: {} bytes (0x{:X})\n", header->m_textSize, header->m_textSize);
    m_formatter->print("  m_stringsOffset: 0x{:X}\n", header->m_stringsOffset);
    m_formatter->print("  m_pStartOfData: {}\n", ptr_str(header->m_pStartOfData));

    if (table_size_bytes == 0 || table_size_bytes > 1024 * 1024) {
        m_formatter->print("  (invalid table size)\n");
        return;
    }

    u32 total_relocs = 0;
    for (u32 i = 0; i < table_size_bytes; i++) {
        total_relocs += std::bitset<8>(reloc_base.get<u8>(i)).count();
    }
    m_formatter->print("  Total relocations: {}\n\n", total_relocs);

    if (total_relocs == 0) return;

    m_formatter->print("Bitmap:\n");
    for (u32 i = 0; i < table_size_bytes; i++) {
        if (i % 8 == 0) m_formatter->print("  ");
        const u8 byte = reloc_base.get<u8>(i);
        m_formatter->print("{:02X} ", byte);
        if ((i + 1) % 8 == 0) m_formatter->print("\n");
    }
    m_formatter->print("\n");
}


std::string BinaryFileInspector::ptr_str(const void *ptr) {
    if (!ptr) return "nullptr";
    return fmt::format("0x{:016X}", reinterpret_cast<uintptr_t>(ptr));
}

std::string BinaryFileInspector::sid_str(sid64 id) {
    if (id == 0) return "(null)";

    try {
        const char *cstr = StringIdManager::instance().get_cstring(id);
        if (cstr && cstr[0] != '\0') { return std::string(cstr); }
    } catch (const std::exception &) {
        // fall through to hex
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
    case symbol_type::UNKNOWN: return "unknown";
    }
    return "unknown";
}

std::string BinaryFileInspector::reg_name(u8 reg) {
    if (reg >= ARG_REGISTERS_OFFSET) { return fmt::format("arg_{}", reg - ARG_REGISTERS_OFFSET); }
    return fmt::format("r{}", reg);
}

std::string BinaryFileInspector::resolve_symbol(u16 index, const ScriptLambda *lambda) {
    if (!lambda || !lambda->m_pSymbols) return fmt::format("ST[{}]", index);
    if (index >= lambda->m_numSymbols) {
        return fmt::format("ST[{}] (out of bounds, m_numSymbols={})", index, lambda->m_numSymbols);
    }

    u64  *symbols = lambda->m_pSymbols;
    sid64 sid = symbols[index];
    auto  it = m_file->m_sidCache.find(sid);
    if (it != m_file->m_sidCache.end()) { return fmt::format("{} [{}]", it->second, sid_str(sid)); }
    return fmt::format("ST[{}] -> {}", index, sid_str(sid));
}

std::string BinaryFileInspector::resolve_float(u16 index, const ScriptLambda *lambda) {
    if (!lambda || !lambda->m_pSymbols) return "?";
    if (index >= lambda->m_numSymbols) return "?";

    u64  *symbols = lambda->m_pSymbols;
    float f;
    std::memcpy(&f, &symbols[index], sizeof(float));
    return fmt::format("{:.6f}", f);
}

std::string BinaryFileInspector::format_instruction(const Instruction& ins, const ScriptLambda* lambda) {
    auto* info = get_instruction_info(ins.opcode);
    if (!info) return "<unknown instruction>";
    
    std::string result = info->name;
    
    switch (info->operands_count()) {
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
    } else if (ins.opcode == Opcode::BranchIfNot || ins.opcode == Opcode::BranchIf ||
               ins.opcode == Opcode::Branch) {
        u16 target = static_cast<u16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
        return fmt::format("  ; => L_{:X}", target);
    }
    
    return result;
}

void BinaryFileInspector::inspect_header() {
    const auto *hdr = m_file->m_dcheader;
    if (!hdr) {
        m_formatter->print("<null header>\n");
        return;
    }

    const u64 data_start_offset = reinterpret_cast<uintptr_t>(hdr->m_pStartOfData) -
                                  reinterpret_cast<uintptr_t>(m_file->m_bytes.get());

    m_formatter->print("Magic:          0x{:08X} ({})\n", hdr->m_magic,
                       hdr->m_magic == DC_FILE_MAGIC ? "DC00" : "INVALID");
    m_formatter->print("Version:        {}\n", hdr->m_versionNumber);
    m_formatter->print("Text Size:      0x{:X} ({} bytes)\n", hdr->m_textSize, hdr->m_textSize);
    m_formatter->print("Strings Offset: 0x{:X}\n", hdr->m_stringsOffset);
    m_formatter->print("Field 10:       {}\n", hdr->m_always1);
    m_formatter->print("Num Entries:    {}\n", hdr->m_numEntries);
    m_formatter->print("Data Start:     0x{:X} (offset from file start)\n", data_start_offset);
}

void BinaryFileInspector::inspect_entry(const DCEntry* entry) {
    if (!entry) {
        m_formatter->print("Entry: NULL\n");
        return;
    }
    
    m_formatter->print("Entry:\n");
    
    IFormatter::Block block(*m_formatter, m_indent);
    m_formatter->print("Address: {:p}\n", (void*)entry);  // Исправлено: entry, а не &entry
    
    // Безопасное получение строк с проверкой
    std::string name_str = sid_str(entry->m_nameID);
    m_formatter->print("Name: {}\n", name_str);
    
    std::string type_str = sid_str(entry->m_typeId);
    m_formatter->print("Type: {}\n", type_str);
    
    m_formatter->print("Ptr:  {}\n", ptr_str(entry->m_entryPtr));
    
    // Try to inspect based on type - ТОЛЬКО если ptr не нулевой
    if (entry->m_entryPtr != nullptr && entry->m_typeId != 0) {
        if (is_valid_ptr(entry->m_entryPtr)) {
            try {
                if (entry->m_typeId == StringId("state-script").value) {
                    const StateScript *ss =
                        reinterpret_cast<const StateScript *>(entry->m_entryPtr);
                    inspect_state_script(ss);
                } else if (entry->m_typeId == StringId("script-lambda").value) {
                    const ScriptLambda *sl =
                        reinterpret_cast<const ScriptLambda *>(entry->m_entryPtr);
                    inspect_script_lambda(sl);
                }
            } catch (const std::exception &e) {
                m_formatter->print("Error inspecting entry: {}\n", e.what());
            }
        } else {
            m_formatter->print("Warning: Ptr points outside file bounds (0x{:X})\n",
                               reinterpret_cast<uintptr_t>(entry->m_entryPtr));
        }
    }
}

void BinaryFileInspector::inspect_state_script(const StateScript *ss) {
    if (!ss) {
        m_formatter->print("StateScript: NULL\n");
        return;
    }

    m_formatter->print("StateScript:\n");
    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("ID: {}\n", sid_str(ss->m_stateScriptId));
    m_formatter->print("Initial State: {}\n", sid_str(ss->m_initialStateId));
    m_formatter->print("State Count: {}\n", ss->m_stateCount);
    m_formatter->print("Line: {}\n", ss->m_line);
    m_formatter->print("Debug File: {}\n", ss->m_pDebugFileName ? ss->m_pDebugFileName : "(null)");
    m_formatter->print("Error Name: {}\n", ss->m_pErrorName ? ss->m_pErrorName : "(null)");

    inspect_declaration_list(ss->m_pSsDeclList);
    inspect_options(ss->m_pSsOptions);

    if (ss->m_pSsStateTable && ss->m_stateCount > 0) {
        m_formatter->print("States:\n");
        IFormatter::Block state_block(*m_formatter, m_indent);
        for (i16 i = 0; i < ss->m_stateCount; i++) { inspect_state(&ss->m_pSsStateTable[i]); }
    }
}

void BinaryFileInspector::inspect_declaration_list(const SsDeclarationList *list) {
    if (!list) {
        m_formatter->print("Declaration List: NULL\n");
        return;
    }

    m_formatter->print("Declaration List:\n");
    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("Total Size: {} bytes\n", list->m_totalDeclarationSize);
    m_formatter->print("Num Declarations: {}\n", list->m_numDeclarations);

    if (list->m_pDeclarations && list->m_numDeclarations > 0) {
        m_formatter->print("Declarations:\n");
        IFormatter::Block decl_block(*m_formatter, m_indent);
        for (u32 i = 0; i < list->m_numDeclarations; i++) {
            inspect_declaration(&list->m_pDeclarations[i]);
        }
    }
}

void BinaryFileInspector::inspect_declaration(const SsDeclaration *decl) {
    m_formatter->print("Declaration:\n");

    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("ID:        {}\n", sid_str(decl->m_declId));
    m_formatter->print("Type:      {}\n", sid_str(decl->m_declTypeId));
    m_formatter->print("Size:      {} bytes\n", decl->m_varSizeSum);
    m_formatter->print("Is Var:    {}\n", decl->m_isVar);
    m_formatter->print("Value Ptr: {}\n", ptr_str(decl->m_pDeclValue));
}

void BinaryFileInspector::inspect_options(const SsOptions *opts) {
    if (!opts) {
        m_formatter->print("Options: NULL\n");
        return;
    }

    m_formatter->print("Options:\n");

    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("Option String: {}\n",
                       opts->m_optionString ? opts->m_optionString : "(null)");
    m_formatter->print("Unknown Flags: 0x{:X}\n", opts->m_unknownFlags);

    if (opts->m_pSymbolArray) { inspect_symbol_array(opts->m_pSymbolArray, "SymbolArray"); }
    if (opts->m_symbolArray2) { inspect_symbol_array(opts->m_symbolArray2, "SymbolArray2"); }
    if (opts->m_symbolArray3) { inspect_symbol_array(opts->m_symbolArray3, "SymbolArray3"); }
    if (opts->m_symbolArray4) { inspect_symbol_array(opts->m_symbolArray4, "SymbolArray4"); }

    m_formatter->print("Field 0x38: {}\n", opts->m_always5);
    m_formatter->print("Field 0x3C: {}\n", opts->m_mostly0);
}

void BinaryFileInspector::inspect_symbol_array(const SymbolArray *arr, const std::string &name) {
    m_formatter->print("{}:\n", name);

    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("Num Entries: {}\n", arr->m_numEntries);
    m_formatter->print("Unknown: {}\n", arr->m_unk);

    if (arr->m_pSymbols && arr->m_numEntries > 0) {
        m_formatter->print("Symbols:\n");
        IFormatter::Block sym_block(*m_formatter, m_indent);
        for (u32 i = 0; i < arr->m_numEntries; i++) {
            m_formatter->print("[{}] {}\n", i, sid_str(arr->m_pSymbols[i]));
        }
    }
}

void BinaryFileInspector::inspect_state(const SsState *state) {
    m_formatter->print("State: {}\n", sid_str(state->m_stateId));

    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("Num OnBlocks: {}\n", state->m_numSsOnBlocks);

    if (state->m_pSsOnBlocks && state->m_numSsOnBlocks > 0) {
        m_formatter->print("OnBlocks:\n");
        IFormatter::Block onblock_block(*m_formatter, m_indent);
        for (i64 i = 0; i < state->m_numSsOnBlocks; i++) {
            inspect_on_block(&state->m_pSsOnBlocks[i]);
        }
    }
}

void BinaryFileInspector::inspect_on_block(const SsOnBlock *block) {
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
        m_formatter->print("OnBlock: {} ({})\n", type_str, sid_str(block->m_blockEventId));
    } else {
        m_formatter->print("OnBlock: {}\n", type_str);
    }

    IFormatter::Block inner_block(*m_formatter, m_indent);

    if (block->m_pScriptLambda) { inspect_script_lambda(block->m_pScriptLambda, block->name()); }

    inspect_track_group(&block->m_trackGroup);
}

void BinaryFileInspector::inspect_track(const SsTrack *track) {
    m_formatter->print("Track: {}\n", sid_str(track->m_trackId));

    IFormatter::Block block(*m_formatter, m_indent);
    m_formatter->print("Index: {}\n", track->m_trackIdx);
    m_formatter->print("Lambda Count: {}\n", track->m_totalLambdaCount);

    if (track->m_pSsLambda && track->m_totalLambdaCount > 0) {
        m_formatter->print("Lambdas:\n");
        IFormatter::Block lambda_block(*m_formatter, m_indent);
        for (i16 i = 0; i < track->m_totalLambdaCount; i++) {
            inspect_lambda(&track->m_pSsLambda[i]);
        }
    }
}

void BinaryFileInspector::inspect_track_group(const SsTrackGroup *group) {
    if (!group) {
        m_formatter->print("TrackGroup: NULL\n");
        return;
    }

    m_formatter->print("TrackGroup:\n");
    IFormatter::Block block(*m_formatter, m_indent);

    m_formatter->print("Name: {}\n", group->m_name ? group->m_name : "(null)");
    m_formatter->print("Num Tracks: {}\n", group->m_numTracks);
    m_formatter->print("Total Lambda Count: {}\n", group->m_totalLambdaCount);

    if (group->m_aTracks && group->m_numTracks > 0) {
        m_formatter->print("Tracks:\n");
        IFormatter::Block track_list_block(*m_formatter, m_indent);
        for (i16 i = 0; i < group->m_numTracks; i++) { inspect_track(&group->m_aTracks[i]); }
    }
}

void BinaryFileInspector::inspect_lambda(const SsLambda *lambda) {
    m_formatter->print("SsLambda:\n");

    IFormatter::Block block(*m_formatter, m_indent);

    if (lambda->m_pScriptLambda) { inspect_script_lambda(lambda->m_pScriptLambda); }
    m_formatter->print("Counter: 0x{:X}\n", lambda->m_someSortOfCounter);
}

void BinaryFileInspector::disassemble(const ScriptLambda *lambda, const std::string &name) {
    const auto *code = lambda->get_code_ptr();
    const u32   num_ins = lambda->m_numInstructions;
    const auto *symbols = lambda->get_symbols_ptr();

    m_formatter->print("script-lambda {} {{\n", name);
    IFormatter::Block block(*m_formatter, m_indent);

    std::vector<StaticType> symbol_types(lambda->m_numSymbols);

    for (u32 i = 0; i < num_ins; i++) {
        const auto &ins = code[i];
        const auto *info = get_instruction_info(ins.opcode);

        const u64 file_offset = reinterpret_cast<uintptr_t>(&code[i]) -
                                reinterpret_cast<uintptr_t>(m_file->m_bytes.get());

        if (!info) {
            m_formatter->print("{:04X}   {:08X}   {:02X} {:02X} {:02X}   <unknown>\n", i,
                               file_offset, static_cast<u8>(ins.opcode), ins.destination,
                               ins.operand1, ins.operand2);
            continue;
        }

        if (info->static_type != StaticType::NONE && ins.uim16 < lambda->m_numSymbols) {
            symbol_types[ins.uim16] = info->static_type;
        }

        m_formatter->print("{:04X}   {:08X}   {:02X} {:02X} {:02X} {:02X}  {}\n", i, file_offset,
                           static_cast<u8>(ins.opcode), ins.destination, ins.operand1, ins.operand2,
                           format_instruction(ins, info, lambda));
    }

    const u32 num_syms = lambda->m_numSymbols;
    if (symbols && num_syms > 0) {
        m_formatter->print("\nSYMBOL TABLE:\n");
        IFormatter::Block sym_block(*m_formatter, m_indent);

        for (u32 i = 0; i < num_syms; i++) {
            const u64        symbol = symbols[i];
            const StaticType type = symbol_types[i];
            const u64        sym_offset =
                reinterpret_cast<u64>(&symbols[i]) - reinterpret_cast<u64>(m_file->m_bytes.get());
            m_formatter->print("{:04} {:016X} {:016X} {}\n", i, sym_offset, symbol,
                               static_str(type, symbol));
        }
    }

    m_formatter->print("\n");
}

std::string BinaryFileInspector::static_str(StaticType type, u64 value) {
    switch (type) {
    case StaticType::NONE: return "none";
    case StaticType::I8: return fmt::format("I8 {}", static_cast<i8>(value));
    case StaticType::I16: return fmt::format("I16 {}", static_cast<i16>(value));
    case StaticType::I32: return fmt::format("I32 {}", static_cast<i32>(value));
    case StaticType::I64: return fmt::format("I64 {}", static_cast<i64>(value));
    case StaticType::U8: return fmt::format("U8 {}", static_cast<u8>(value));
    case StaticType::U16: return fmt::format("U16 {}", static_cast<u16>(value));
    case StaticType::U32: return fmt::format("U32 {}", static_cast<u32>(value));
    case StaticType::U64: return fmt::format("U64 {}", value);
    case StaticType::FLOAT: {
        float f;
        std::memcpy(&f, &value, sizeof(float));
        return fmt::format("FLOAT {:.6f}", f);
    }
    case StaticType::DOUBLE: {
        double d;
        std::memcpy(&d, &value, sizeof(double));
        return fmt::format("DOUBLE {:.10f}", d);
    }
    case StaticType::POINTER: return fmt::format("PTR 0x{:016X}", value);
    case StaticType::SID: {
        const char *str = StringIdManager::instance().get_cstring(value);
        if (str && str[0] != '\0') { return fmt::format("SID {} ({:016X})", str, value); }
        return fmt::format("SID {:016X}", value);
    }
    default: return fmt::format("unknown(0x{:X})", value);
    }
}

void BinaryFileInspector::inspect_script_lambda(const ScriptLambda *lambda,
                                                const std::string  &name) {
    disassemble(lambda, name);
}

void BinaryFileInspector::inspect_symbol(const symbol *sym) {
    m_formatter->print("Symbol: ID={}, Type={}\n", sid_str(sym->id), type_name(sym->type));

    IFormatter::Block block(*m_formatter, m_indent);

    switch (sym->type) {
    case symbol_type::B8:
        m_formatter->print("Value: {}\n", sym->b8_ptr ? *sym->b8_ptr : false);
        break;
    case symbol_type::I32:
        m_formatter->print("Value: {}\n", sym->i32_ptr ? *sym->i32_ptr : 0);
        break;
    case symbol_type::F32:
        m_formatter->print("Value: {:.6f}\n", sym->f32_ptr ? *sym->f32_ptr : 0.0f);
        break;
    case symbol_type::SS:
        if (sym->ss_ptr) { inspect_state_script(sym->ss_ptr); }
        break;
    case symbol_type::LAMBDA:
        if (sym->lambda_ptr) { inspect_script_lambda(sym->lambda_ptr); }
        break;
    default: break;
    }
}

std::string BinaryFileInspector::format_instruction(const Instruction     &ins,
                                                    const InstructionInfo *info,
                                                    const ScriptLambda    *lambda) {
    std::string result = info->name;

    switch (info->operands_count()) {
    case 1:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        break;
    case 2:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        if (info->b_type == OperandType::REG) {
            result += fmt::format(", r{}", ins.operand1);
        } else if (info->b_type == OperandType::IMM_U16) {
            u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            result += fmt::format(", {}", imm);
        }
        break;
    case 3:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        if (info->b_type == OperandType::REG) { result += fmt::format(", r{}", ins.operand1); }
        if (info->c_type == OperandType::REG) {
            result += fmt::format(", r{}", ins.operand2);
        } else if (info->c_type == OperandType::IMM_U8) {
            result += fmt::format(", {}", ins.operand2);
        }
        break;
    }

    if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt ||
        ins.opcode == Opcode::LookupFloat) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_symbol(idx, lambda));
    } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_float(idx, lambda));
    } else if (ins.opcode == Opcode::BranchIfNot || ins.opcode == Opcode::BranchIf ||
               ins.opcode == Opcode::Branch) {
        u16 target = static_cast<u16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
        result += fmt::format("  ; => L_{:X}", target);
    }

    return result;
}

std::string BinaryFileInspector::format_instruction(const ShortInstruction &ins,
                                                    const InstructionInfo  *info,
                                                    const ScriptLambda     *lambda) {
    std::string result = info->name;

    switch (info->operands_count()) {
    case 1:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        break;
    case 2:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        if (info->b_type == OperandType::REG) {
            result += fmt::format(", r{}", ins.operand1);
        } else if (info->b_type == OperandType::IMM_U16) {
            u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            result += fmt::format(", {}", imm);
        }
        break;
    case 3:
        if (info->a_type == OperandType::REG) { result += fmt::format(" r{}", ins.destination); }
        if (info->b_type == OperandType::REG) { result += fmt::format(", r{}", ins.operand1); }
        if (info->c_type == OperandType::REG) {
            result += fmt::format(", r{}", ins.operand2);
        } else if (info->c_type == OperandType::IMM_U8) {
            result += fmt::format(", {}", ins.operand2);
        }
        break;
    }

    if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt ||
        ins.opcode == Opcode::LookupFloat) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_symbol(idx, lambda));
    } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_float(idx, lambda));
    } else if (ins.opcode == Opcode::BranchIfNot || ins.opcode == Opcode::BranchIf ||
               ins.opcode == Opcode::Branch) {
        u16 target = static_cast<u16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
        result += fmt::format("  ; => L_{:X}", target);
    }

    return result;
}

} // namespace carbon