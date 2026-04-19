// BinaryFileInspector.cpp
#include "BinaryFileInspector.hpp"
#include "common/carbon/lib/StringId.hpp"
#include <iomanip>
#include <sstream>

namespace carbon {

BinaryFileInspector::BinaryFileInspector(BinaryFile& file, u16 indent)
    : m_file(file), m_indent(indent), m_formatter(util::Formatter::instance()) {}

std::string BinaryFileInspector::inspect() {
    std::string result;
    
    // Header
    result += m_formatter.format("=== Binary File: {} ===\n", m_file.m_path.string());
    result += inspect_header();
    
    // Relocation table
    result += m_formatter.format("\n--- Relocation Table ---\n");
    // TODO: implement relocation table inspection
    
    // String table
    result += m_formatter.format("\n--- String Table ---\n");
    for (const auto& [id, str] : m_file.m_sidCache) {
        result += m_formatter.format("  {}: {}\n", sid_str(id), str);
    }
    
    // Entries
    result += m_formatter.format("\n--- Entries ({} total) ---\n", m_file.m_dcheader->m_numEntries);
    {
        util::Formatter::Block block(m_formatter, m_indent);
        for (u32 i = 0; i < m_file.m_dcheader->m_numEntries; i++) {
            result += inspect_entry(m_file.m_dcheader->m_pStartOfData[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::ptr_str(const void* ptr) {
    if (!ptr) return "nullptr";
    std::stringstream ss;
    ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr);
    return ss.str();
}

std::string BinaryFileInspector::sid_str(sid64 id) {
    auto it = m_file.m_sidCache.find(id);
    if (it != m_file.m_sidCache.end()) {
        return fmt::format("{} [{}]", it->second, StringId(id).to_string());
    }
    return StringId(id).to_string();
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
    auto it = m_file.m_sidCache.find(sid);
    if (it != m_file.m_sidCache.end()) {
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
    if (!info) return "???";
    
    std::string result = info->name;
    
    switch (info->oprands_count()) {
        case 1:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" {}", reg_name(ins.destination));
            }
            break;
            
        case 2:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" {}", reg_name(ins.destination));
            } else if (info->a_type == OperandType::IMM_U16) {
                result += fmt::format(" {}", ins.destination);
            }
            
            if (info->b_type == OperandType::REG) {
                result += fmt::format(", {}", reg_name(ins.operand1));
            } else if (info->b_type == OperandType::IMM_U16) {
                u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
                result += fmt::format(", {}", imm);
            } else if (info->b_type == OperandType::IMM_I16) {
                i16 imm = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
                result += fmt::format(", {}", imm);
            }
            break;
            
        case 3:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" {}", reg_name(ins.destination));
            }
            if (info->b_type == OperandType::REG) {
                result += fmt::format(", {}", reg_name(ins.operand1));
            } else if (info->b_type == OperandType::IMM_U16) {
                result += fmt::format(", {}", ins.operand1);
            }
            if (info->c_type == OperandType::REG) {
                result += fmt::format(", {}", reg_name(ins.operand2));
            } else if (info->c_type == OperandType::IMM_U8) {
                result += fmt::format(", {}", ins.operand2);
            } else if (info->c_type == OperandType::IMM_I16) {
                i16 imm = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
                result += fmt::format(", {}", imm);
            }
            break;
    }
    
    // Add comments for special instructions
    if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt || ins.opcode == Opcode::LookupFloat) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_symbol(idx, lambda));
    } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
        u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
        result += fmt::format("  ; = {}", resolve_float(idx, lambda));
    } else if (ins.opcode == Opcode::BranchIfNot || ins.opcode == Opcode::BranchIf || ins.opcode == Opcode::Branch) {
        i16 offset = static_cast<i16>((static_cast<u16>(ins.operand2) << 8) | ins.operand1);
        result += fmt::format("  ; => L_{:X}", offset);
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_header() {
    auto* hdr = m_file.m_dcheader;
    if (!hdr) return "";
    
    std::string result;
    result += m_formatter.format("Magic: 0x{:08X} ({})\n", hdr->m_magic, 
                                  hdr->m_magic == DC_MAGIC ? "DC00" : "INVALID");
    result += m_formatter.format("Version: {}\n", hdr->m_versionNumber);
    result += m_formatter.format("Text Size: 0x{:X} ({} bytes)\n", hdr->m_textSize, hdr->m_textSize);
    result += m_formatter.format("Strings Offset: 0x{:X}\n", hdr->m_stringsOffset);
    result += m_formatter.format("Field 10: {}\n", hdr->field_10);
    result += m_formatter.format("Num Entries: {}\n", hdr->m_numEntries);
    result += m_formatter.format("Data Start: {}\n", ptr_str(hdr->m_pStartOfData));
    
    return result;
}

std::string BinaryFileInspector::inspect_entry(const Entry& entry) {
    std::string result;
    result += m_formatter.format("Entry:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("Name: {}\n", sid_str(entry.m_nameID));
    result += m_formatter.format("Type: {}\n", sid_str(entry.m_typeId));
    result += m_formatter.format("Ptr: {}\n", ptr_str(entry.m_entryPtr));
    
    // Try to inspect based on type
    if (entry.m_typeId == StringId("state-script").value) {
        auto* ss = static_cast<const StateScript*>(entry.m_entryPtr);
        result += inspect_state_script(*ss);
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_state_script(const StateScript& ss) {
    std::string result;
    result += m_formatter.format("StateScript:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("ID: {}\n", sid_str(ss.m_stateScriptId));
    result += m_formatter.format("Initial State: {}\n", sid_str(ss.m_initialStateId));
    result += m_formatter.format("State Count: {}\n", ss.m_stateCount);
    result += m_formatter.format("Line: {}\n", ss.m_line);
    result += m_formatter.format("Debug File: {}\n", ss.m_pDebugFileName ? ss.m_pDebugFileName : "(null)");
    result += m_formatter.format("Error Name: {}\n", ss.m_pErrorName ? ss.m_pErrorName : "(null)");
    
    if (ss.m_pSsDeclList) {
        result += inspect_declaration_list(*ss.m_pSsDeclList);
    }
    
    if (ss.m_pSsOptions) {
        result += inspect_options(*ss.m_pSsOptions);
    }
    
    if (ss.m_pSsStateTable && ss.m_stateCount > 0) {
        result += m_formatter.format("States:\n");
        util::Formatter::Block state_block(m_formatter, m_indent);
        for (i16 i = 0; i < ss.m_stateCount; i++) {
            result += inspect_state(ss.m_pSsStateTable[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_declaration_list(const SsDeclarationList& list) {
    std::string result;
    result += m_formatter.format("Declaration List:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("Total Size: {} bytes\n", list.m_totalDeclarationSize);
    result += m_formatter.format("Num Declarations: {}\n", list.m_numDeclarations);
    
    if (list.m_pDeclarations && list.m_numDeclarations > 0) {
        result += m_formatter.format("Declarations:\n");
        util::Formatter::Block decl_block(m_formatter, m_indent);
        for (u32 i = 0; i < list.m_numDeclarations; i++) {
            result += inspect_declaration(list.m_pDeclarations[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_declaration(const SsDeclaration& decl) {
    std::string result;
    result += m_formatter.format("Declaration:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("ID: {}\n", sid_str(decl.m_declId));
    result += m_formatter.format("Type: {}\n", sid_str(decl.m_declTypeId));
    result += m_formatter.format("Size: {} bytes\n", decl.m_varSizeSum);
    result += m_formatter.format("Is Var: {}\n", decl.m_isVar);
    result += m_formatter.format("Value Ptr: {}\n", ptr_str(decl.m_pDeclValue));
    
    return result;
}

std::string BinaryFileInspector::inspect_options(const SsOptions& opts) {
    std::string result;
    result += m_formatter.format("Options:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("Option String: {}\n", opts.m_optionString ? opts.m_optionString : "(null)");
    result += m_formatter.format("Unknown Flags: 0x{:X}\n", opts.m_unknownFlags);
    
    if (opts.m_pSymbolArray) {
        result += inspect_symbol_array(*opts.m_pSymbolArray, "SymbolArray");
    }
    if (opts.m_symbolArray2) {
        result += inspect_symbol_array(*opts.m_symbolArray2, "SymbolArray2");
    }
    if (opts.m_symbolArray3) {
        result += inspect_symbol_array(*opts.m_symbolArray3, "SymbolArray3");
    }
    if (opts.m_symbolArray4) {
        result += inspect_symbol_array(*opts.m_symbolArray4, "SymbolArray4");
    }
    
    result += m_formatter.format("Field 0x38: {}\n", opts.m_always5);
    result += m_formatter.format("Field 0x3C: {}\n", opts.m_mostly0);
    
    return result;
}

std::string BinaryFileInspector::inspect_symbol_array(const SymbolArray& arr, const std::string& name) {
    std::string result;
    result += m_formatter.format("{}:\n", name);
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("Num Entries: {}\n", arr.m_numEntries);
    result += m_formatter.format("Unknown: {}\n", arr.m_unk);
    
    if (arr.m_pSymbols && arr.m_numEntries > 0) {
        result += m_formatter.format("Symbols:\n");
        util::Formatter::Block sym_block(m_formatter, m_indent);
        for (u32 i = 0; i < arr.m_numEntries; i++) {
            result += m_formatter.format("[{}] {}\n", i, sid_str(arr.m_pSymbols[i]));
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_state(const SsState& state) {
    std::string result;
    result += m_formatter.format("State: {}\n", sid_str(state.m_stateId));
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    result += m_formatter.format("Num OnBlocks: {}\n", state.m_numSsOnBlocks);
    
    if (state.m_pSsOnBlocks && state.m_numSsOnBlocks > 0) {
        result += m_formatter.format("OnBlocks:\n");
        util::Formatter::Block onblock_block(m_formatter, m_indent);
        for (i64 i = 0; i < state.m_numSsOnBlocks; i++) {
            result += inspect_on_block(state.m_pSsOnBlocks[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_on_block(const SsOnBlock& block) {
    std::string result;
    std::string type_str;
    
    switch (block.m_blockType) {
        case BlockType::Start: type_str = "Start"; break;
        case BlockType::End: type_str = "End"; break;
        case BlockType::Event: type_str = "Event"; break;
        case BlockType::Update: type_str = "Update"; break;
        case BlockType::Virtual: type_str = "Virtual"; break;
        case BlockType::Code: type_str = "Code"; break;
        case BlockType::Exit: type_str = "Exit"; break;
        case BlockType::Post: type_str = "Post"; break;
        default: type_str = fmt::format("Unknown({})", static_cast<int>(block.m_blockType));
    }
    
    if (block.m_blockType == BlockType::Event) {
        result += m_formatter.format("OnBlock: {} ({})\n", type_str, sid_str(block.m_blockEventId));
    } else {
        result += m_formatter.format("OnBlock: {}\n", type_str);
    }
    
    util::Formatter::Block inner_block(m_formatter, m_indent);
    
    if (block.m_pScriptLambda) {
        result += inspect_script_lambda(*block.m_pScriptLambda, block.name());
    }
    
    // Track group inspection
    result += m_formatter.format("TrackGroup:\n");
    util::Formatter::Block track_block(m_formatter, m_indent);
    result += m_formatter.format("Name: {}\n", block.m_trackGroup.m_name ? block.m_trackGroup.m_name : "(null)");
    result += m_formatter.format("Num Tracks: {}\n", block.m_trackGroup.m_numTracks);
    
    if (block.m_trackGroup.m_aTracks && block.m_trackGroup.m_numTracks > 0) {
        result += m_formatter.format("Tracks:\n");
        util::Formatter::Block track_list_block(m_formatter, m_indent);
        for (i16 i = 0; i < block.m_trackGroup.m_numTracks; i++) {
            result += inspect_track(block.m_trackGroup.m_aTracks[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_track(const SsTrack& track) {
    std::string result;
    result += m_formatter.format("Track: {}\n", sid_str(track.m_trackId));
    
    util::Formatter::Block block(m_formatter, m_indent);
    result += m_formatter.format("Index: {}\n", track.m_trackIdx);
    result += m_formatter.format("Lambda Count: {}\n", track.m_totalLambdaCount);
    
    if (track.m_pSsLambda && track.m_totalLambdaCount > 0) {
        result += m_formatter.format("Lambdas:\n");
        util::Formatter::Block lambda_block(m_formatter, m_indent);
        for (i16 i = 0; i < track.m_totalLambdaCount; i++) {
            result += inspect_lambda(track.m_pSsLambda[i]);
        }
    }
    
    return result;
}

std::string BinaryFileInspector::inspect_lambda(const SsLambda& lambda) {
    std::string result;
    result += m_formatter.format("SsLambda:\n");
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    if (lambda.m_pScriptLambda) {
        result += inspect_script_lambda(*lambda.m_pScriptLambda);
    }
    result += m_formatter.format("Counter: 0x{:X}\n", lambda.m_someSortOfCounter);
    
    return result;
}

std::string BinaryFileInspector::disassemble(const ScriptLambda& lambda, const std::string& name) {
    std::string result;
    
    u32 num_ins = lambda.m_numInstructions;
    auto* code = lambda.get_code_ptr();
    auto* symbols = lambda.get_symbols_ptr();
    
    result += m_formatter.format("script-lambda {} {{\n", name.empty() ? "" : fmt::format("[{}] ", name));
    
    util::Formatter::Block block(m_formatter, m_indent);
    
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
        result += m_formatter.format("[{} args] ", args.size());
        bool first = true;
        for (u8 arg : args) {
            if (!first) result += ", ";
            result += fmt::format("arg_{}", arg - 32);
            first = false;
        }
        result += "\n";
    }
    result += "\n";
    
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
            result += m_formatter.format("L_{:X}:\n", i);
        }
        
        result += m_formatter.format("{:04X}   {:08X}   {:02X} {:02X} {:02X}   {:<20}",
            i,
            reinterpret_cast<uintptr_t>(&code[i]),
            static_cast<u8>(ins.opcode),
            ins.destination,
            ins.operand1,
            ins.operand2,
            format_instruction(ins, &lambda));
        
        // Add comment with resolved values
        if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt || ins.opcode == Opcode::LookupFloat) {
            u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            result += fmt::format("  ; {}", resolve_symbol(idx, &lambda));
        } else if (ins.opcode == Opcode::LoadStaticFloatImm) {
            u16 idx = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
            result += fmt::format("  ; = {}", resolve_float(idx, &lambda));
        } else if (ins.opcode == Opcode::Move && ins.destination == 49) {
            result += fmt::format("  ; saving result");
        } else if (ins.opcode == Opcode::Call || ins.opcode == Opcode::CallFf) {
            result += fmt::format("  ; call with {} args", ins.operand2);
        }
        
        result += "\n";
    }
    
    // Symbol table
    if (symbols && num_ins > 0) {
        result += m_formatter.format("\nSYMBOL TABLE:\n");
        util::Formatter::Block sym_block(m_formatter, m_indent);
        
        for (u32 i = 0; i < num_ins; i++) {
            sid64 sid = symbols[i];
            auto it = m_file.m_sidCache.find(sid);
            if (it != m_file.m_sidCache.end()) {
                result += m_formatter.format("{:04X}   {:08X}   {}: {}\n", 
                    i, reinterpret_cast<uintptr_t>(&symbols[i]), it->second, sid_str(sid));
            } else {
                result += m_formatter.format("{:04X}   {:08X}   {}\n", 
                    i, reinterpret_cast<uintptr_t>(&symbols[i]), sid_str(sid));
            }
        }
    }
    
    result += m_formatter.format("}}\n\n");
    
    return result;
}

std::string BinaryFileInspector::inspect_script_lambda(const ScriptLambda& lambda, const std::string& name) {
    return disassemble(lambda, name);
}

std::string BinaryFileInspector::inspect_symbol(const symbol& sym) {
    std::string result;
    result += m_formatter.format("Symbol: ID={}, Type={}\n", sid_str(sym.id), type_name(sym.type));
    
    util::Formatter::Block block(m_formatter, m_indent);
    
    switch (sym.type) {
        case symbol_type::B8:
            result += m_formatter.format("Value: {}\n", sym.b8_ptr ? *sym.b8_ptr : false);
            break;
        case symbol_type::I32:
            result += m_formatter.format("Value: {}\n", sym.i32_ptr ? *sym.i32_ptr : 0);
            break;
        case symbol_type::F32:
            result += m_formatter.format("Value: {:.6f}\n", sym.f32_ptr ? *sym.f32_ptr : 0.0f);
            break;
        case symbol_type::SS:
            if (sym.ss_ptr) {
                result += inspect_state_script(*sym.ss_ptr);
            }
            break;
        case symbol_type::LAMBDA:
            if (sym.lambda_ptr) {
                result += inspect_script_lambda(*sym.lambda_ptr);
            }
            break;
        default:
            break;
    }
    
    return result;
}

} // namespace carbon