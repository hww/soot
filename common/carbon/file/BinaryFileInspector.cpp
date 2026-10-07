// BinaryFileInspector.cpp
#include "BinaryFileInspector.hpp"
#include "common/carbon/file/DeclarationTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/carbon/file/DCScript.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "util/Formatter.hpp"
#include "vm/Instructions.hpp"
#include <bitset>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>

using namespace util;
using namespace carbon;

namespace {

    /// @brief Register all well-known type names in the global StringIdManager,
    ///        so that sid_str() can resolve them.
    /// @details Runs once per process. Covers:
    ///            - SsDeclaration type names (mirrors KNOWN_DECL_TYPES)
    ///            - DCEntry type names
    ///            - SsState block-type names
    ///            - Common SIDs used throughout the format
    void register_known_decl_types() {
        static bool done = false;
        if (done) { return; }
        done = true;

        auto &mgr = StringIdManager::instance();

        // --- SsDeclaration types ---
        mgr.register_string("boolean");
        mgr.register_string("int32");
        mgr.register_string("uint64");
        mgr.register_string("float");
        mgr.register_string("timer");
        mgr.register_string("bound-frame");
        mgr.register_string("symbol");
        mgr.register_string("string");
        mgr.register_string("vector");
        mgr.register_string("quat");
        mgr.register_string("point");

        // --- DCEntry types ---
        mgr.register_string("script-lambda");
        mgr.register_string("state-script");
        mgr.register_string("map");
        mgr.register_string("map-32");
        mgr.register_string("vector3");
        mgr.register_string("vector4");
        mgr.register_string("transform");
        mgr.register_string("matrix");
        mgr.register_string("quaternion");
        mgr.register_string("locator");
        mgr.register_string("color");

        // --- BlockType names (SsOnBlock.m_blockType) ---
        mgr.register_string("start");
        mgr.register_string("end");
        mgr.register_string("event");
        mgr.register_string("update");
        mgr.register_string("virtual");
        mgr.register_string("code");
        mgr.register_string("exit");
        mgr.register_string("post");

        // --- Common SIDs used throughout the format ---
        mgr.register_string("array");
        mgr.register_string("global");
        mgr.register_string("none");
        mgr.register_string("unknown");
        mgr.register_string("unnamed");

        // --- State-script variable type SIDs (used by SsDeclaration.m_declTypeId) ---
        mgr.register_string("vector3");
        mgr.register_string("vector2");
        mgr.register_string("vector4");
        mgr.register_string("color");
        mgr.register_string("matrix");
        mgr.register_string("transform");
    }

} // namespace

namespace {

    /// @brief Escape a string for JSON.
    std::string json_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += fmt::format("\\u{:04X}", static_cast<u8>(c));
                } else {
                    out += c;
                }
                break;
            }
        }
        return out;
    }

    /// @brief Write a JSON string literal (with quotes).
    void write_json_string(std::ostream &os, const std::string &s) {
        os << '"' << json_escape(s) << '"';
    }

    const char *kind_to_json(BinaryFile::EntryKind kind) {
        switch (kind) {
        case BinaryFile::EntryKind::ScriptLambda: return "script-lambda";
        case BinaryFile::EntryKind::StateScript: return "state-script";
        case BinaryFile::EntryKind::SsType: return "ss-type";
        case BinaryFile::EntryKind::Map: return "map";
        case BinaryFile::EntryKind::DataStruct: return "data-struct";
        case BinaryFile::EntryKind::Unknown: return "unknown";
        }
        return "unknown";
    }

} // namespace


namespace carbon {

    BinaryFileInspector::BinaryFileInspector(BinaryFile *file, int indent, InspectMode mode)
        : m_file(file), m_indent(indent), m_mode(mode),
          m_formatter(std::make_unique<OutputFormatter>()) {
        register_known_decl_types();
    }

    void BinaryFileInspector::inspect() {
        m_formatter->print("=== Binary File: {} ===\n", m_file->m_path.string());

        // Bird's-eye view first.
        m_formatter->print("\n--- Overview ---\n");
        inspect_overview();

        m_formatter->print("\n--- Header ---\n");
        inspect_header();

        inspect_relocations(64);

        m_formatter->print("\n--- String Table ---\n");
        for (const auto &[id, str] : m_file->m_sidCache) {
            m_formatter->print("  0x{:016X}: {}\n", id, str);
        }

        const u32 n = m_file->entry_count();

        m_formatter->print("\n--- Entries ({} total) ---\n", n);
        inspect_entry_summary();
        // Printed after the entry table so the reader sees metadata first.
        inspect_data_structs();

        if (m_mode == InspectMode::Full) {
            const DCEntry *table = m_file->entries();
            if (table) {
                m_formatter->print("\n--- Entry details ---\n");
                IFormatter::Block block(*m_formatter, m_indent);
                for (u32 i = 0; i < n; ++i) { inspect_entry(&table[i]); }
            }
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
        m_formatter->print("  m_textSize: {} bytes (0x{:X})\n", header->m_textSize,
                           header->m_textSize);
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

    /// @brief Resolve a SID64 to a human-readable string.
    /// @details First checks the well-known SIDs (which may be interned per
    ///          translation unit and therefore not present in the global
    ///          StringIdManager), then falls back to StringIdManager, then
    ///          to a hex representation.
    std::string BinaryFileInspector::sid_str(sid64 id) {
        if (id == 0) return "(null)";

        // --- Well-known SIDs, matched by identity.
        //     This matters because SID("...") is not guaranteed to be the same
        //     integer across translation units.
        if (id == SS_TYPE_SID) return "ss-type";
        if (id == SCRIPT_LAMBDA_SID) return "script-lambda";
        if (id == ARRAY_SID) return "array";
        if (id == GLOBAL_SID) return "global";
        if (id == FUNCTION_SID) return "function";

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
        if (reg >= ARG_REGISTERS_OFFSET) {
            return fmt::format("arg_{}", reg - ARG_REGISTERS_OFFSET);
        }
        return fmt::format("r{}", reg);
    }

    std::string BinaryFileInspector::resolve_symbol(u16 index, const ScriptLambda *lambda) {
        if (!lambda || !lambda->m_pSymbols) return fmt::format("ST[{}]", index);
        if (index >= lambda->m_numSymbols) {
            return fmt::format("ST[{}] (out of bounds, m_numSymbols={})", index,
                               lambda->m_numSymbols);
        }

        u64  *symbols = lambda->m_pSymbols;
        sid64 sid = symbols[index];
        auto  it = m_file->m_sidCache.find(sid);
        if (it != m_file->m_sidCache.end()) {
            return fmt::format("{} [{}]", it->second, sid_str(sid));
        }
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

    std::string BinaryFileInspector::format_instruction(const Instruction  &ins,
                                                        const ScriptLambda *lambda) {
        auto *info = get_instruction_info(ins.opcode);
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
        if (ins.opcode == Opcode::LookupPointer || ins.opcode == Opcode::LookupInt ||
            ins.opcode == Opcode::LookupFloat) {
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

    void BinaryFileInspector::inspect_entry(const DCEntry *entry) {
        if (!entry) {
            m_formatter->print("Entry: NULL\n");
            return;
        }

        m_formatter->print("Entry:\n");

        IFormatter::Block block(*m_formatter, m_indent);
        m_formatter->print("Address: {:p}\n", (void *)entry);
        m_formatter->print("Name: {}\n", sid_str(entry->m_nameID));
        m_formatter->print("Type: {}\n", sid_str(entry->m_typeId));
        m_formatter->print("Ptr:  {}\n", ptr_str(entry->m_entryPtr));

        if (entry->m_entryPtr == nullptr || entry->m_typeId == 0) { return; }

        try {
            switch (BinaryFile::entry_kind(*entry)) {
            case BinaryFile::EntryKind::StateScript: {
                if (const auto *ss = m_file->entry_as_state_script(*entry)) {
                    inspect_state_script(ss);
                }
                break;
            }
            case BinaryFile::EntryKind::ScriptLambda: {
                if (const auto *sl = m_file->entry_as_lambda(*entry)) { inspect_script_lambda(sl); }
                break;
            }
            case BinaryFile::EntryKind::SsType: {
                if (const auto *st = m_file->entry_as_ss_type(*entry)) { inspect_ss_type(st); }
                break;
            }
            case BinaryFile::EntryKind::Map:
            case BinaryFile::EntryKind::DataStruct: {
                // Opaque payload: print the first 64 bytes as raw hex.
                inspect_raw_payload(entry, 64);
                break;
            }
            case BinaryFile::EntryKind::Unknown: break;
            }
        } catch (const std::exception &e) {
            m_formatter->print("Error inspecting entry: {}\n", e.what());
        }
    }

    void BinaryFileInspector::inspect_raw_payload(const DCEntry *entry, u32 max_bytes) {
        if (!entry || !entry->m_entryPtr) { return; }

        // Compute how many bytes are safe to read.
        const auto *base = reinterpret_cast<const u8 *>(m_file->m_bytes.get());
        const auto *ptr = reinterpret_cast<const u8 *>(entry->m_entryPtr);
        const auto *end = base + m_file->m_size;

        if (ptr < base || ptr >= end) {
            m_formatter->print("Payload: (out of bounds)\n");
            return;
        }

        const u64 available = static_cast<u64>(end - ptr);
        const u32 n = static_cast<u32>(std::min<u64>(available, max_bytes));

        m_formatter->print("Payload (first {} bytes):\n", n);

        constexpr u32 BYTES_PER_LINE = 16;
        for (u32 i = 0; i < n; i += BYTES_PER_LINE) {
            m_formatter->print("  {:04X}  ", i);

            // Hex bytes.
            for (u32 j = 0; j < BYTES_PER_LINE; ++j) {
                if (i + j < n) {
                    m_formatter->print("{:02X} ", ptr[i + j]);
                } else {
                    m_formatter->print("   ");
                }
                if (j == 7) {
                    m_formatter->print(" "); // extra space in the middle
                }
            }

            m_formatter->print(" |");

            // ASCII characters.
            for (u32 j = 0; j < BYTES_PER_LINE && i + j < n; ++j) {
                const u8 c = ptr[i + j];
                m_formatter->print("{}", (c >= 32 && c < 127) ? static_cast<char>(c) : '.');
            }

            m_formatter->print("|\n");
        }

        if (available > n) { m_formatter->print("  ... ({} more bytes)\n", available - n); }
    }

    void BinaryFileInspector::inspect_entry_summary() {
        const DCEntry *table = m_file->entries();
        const u32      n = m_file->entry_count();

        if (!table || n == 0) {
            m_formatter->print("(no entries)\n");
            return;
        }

        m_formatter->print("{:>4}  {:<32}  {:<16}  {:<14}  {}\n", "idx", "name", "type", "kind",
                           "offset");

        for (u32 i = 0; i < n; ++i) {
            const DCEntry &e = table[i];

            const std::string name = sid_str(e.m_nameID);
            const std::string type = sid_str(e.m_typeId);

            const char *kind_str = "unknown";
            switch (BinaryFile::entry_kind(e)) {
            case BinaryFile::EntryKind::ScriptLambda: kind_str = "script-lambda"; break;
            case BinaryFile::EntryKind::StateScript: kind_str = "state-script"; break;
            case BinaryFile::EntryKind::SsType: kind_str = "ss-type"; break;
            case BinaryFile::EntryKind::Map: kind_str = "map"; break;
            case BinaryFile::EntryKind::DataStruct: kind_str = "data-struct"; break;
            case BinaryFile::EntryKind::Unknown: kind_str = "unknown"; break;
            }

            // Compute the payload offset from the start of the file.
            u64 offset = 0;
            if (e.m_entryPtr) {
                const auto base = reinterpret_cast<uintptr_t>(m_file->m_bytes.get());
                const auto addr = reinterpret_cast<uintptr_t>(e.m_entryPtr);
                if (addr >= base) { offset = addr - base; }
            }

            m_formatter->print("{:>4}  {:<32}  {:<16}  {:<14}  0x{:08X}\n", i, name, type, kind_str,
                               offset);
        }
    }

    void BinaryFileInspector::inspect_overview() {
        const DCEntry *table = m_file->entries();
        const u32      n = m_file->entry_count();

        if (!table || n == 0) {
            m_formatter->print("(empty file)\n");
            return;
        }

        // Count entries by kind.
        u32 n_lambdas = 0;
        u32 n_states = 0;
        u32 n_types = 0;
        u32 n_maps = 0;
        u32 n_data = 0;
        u32 n_unknown = 0;

        for (u32 i = 0; i < n; ++i) {
            switch (BinaryFile::entry_kind(table[i])) {
            case BinaryFile::EntryKind::ScriptLambda: ++n_lambdas; break;
            case BinaryFile::EntryKind::StateScript: ++n_states; break;
            case BinaryFile::EntryKind::SsType: ++n_types; break;
            case BinaryFile::EntryKind::Map: ++n_maps; break;
            case BinaryFile::EntryKind::DataStruct: ++n_data; break;
            case BinaryFile::EntryKind::Unknown: ++n_unknown; break;
            }
        }

        m_formatter->print("Overview:\n");
        m_formatter->print("  total entries:    {}\n", n);
        m_formatter->print("  script-lambdas:   {}\n", n_lambdas);
        m_formatter->print("  state-scripts:    {}\n", n_states);
        m_formatter->print("  ss-types:         {}\n", n_types);
        m_formatter->print("  maps:             {}\n", n_maps);
        m_formatter->print("  data structs:     {}\n", n_data);
        m_formatter->print("  unknown:          {}\n", n_unknown);

        // Print names grouped by kind.
        auto print_group = [&](const char *header, BinaryFile::EntryKind kind) {
            bool first = true;
            for (u32 i = 0; i < n; ++i) {
                if (BinaryFile::entry_kind(table[i]) != kind) { continue; }
                if (first) {
                    m_formatter->print("\n  {}:\n", header);
                    first = false;
                }
                m_formatter->print("    [{}] {}\n", i, sid_str(table[i].m_nameID));
            }
        };

        print_group("script-lambdas", BinaryFile::EntryKind::ScriptLambda);
        print_group("state-scripts", BinaryFile::EntryKind::StateScript);
        print_group("ss-types", BinaryFile::EntryKind::SsType);
        print_group("maps", BinaryFile::EntryKind::Map);
        print_group("data-structs", BinaryFile::EntryKind::DataStruct);
        print_group("unknown", BinaryFile::EntryKind::Unknown);
    }

    void BinaryFileInspector::inspect_type_summary() {
        using SidCountMap = std::map<sid64, u32>;
        const DCEntry *table = m_file->entries();
        const u32      n = m_file->entry_count();

        if (!table || n == 0) {
            m_formatter->print("(no entries)\n");
            return;
        }

        SidCountMap entry_types;
        SidCountMap decl_types;
        SidCountMap block_types;
        SidCountMap lambda_func_names;

        for (u32 i = 0; i < n; ++i) {
            const DCEntry &e = table[i];

            // Entry type.
            if (e.m_typeId != 0) { entry_types[e.m_typeId]++; }

            // State-script: walk declarations and blocks.
            if (BinaryFile::entry_kind(e) == BinaryFile::EntryKind::StateScript) {
                const auto *ss = m_file->entry_as_state_script(e);
                if (!ss) continue;

                // Declarations.
                if (ss->m_pSsDeclList && ss->m_pSsDeclList->m_pDeclarations) {
                    for (u32 j = 0; j < ss->m_pSsDeclList->m_numDeclarations; ++j) {
                        const auto &d = ss->m_pSsDeclList->m_pDeclarations[j];
                        if (d.m_declTypeId != 0) { decl_types[d.m_declTypeId]++; }
                    }
                }

                // Blocks.
                if (ss->m_pSsStateTable) {
                    for (i16 k = 0; k < ss->m_stateCount; ++k) {
                        const auto &st = ss->m_pSsStateTable[k];
                        if (!st.m_pSsOnBlocks) continue;
                        for (i64 b = 0; b < st.m_numSsOnBlocks; ++b) {
                            const auto &blk = st.m_pSsOnBlocks[b];
                            // BlockType is an enum, not a SID — convert to a pseudo-SID.
                            const sid64 bt = static_cast<sid64>(blk.m_blockType);
                            block_types[bt]++;
                        }
                    }
                }
            }
        }

        // Now print.
        m_formatter->print("Type summary:\n");

        if (!entry_types.empty()) {
            m_formatter->print("\n  Entry types ({} distinct):\n", entry_types.size());
            for (const auto &[sid, count] : entry_types) {
                m_formatter->print("    {:<32}  x{}\n", sid_str(sid), count);
            }
        }

        if (!decl_types.empty()) {
            m_formatter->print("\n  Declaration types ({} distinct):\n", decl_types.size());
            for (const auto &[sid, count] : decl_types) {
                m_formatter->print("    {:<32}  x{}\n", sid_str(sid), count);
            }
        }

        if (!block_types.empty()) {
            m_formatter->print("\n  Block types ({} distinct):\n", block_types.size());
            for (const auto &[v, count] : block_types) {
                const char *name = "unknown";
                switch (static_cast<BlockType>(v)) {
                case BlockType::Start: name = "start"; break;
                case BlockType::End: name = "end"; break;
                case BlockType::Event: name = "event"; break;
                case BlockType::Update: name = "update"; break;
                case BlockType::Virtual: name = "virtual"; break;
                case BlockType::Code: name = "code"; break;
                case BlockType::Exit: name = "exit"; break;
                case BlockType::Post: name = "post"; break;
                }
                m_formatter->print("    {:<32}  x{}\n", name, count);
            }
        }
    }

    void BinaryFileInspector::inspect_json(std::ostream &os) {
        const auto    *hdr = m_file->m_dcheader;
        const DCEntry *table = m_file->entries();
        const u32      n = m_file->entry_count();

        os << "{\n";

        // Path and size.
        os << "  \"path\": ";
        write_json_string(os, m_file->m_path.string());
        os << ",\n";
        os << "  \"size\": " << m_file->m_size << ",\n";

        // Header.
        os << "  \"header\": {\n";
        if (hdr) {
            os << "    \"magic\": \"0x" << fmt::format("{:08X}", hdr->m_magic) << "\",\n";
            os << "    \"version\": " << hdr->m_versionNumber << ",\n";
            os << "    \"textSize\": " << hdr->m_textSize << ",\n";
            os << "    \"stringsOffset\": " << hdr->m_stringsOffset << ",\n";
            os << "    \"always1\": " << hdr->m_always1 << ",\n";
            os << "    \"numEntries\": " << hdr->m_numEntries << "\n";
        } else {
            os << "    \"valid\": false\n";
        }
        os << "  },\n";

        // Entries.
        os << "  \"entries\": [\n";
        if (table) {
            for (u32 i = 0; i < n; ++i) {
                const DCEntry &e = table[i];

                u64 offset = 0;
                if (e.m_entryPtr) {
                    const auto base = reinterpret_cast<uintptr_t>(m_file->m_bytes.get());
                    const auto addr = reinterpret_cast<uintptr_t>(e.m_entryPtr);
                    if (addr >= base) { offset = addr - base; }
                }

                os << "    {\n";
                os << "      \"index\": " << i << ",\n";
                os << "      \"name\": ";
                write_json_string(os, sid_str(e.m_nameID));
                os << ",\n";
                os << "      \"type\": ";
                write_json_string(os, sid_str(e.m_typeId));
                os << ",\n";
                os << "      \"kind\": ";
                write_json_string(os, kind_to_json(BinaryFile::entry_kind(e)));
                os << ",\n";
                os << "      \"offset\": " << offset << "\n";
                os << "    }";
                if (i + 1 < n) { os << ","; }
                os << "\n";
            }
        }
        os << "  ],\n";

        // Violations.
        os << "  \"violations\": [\n";
        const auto violations = m_file->validate();
        for (size_t i = 0; i < violations.size(); ++i) {
            os << "    ";
            write_json_string(os, violations[i]);
            if (i + 1 < violations.size()) { os << ","; }
            os << "\n";
        }
        os << "  ]\n";

        os << "}\n";
    }

    void BinaryFileInspector::inspect_ss_type(const SsType *st) {
        if (!st) {
            m_formatter->print("SsType: NULL\n");
            return;
        }

        m_formatter->print("SsType:\n");
        IFormatter::Block block(*m_formatter, m_indent);

        m_formatter->print("Name:       {}\n", sid_str(st->m_name));
        m_formatter->print("Parent:     {}\n", st->m_parent ? sid_str(st->m_parent) : "(none)");
        m_formatter->print("Size:       {} bytes\n", st->m_size);
        m_formatter->print("Align:      {} bytes\n", st->m_align);
        m_formatter->print("NumFields:  {}\n", st->m_numFields);
        m_formatter->print("NumMethods: {}\n", st->m_numMethods);

        std::string flags;
        if (st->m_flags & 0x1) { flags += "basic "; }
        if (st->m_flags & 0x2) { flags += "structure "; }
        if (flags.empty()) { flags = "(none)"; }
        m_formatter->print("Flags:      0x{:X} ({})\n", st->m_flags, flags);

        // Fields.
        if (st->m_pFields && st->m_numFields > 0) {
            m_formatter->print("\nFields ({}):\n", st->m_numFields);
            IFormatter::Block field_block(*m_formatter, m_indent);

            m_formatter->print("{:>4}  {:<24}  {:<16}  {:>8}  {:>6}  {}\n", "idx", "name", "type",
                               "offset", "size", "flags");

            for (u32 i = 0; i < st->m_numFields; ++i) {
                const SsField &f = st->m_pFields[i];

                std::string fflags;
                if (f.m_flags & 0x1) { fflags += "inline "; }
                if (f.m_flags & 0x2) { fflags += "dynamic "; }
                if (f.m_flags & 0x4) { fflags += "array[" + std::to_string(f.m_count) + "] "; }
                if (fflags.empty()) { fflags = "-"; }

                m_formatter->print("{:>4}  {:<24}  {:<16}  {:>8}  {:>6}  {}\n", i,
                                   sid_str(f.m_name), sid_str(f.m_type), f.m_offset, f.m_size,
                                   fflags);
            }
        }

        // Methods (VTable).
        if (st->m_pMethods && st->m_numMethods > 0) {
            const auto *methods = reinterpret_cast<const SsMethod *>(st->m_pMethods);

            m_formatter->print("\nMethods ({}):\n", st->m_numMethods);
            IFormatter::Block method_block(*m_formatter, m_indent);

            m_formatter->print("{:>4}  {:<24}  {:<24}  {}\n", "id", "name", "full-name", "lambda");

            for (u32 i = 0; i < st->m_numMethods; ++i) {
                const SsMethod &m = methods[i];

                std::string lambda_str = "(null)";
                if (m.m_pLambda != nullptr) {
                    const auto base = reinterpret_cast<uintptr_t>(m_file->m_bytes.get());
                    const auto addr = reinterpret_cast<uintptr_t>(m.m_pLambda);
                    if (addr >= base && addr < base + m_file->m_size) {
                        lambda_str = fmt::format("0x{:04X}", addr - base);
                    } else {
                        lambda_str = fmt::format("{}", ptr_str(m.m_pLambda));
                    }
                }

                m_formatter->print("{:>4}  {:<24}  {:<24}  {}\n", i, sid_str(m.m_name),
                                   sid_str(m.m_fullName), lambda_str);
            }
        }
    }

    void BinaryFileInspector::inspect_state_script(const StateScript *ss) {
        if (!ss) {
            m_formatter->print("StateScript: NULL\n");
            return;
        }

        // Summary first.
        inspect_state_script_summary(ss);

        // Declarations.
        m_formatter->print("\n");
        inspect_declaration_list(ss->m_pSsDeclList);

        // Options.
        m_formatter->print("\n");
        inspect_options(ss->m_pSsOptions);

        // States.
        if (ss->m_pSsStateTable && ss->m_stateCount > 0) {
            m_formatter->print("\nStates ({}):\n", ss->m_stateCount);
            inspect_state_summary(ss);

            if (m_mode == InspectMode::Full) {
                m_formatter->print("\nState details:\n");
                inspect_state_detail_summary(ss);
            }
        }
    }

    void BinaryFileInspector::inspect_state_script_summary(const StateScript *ss) {
        if (!ss) {
            m_formatter->print("(null state-script)\n");
            return;
        }

        const std::string id = sid_str(ss->m_stateScriptId);
        const std::string initial = sid_str(ss->m_initialStateId);

        const u32 num_decls = ss->m_pSsDeclList ? ss->m_pSsDeclList->m_numDeclarations : 0;
        const u32 num_states = static_cast<u32>(ss->m_stateCount);

        // Collect the option list, if any.
        std::string options;
        if (ss->m_pSsOptions && ss->m_pSsOptions->m_pSymbolArray) {
            const SymbolArray *arr = ss->m_pSsOptions->m_pSymbolArray;
            for (u32 i = 0; i < arr->m_numEntries; ++i) {
                if (!options.empty()) { options += ", "; }
                options += sid_str(arr->m_pSymbols[i]);
            }
        }

        m_formatter->print("StateScript {}:\n", id);
        m_formatter->print("  initial state:    {}\n", initial);
        m_formatter->print("  declarations:     {}\n", num_decls);
        m_formatter->print("  states:           {}\n", num_states);
        m_formatter->print("  options:          {}\n", options.empty() ? "(none)" : options);
        m_formatter->print("  line:             {}\n", ss->m_line);
        m_formatter->print("  debug file:       {}\n",
                           ss->m_pDebugFileName ? ss->m_pDebugFileName : "(null)");
        m_formatter->print("  error name:       {}\n",
                           ss->m_pErrorName ? ss->m_pErrorName : "(null)");
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

        if (!list->m_pDeclarations || list->m_numDeclarations == 0) { return; }

        if (m_mode == InspectMode::Summary) {
            m_formatter->print("\n");
            inspect_declaration_summary(list);
        } else {
            for (u32 i = 0; i < list->m_numDeclarations; ++i) {
                inspect_declaration(&list->m_pDeclarations[i]);
            }
        }
    }

    void BinaryFileInspector::inspect_declaration(const SsDeclaration *decl) {
        if (!decl) {
            m_formatter->print("Declaration: NULL\n");
            return;
        }

        m_formatter->print("Declaration:\n");

        IFormatter::Block block(*m_formatter, m_indent);

        m_formatter->print("ID:        {}\n", sid_str(decl->m_declId));
        m_formatter->print("Type:      {}\n", sid_str(decl->m_declTypeId));
        m_formatter->print("Size:      {} bytes\n", decl->m_varSizeSum);
        m_formatter->print("Is Var:    {}\n", decl->m_isVar);
        m_formatter->print("Value Ptr: {}\n", ptr_str(decl->m_pDeclValue));

        if (decl->m_pDeclValue == nullptr) {
            m_formatter->print("Value:     nullptr\n");
            return;
        }

        const sid64 type = decl->m_declTypeId;

        // Print the value according to the declaration type SID.
        // Unknown types are printed as ??? plus the first 16 bytes as hex,
        // so that new types can be reverse-engineered manually.
        if (type == SID("boolean")) {
            const u8 v = *reinterpret_cast<const u8 *>(decl->m_pDeclValue);
            m_formatter->print("Value:     {}\n", v ? "true" : "false");
        } else if (type == SID("int32")) {
            m_formatter->print("Value:     {}\n",
                               *reinterpret_cast<const i32 *>(decl->m_pDeclValue));
        } else if (type == SID("uint64")) {
            m_formatter->print("Value:     {}\n",
                               *reinterpret_cast<const u64 *>(decl->m_pDeclValue));
        } else if (type == SID("float")) {
            m_formatter->print("Value:     {:.2f}\n",
                               *reinterpret_cast<const f32 *>(decl->m_pDeclValue));
        } else if (type == SID("timer")) {
            m_formatter->print("Value:     {:.2f}\n",
                               *reinterpret_cast<const f32 *>(decl->m_pDeclValue));
        } else if (type == SID("bound-frame")) {
            m_formatter->print("Value:     {:.2f}\n",
                               *reinterpret_cast<const f32 *>(decl->m_pDeclValue));
        } else if (type == SID("symbol")) {
            const sid64 value = *reinterpret_cast<const sid64 *>(decl->m_pDeclValue);
            m_formatter->print("Value:     {}\n", sid_str(value));
        } else if (type == SID("string")) {
            const char *str = *reinterpret_cast<const char *const *>(decl->m_pDeclValue);
            m_formatter->print("Value:     \"{}\"\n", str ? str : "");
        } else if (type == SID("vector")) {
            const f32 *v = reinterpret_cast<const f32 *>(decl->m_pDeclValue);
            m_formatter->print("Value:     ({:.2f}, {:.2f}, {:.2f}, {:.2f})\n", v[0], v[1], v[2],
                               v[3]);
        } else if (type == SID("quat")) {
            const f32 *v = reinterpret_cast<const f32 *>(decl->m_pDeclValue);
            m_formatter->print("Value:     ({:.2f}, {:.2f}, {:.2f}, {:.2f})\n", v[0], v[1], v[2],
                               v[3]);
        } else if (type == SID("point")) {
            const f32 *v = reinterpret_cast<const f32 *>(decl->m_pDeclValue);
            m_formatter->print("Value:     ({:.2f}, {:.2f}, {:.2f})\n", v[0], v[1], v[2]);
        } else {
            // Unknown type: print as ??? and dump the first 16 bytes as hex.
            m_formatter->print("Value:     ??? (unknown type {})\n", sid_str(type));

            const u8 *raw = reinterpret_cast<const u8 *>(decl->m_pDeclValue);
            m_formatter->print("Raw:       ");
            for (int i = 0; i < 16; ++i) { m_formatter->print("{:02X} ", raw[i]); }
            m_formatter->print("\n");
        }
    }

    void BinaryFileInspector::inspect_declaration_summary(const SsDeclarationList *list) {
        if (!list || !list->m_pDeclarations) {
            m_formatter->print("(no declarations)\n");
            return;
        }

        m_formatter->print("{:>4}  {:<32}  {:<12}  {:>5}  {}\n", "idx", "name", "type", "size",
                           "value");

        for (u32 i = 0; i < list->m_numDeclarations; ++i) {
            const SsDeclaration &d = list->m_pDeclarations[i];

            const std::string name = sid_str(d.m_declId);
            const std::string type = sid_str(d.m_declTypeId);

            std::string value;
            if (d.m_pDeclValue == nullptr) {
                value = "nullptr";
            } else if (d.m_declTypeId == SID("boolean")) {
                const u8 v = *reinterpret_cast<const u8 *>(d.m_pDeclValue);
                value = v ? "true" : "false";
            } else if (d.m_declTypeId == SID("int32")) {
                value = fmt::format("{}", *reinterpret_cast<const i32 *>(d.m_pDeclValue));
            } else if (d.m_declTypeId == SID("uint64")) {
                value = fmt::format("{}", *reinterpret_cast<const u64 *>(d.m_pDeclValue));
            } else if (d.m_declTypeId == SID("float")) {
                value = fmt::format("{:.2f}", *reinterpret_cast<const f32 *>(d.m_pDeclValue));
            } else if (d.m_declTypeId == SID("timer")) {
                value = fmt::format("{:.2f}", *reinterpret_cast<const f32 *>(d.m_pDeclValue));
            } else if (d.m_declTypeId == SID("bound-frame")) {
                value = fmt::format("{:.2f}", *reinterpret_cast<const f32 *>(d.m_pDeclValue));
            } else if (d.m_declTypeId == SID("symbol")) {
                const sid64 s = *reinterpret_cast<const sid64 *>(d.m_pDeclValue);
                value = sid_str(s);
            } else if (d.m_declTypeId == SID("string")) {
                const char *str = *reinterpret_cast<const char *const *>(d.m_pDeclValue);
                value = fmt::format("\"{}\"", str ? str : "");
            } else if (d.m_declTypeId == SID("vector")) {
                const f32 *v = reinterpret_cast<const f32 *>(d.m_pDeclValue);
                value = fmt::format("({:.2f}, {:.2f}, {:.2f}, {:.2f})", v[0], v[1], v[2], v[3]);
            } else if (d.m_declTypeId == SID("quat")) {
                const f32 *v = reinterpret_cast<const f32 *>(d.m_pDeclValue);
                value = fmt::format("({:.2f}, {:.2f}, {:.2f}, {:.2f})", v[0], v[1], v[2], v[3]);
            } else if (d.m_declTypeId == SID("point")) {
                const f32 *v = reinterpret_cast<const f32 *>(d.m_pDeclValue);
                value = fmt::format("({:.2f}, {:.2f}, {:.2f})", v[0], v[1], v[2]);
            } else {
                // Unknown type: dump first 8 bytes as hex.
                const u8 *raw = reinterpret_cast<const u8 *>(d.m_pDeclValue);
                value = fmt::format("??? {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                                    raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
            }

            m_formatter->print("{:>4}  {:<32}  {:<12}  {:>5}  {}\n", i, name, type, d.m_varSizeSum,
                               value);
        }
    }

    void BinaryFileInspector::inspect_declarations_only() {
        const DCEntry *table = m_file->entries();
        const u32      n = m_file->entry_count();

        if (!table || n == 0) {
            m_formatter->print("(no entries)\n");
            return;
        }

        bool any = false;
        for (u32 i = 0; i < n; ++i) {
            if (BinaryFile::entry_kind(table[i]) != BinaryFile::EntryKind::StateScript) {
                continue;
            }
            const auto *ss = m_file->entry_as_state_script(table[i]);
            if (!ss) { continue; }
            any = true;

            m_formatter->print("StateScript {}:\n", sid_str(ss->m_stateScriptId));
            inspect_declaration_list(ss->m_pSsDeclList);
            m_formatter->print("\n");
        }

        if (!any) { m_formatter->print("(no state-scripts found)\n"); }
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

    void BinaryFileInspector::inspect_symbol_array(const SymbolArray *arr,
                                                   const std::string &name) {
        if (!arr) {
            m_formatter->print("{}: (null)\n", name);
            return;
        }

        m_formatter->print("{}: {} entries\n", name, arr->m_numEntries);

        if (!arr->m_pSymbols || arr->m_numEntries == 0) { return; }

        for (u32 i = 0; i < arr->m_numEntries; ++i) {
            m_formatter->print("  [{}] {}\n", i, sid_str(arr->m_pSymbols[i]));
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

        if (block->m_pScriptLambda) {
            inspect_script_lambda(block->m_pScriptLambda, block->name());
        }

        inspect_track_group(&block->m_trackGroup);
    }

    void BinaryFileInspector::inspect_state_summary(const StateScript *ss) {
        if (!ss || !ss->m_pSsStateTable || ss->m_stateCount <= 0) {
            m_formatter->print("(no states)\n");
            return;
        }

        m_formatter->print("{:>4}  {:<40}  {:>6}  {}\n", "idx", "name", "blocks", "block types");

        for (i16 i = 0; i < ss->m_stateCount; ++i) {
            const SsState &state = ss->m_pSsStateTable[i];

            const std::string name = sid_str(state.m_stateId);

            // Build a comma-separated list of block types.
            std::string types;
            for (i64 j = 0; j < state.m_numSsOnBlocks; ++j) {
                const SsOnBlock &block = state.m_pSsOnBlocks[j];
                if (!types.empty()) { types += ", "; }
                switch (block.m_blockType) {
                case BlockType::Start: types += "start"; break;
                case BlockType::End: types += "end"; break;
                case BlockType::Event: types += "event " + sid_str(block.m_blockEventId); break;
                case BlockType::Update: types += "update"; break;
                case BlockType::Virtual: types += "virtual"; break;
                case BlockType::Code: types += "code"; break;
                case BlockType::Exit: types += "exit"; break;
                case BlockType::Post: types += "post"; break;
                default:
                    types += fmt::format("unknown({})", static_cast<int>(block.m_blockType));
                    break;
                }
            }

            m_formatter->print("{:>4}  {:<40}  {:>6}  {}\n", i, name, state.m_numSsOnBlocks, types);
        }
    }

    void BinaryFileInspector::inspect_state_detail_summary(const StateScript *ss) {
        if (!ss || !ss->m_pSsStateTable || ss->m_stateCount <= 0) {
            m_formatter->print("(no states)\n");
            return;
        }

        for (i16 i = 0; i < ss->m_stateCount; ++i) {
            const SsState &state = ss->m_pSsStateTable[i];
            m_formatter->print("state[{}] {}\n", i, sid_str(state.m_stateId));

            for (i64 j = 0; j < state.m_numSsOnBlocks; ++j) {
                const SsOnBlock &block = state.m_pSsOnBlocks[j];

                // Block type name.
                std::string block_name;
                switch (block.m_blockType) {
                case BlockType::Start: block_name = "start"; break;
                case BlockType::End: block_name = "end"; break;
                case BlockType::Event: block_name = "event " + sid_str(block.m_blockEventId); break;
                case BlockType::Update: block_name = "update"; break;
                case BlockType::Virtual: block_name = "virtual"; break;
                case BlockType::Code: block_name = "code"; break;
                case BlockType::Exit: block_name = "exit"; break;
                case BlockType::Post: block_name = "post"; break;
                default:
                    block_name = fmt::format("unknown({})", static_cast<int>(block.m_blockType));
                    break;
                }

                m_formatter->print("  block[{}] {}\n", j, block_name);

                const SsTrackGroup &tg = block.m_trackGroup;

                // Individual tracks with lambda summary.
                if (tg.m_aTracks && tg.m_numTracks > 0) {
                    for (i16 t = 0; t < tg.m_numTracks; ++t) {
                        const SsTrack &track = tg.m_aTracks[t];
                        m_formatter->print("    track[{}] {:<24}  {} lambda(s)\n", t,
                                           sid_str(track.m_trackId), track.m_totalLambdaCount);
                        inspect_lambda_summary(&track);
                    }
                }

                // Rare script-lambda attached to the track group.
                if (tg.m_rareScriptLambda) {
                    m_formatter->print("    (rare script-lambda attached)\n");
                }
            }
        }
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

    void BinaryFileInspector::inspect_lambda_summary(const SsTrack *track) {
        if (!track || !track->m_pSsLambda || track->m_totalLambdaCount <= 0) {
            m_formatter->print("    (no lambdas)\n");
            return;
        }

        m_formatter->print("    {:>4}  {:<12}  {:>6}  {:>8}  {:<24}  {}\n", "idx", "func-name",
                           "instrs", "symbols", "global-scope", "kind");
        for (i16 i = 0; i < track->m_totalLambdaCount; ++i) {
            const SsLambda &ss_lambda = track->m_pSsLambda[i];

            if (!ss_lambda.m_pScriptLambda) {
                m_formatter->print("    {:>4}  {:<12}  {:>6}  {:>8}  {:<24}  {}\n", i, "(null)", 0,
                                   0, "(null)", "empty slot");
                continue;
            }

            const ScriptLambda &sl = *ss_lambda.m_pScriptLambda;

            const std::string func_name =
                sl.m_funcName != 0 ? sid_str(sl.m_funcName) : "(anonymous)";
            const std::string global_scope =
                sl.m_sidGlobal != 0 ? sid_str(sl.m_sidGlobal) : "(null)";

            m_formatter->print("    {:>4}  {:<12}  {:>6}  {:>8}  {:<24}  {}\n", i, func_name,
                               sl.m_numInstructions, sl.m_numSymbols, global_scope,
                               "script-lambda");
        }
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

            m_formatter->print("{:04X}   {:08X}   {:02X} {:02X} {:02X} {:02X}  {}\n", i,
                               file_offset, static_cast<u8>(ins.opcode), ins.destination,
                               ins.operand1, ins.operand2, format_instruction(ins, info, lambda));
        }

        const u32 num_syms = lambda->m_numSymbols;
        if (symbols && num_syms > 0) {
            m_formatter->print("\nSYMBOL TABLE:\n");
            IFormatter::Block sym_block(*m_formatter, m_indent);

            for (u32 i = 0; i < num_syms; i++) {
                const u64        symbol = symbols[i];
                const StaticType type = symbol_types[i];
                const u64        sym_offset = reinterpret_cast<u64>(&symbols[i]) -
                                       reinterpret_cast<u64>(m_file->m_bytes.get());
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

    std::string BinaryFileInspector::format_instruction(const LongInstruction     &ins,
                                                        const InstructionInfo *info,
                                                        const ScriptLambda    *lambda) {
        std::string result = info->name;

        switch (info->operands_count()) {
        case 1:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
            break;
        case 2:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
            if (info->b_type == OperandType::REG) {
                result += fmt::format(", r{}", ins.operand1);
            } else if (info->b_type == OperandType::IMM_U16) {
                u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
                result += fmt::format(", {}", imm);
            }
            break;
        case 3:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
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
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
            break;
        case 2:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
            if (info->b_type == OperandType::REG) {
                result += fmt::format(", r{}", ins.operand1);
            } else if (info->b_type == OperandType::IMM_U16) {
                u16 imm = (static_cast<u16>(ins.operand2) << 8) | ins.operand1;
                result += fmt::format(", {}", imm);
            }
            break;
        case 3:
            if (info->a_type == OperandType::REG) {
                result += fmt::format(" r{}", ins.destination);
            }
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
    /// @brief Print the contents of every data-struct entry in the file.
    /// @details Uses the layout information serialised into BinaryFile::m_dataStructs.
    ///          Does NOT consult TypeSystem — libcarbon is independent of it.
    ///
    ///          NOTE: ds.offset is a *file offset*, not an absolute pointer. It is
    ///          set in FileNode::make_binary from the entry table (before relocation)
    ///          and is relative to the start of m_bytes. So the payload starts at
    ///          m_bytes.get() + ds.offset.
    void BinaryFileInspector::inspect_data_structs() {
        if (m_file->m_dataStructs.empty()) { return; }

        m_formatter->print("\n--- Data Structs Content ---\n");

        for (const auto &ds : m_file->m_dataStructs) {
            const auto &layout = ds.layout;

            // ds.offset is a file offset (relative to m_bytes), NOT an absolute
            // pointer. Use it directly.
            const u64 file_offset = ds.offset;

            // Bounds check: the payload must fit entirely inside the file.
            if (file_offset + layout.total_size > m_file->m_size) {
                m_formatter->print("\n  {} (type {}, offset 0x{:X}): out of bounds\n", ds.name,
                                   layout.type_name, file_offset);
                continue;
            }

            m_formatter->print("\n  {} (type {}, offset 0x{:X}, size {}):\n", ds.name,
                               layout.type_name, file_offset, layout.total_size);
            m_formatter->print("    {:<8} {:<20} {:<12} {:<10} {}\n", "Offset", "Bytes", "Field",
                               "Type", "Value");

            const u8 *base = reinterpret_cast<const u8 *>(m_file->m_bytes.get()) + file_offset;

            for (const auto &f : layout.fields) {
                m_formatter->print("    {:<8} ", fmt::format("0x{:04X}", file_offset + f.offset));

                const u32 dump_size = std::min<u32>(f.size, 8);
                for (u32 i = 0; i < dump_size; ++i) {
                    m_formatter->print("{:02X} ", base[f.offset + i]);
                }
                for (u32 i = dump_size; i < 8; ++i) { m_formatter->print("   "); }

                m_formatter->print("  {:<12} {:<12} ", f.name, f.type_name);

                // Interpret the value based on the serialised type name.
                if (f.is_array) {
                    m_formatter->print("[{}]", f.array_size);
                } else if (f.is_inline) {
                    m_formatter->print("(inline)");
                } else if (f.type_name == "int" || f.type_name == "int32") {
                    i32 v;
                    std::memcpy(&v, base + f.offset, sizeof(i32));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "int8") {
                    i8 v;
                    std::memcpy(&v, base + f.offset, sizeof(i8));
                    m_formatter->print("{}", static_cast<int>(v));
                } else if (f.type_name == "int16") {
                    i16 v;
                    std::memcpy(&v, base + f.offset, sizeof(i16));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "int64") {
                    i64 v;
                    std::memcpy(&v, base + f.offset, sizeof(i64));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "uint8") {
                    u8 v;
                    std::memcpy(&v, base + f.offset, sizeof(u8));
                    m_formatter->print("{}", static_cast<unsigned>(v));
                } else if (f.type_name == "uint16") {
                    u16 v;
                    std::memcpy(&v, base + f.offset, sizeof(u16));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "uint32") {
                    u32 v;
                    std::memcpy(&v, base + f.offset, sizeof(u32));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "uint64") {
                    u64 v;
                    std::memcpy(&v, base + f.offset, sizeof(u64));
                    m_formatter->print("{}", v);
                } else if (f.type_name == "float") {
                    f32 v;
                    std::memcpy(&v, base + f.offset, sizeof(f32));
                    m_formatter->print("{:.6f}", v);
                } else if (f.type_name == "symbol") {
                    u64 v;
                    std::memcpy(&v, base + f.offset, sizeof(u64));
                    m_formatter->print("{}", sid_str(v));
                } else if (f.type_name == "string" || f.type_name.starts_with("pointer")) {
                    u64 v;
                    std::memcpy(&v, base + f.offset, sizeof(u64));
                    m_formatter->print("0x{:X}", v);
                } else {
                    m_formatter->print("?");
                }
                m_formatter->print("\n");
            }
        }
    }
} // namespace carbon