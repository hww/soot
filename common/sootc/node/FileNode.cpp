// FileNode.cpp
#include "common/sootc/node/FileNode.hpp"
#include "common/sootc/node/FunctionNode.hpp"
#include "common/carbon/file/DCHeader.hpp"
#include "common/util/Log.hpp"
#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "TypeDeclarationNode.hpp"
#include "EnumDeclarationNode.hpp"
#include "DataDeclarationNode.hpp"

#include <cassert>
#include <cstring>
#include <numeric>

using namespace carbon;

namespace sootc {

// ============================================================================
// Constructor
// ============================================================================
FileNode::FileNode(const std::string& name) 
    : Node(NodeType::FileNode), m_name(name) {}

// ============================================================================
// to_string
// ============================================================================
std::string FileNode::to_string() const {
    return "FileNode(name=" + m_name + ", symbols=" + std::to_string(m_symbols.size()) + ")";
}

// ============================================================================
// generate - главный метод генерации бинарника (интерфейс Node)
// ============================================================================
ProgramBinaryElement FileNode::generate(GlobalState &state) {
    auto entries = collect_all(state);

    size_t type_decls = 0;
    for (auto &child : m_children) {
        if (dynamic_cast<TypeDeclarationNode *>(child.get()) != nullptr ||
            dynamic_cast<EnumDeclarationNode *>(child.get()) != nullptr) {
            ++type_decls;
        }
    }

    if (entries.empty() && type_decls > 0) {
        lg::info("FileNode::generate: {} type/enum declaration(s), no emittable entries",
                 type_decls);
        return ProgramBinaryElement(0);
    }

    if (entries.empty()) {
        std::string types;
        for (auto &child : m_children) {
            if (!types.empty()) types += ", ";
            types += child->get_node_type_string();
        }
        throw CompilerError("FileNode::generate")
            .where(fmt::format("file '{}'", m_name))
            .expected("at least one FunctionNode or DataDeclarationNode")
            .got(fmt::format("{} children: [{}]", m_children.size(),
                             types.empty() ? "<none>" : types));
    }

    return make_binary(std::move(entries), state);
}

// ============================================================================
// collect_functions - собирает ProgramBinaryElement для всех функций
// ============================================================================
std::vector<ProgramBinaryElement> FileNode::collect_all(GlobalState &state) {
    std::vector<ProgramBinaryElement> entries;

    for (auto &child : m_children) {
        if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
            fn->emit_body();
            entries.push_back(fn->generate(state));
            lg::info("Function '{}': {} instructions, {} constants", fn->name(),
                     fn->instructions().size(), fn->constants().size());
        } else if (auto *decl = dynamic_cast<DataDeclarationNode *>(child.get())) {
            ProgramBinaryElement element = decl->generate(state);
            const size_t         payload_size = element.m_rawData.size();
            if (payload_size > 0) {
                entries.push_back(std::move(element));
                lg::info("Data declaration '{}': {} bytes", decl->name(), payload_size);
            } else {
                lg::warn("Data declaration '{}' produced empty element", decl->name());
            }
        }
        // TypeDeclarationNode / EnumDeclarationNode — no binary output.
    }

    return entries;
}

// ============================================================================
// make_binary - сборка финального бинарника
// ============================================================================
ProgramBinaryElement FileNode::make_binary(std::vector<ProgramBinaryElement> program_elements,
                                           GlobalState                      &state) {
    printf("=== make_binary DEBUG ===\n");
    printf("program_elements.size() = %zu\n", program_elements.size());

    if (program_elements.empty()) { return ProgramBinaryElement(0); }

    constexpr sid64 ARRAY_SID = SID("array");
    constexpr u64   first_entry_offset = 0x28;
    constexpr u32   header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);

    const u64 num_entries = program_elements.size();
    const u64 entries_size = sizeof(DCEntry) * num_entries;

    const u64 entries_data_size =
        std::accumulate(program_elements.begin(), program_elements.end(), u64{0},
                        [](u64 acc, const ProgramBinaryElement &element) {
                            return acc + element.m_rawData.size();
                        });

    const u64 stringtable_size =
        std::accumulate(state.m_strings.begin(), state.m_strings.end(), u64{0},
                        [](u64 acc, const std::string &s) { return acc + s.size() + 1; });

    std::vector<char> stringtable;
    stringtable.reserve(stringtable_size);
    for (const auto &s : state.m_strings) {
        stringtable.insert(stringtable.end(), s.begin(), s.end());
        stringtable.push_back('\0');
    }

    const u64 data_size = header_size + entries_size + entries_data_size;
    const u64 total_size =
        data_size + stringtable_size + 4 + ((data_size + stringtable_size + 63) / 64);

    printf("total_size = %lu\n", (unsigned long)total_size);

    ProgramBinaryElement element(total_size);

    // ========================================
    // 1. HEADER
    // ========================================
    // DC_Header has 7 fields. The last one (m_pStartOfData) is a pointer and
    // must be relocated. In 8-byte slot terms the header is 4 slots wide:
    //   slot 0: m_magic       (4) + m_versionNumber (4)
    //   slot 1: m_textSize    (4) + m_stringsOffset (4)
    //   slot 2: m_always1     (4) + m_numEntries    (4)
    //   slot 3: m_pStartOfData (8)                        ← relocatable
    DC_Header header{DC_FILE_MAGIC,
                     DC_FILE_VERSION,
                     static_cast<uint32_t>(data_size + stringtable_size),
                     static_cast<uint32_t>(data_size),
                     0x1,
                     static_cast<uint32_t>(num_entries),
                     reinterpret_cast<DCEntry *>(first_entry_offset)};
    element.push_bytes(header, {0, 0, 0, 1}); // slot 3 is a relocated pointer

    // ARRAY_SID is a plain SID64 value — no relocations.
    element.push_bytes(ARRAY_SID, {0});

    // ========================================
    // 2. ENTRY TABLE
    // ========================================
    // DCEntry has 3 fields, each 8 bytes:
    //   slot 0: m_nameID  (8)   — no relocation
    //   slot 1: m_typeId  (8)   — no relocation
    //   slot 2: m_entryPtr (8)  — relocatable
    const u64 first_function_start = header_size + num_entries * sizeof(DCEntry);
    u64       prev_entry_size = 0;

    for (auto &fn : program_elements) {
        DCEntry entry = fn.m_entry;
        entry.m_entryPtr = reinterpret_cast<void *>(first_function_start + prev_entry_size);
        element.push_bytes(entry, {0, 0, 1});
        prev_entry_size += fn.m_rawData.size();
        lg::info("FileNode::make_binary entry {}", entry.to_string());
    }

    // ========================================
    // 3. FUNCTION PAYLOADS
    // ========================================
    for (auto &fn : program_elements) {
        for (const auto offset : fn.m_stringOffsets) {
            const u64 str_index = *reinterpret_cast<u64 *>(&fn.m_rawData[offset]);
            u64       relative_offset = data_size;
            for (u32 i = 0; i < str_index; ++i) {
                relative_offset += state.m_strings[i].size() + 1;
            }
            *reinterpret_cast<u64 *>(&fn.m_rawData[offset]) =
                relative_offset - element.m_rawData.size();
        }

        fn.adjust_offsets(element.m_rawData.size());

        element.m_rawData.insert(element.m_rawData.end(), fn.m_rawData.begin(), fn.m_rawData.end());

        for (size_t i = 0; i < fn.m_relocTable.size(); ++i) {
            element.m_relocTable.push_back(fn.m_relocTable[i]);
        }

        element.check_size();
    }

    // ========================================
    // 4. STRING TABLE
    // ========================================
    element.m_rawData.insert(
        element.m_rawData.end(), reinterpret_cast<const std::byte *>(stringtable.data()),
        reinterpret_cast<const std::byte *>(stringtable.data()) + stringtable.size());

    // 5. Padding
    size_t padding = (4 - (stringtable.size() % 4)) % 4;
    element.m_rawData.insert(element.m_rawData.end(), padding, std::byte{0});

    // 6. Reloc table size
    uint32_t reloc_size = static_cast<uint32_t>((data_size + stringtable_size + 63) / 64);
    element.push_bytes(reloc_size, {0});
    lg::info("Constuct reloc table with size {}", reloc_size);

    // 7. Relocation bitmap
    size_t reloc_bytes = (element.m_relocTable.size() + 7) / 8;
    for (size_t i = 0; i < reloc_bytes; ++i) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = i * 8 + bit;
            if (idx < element.m_relocTable.size() && element.m_relocTable[idx]) {
                byte |= (1 << bit);
            }
        }
        element.push_bytes(byte, {0});
    }

    return element;
}

// ============================================================================
// Управление символами
// ============================================================================
Node* FileNode::lookup(const std::string& name) {
    // 1. Свои символы
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) return it->second;
    
    // 2. Импорты
    for (auto* imp : m_imports) {
        if (auto* val = imp->lookup(name)) return val;
    }
    
    // 3. Родитель
    return parent() ? parent()->lookup(name) : nullptr;
}

void FileNode::bind(const std::string& name, Node* node) {
    if (m_symbols.find(name) == m_symbols.end()) {
        m_ordered_symbols.push_back(node);
    }
    m_symbols[name] = node;
}

void FileNode::add_import(FileNode* file) {
    m_imports.push_back(file);
}

void FileNode::insert_into_reloctable(u8* reloc_table, u64& byte_offset, u64& bit_offset, u8 bits, u64 num_bits) noexcept {
    const u8 bit_space_remaining = (8 - bit_offset % 8);
    if (bit_space_remaining >= num_bits) {
        reloc_table[byte_offset] |= bits << bit_offset;
        bit_offset += num_bits;
        if (bit_offset == 8) {
            bit_offset = 0;
            byte_offset++;
        }
    } else {
        reloc_table[byte_offset++] |= bits << bit_offset;
        reloc_table[byte_offset] |= bits >> bit_space_remaining;
        bit_offset = num_bits - bit_space_remaining;
    }
}

} // namespace sootc