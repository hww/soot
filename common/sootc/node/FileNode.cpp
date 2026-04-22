// FileNode.cpp
#include "common/sootc/node/FileNode.hpp"
#include "common/sootc/node/FunctionNode.hpp"
#include "common/carbon/file/DCHeader.hpp"
#include "common/util/Log.hpp"
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
ProgramBinaryElement FileNode::generate(GlobalState& state) {
    // 1. Собираем все функции
    auto functions = collect_functions(state);
    if (functions.empty()) {
        throw std::runtime_error("No functions found in file");
    }
    
    // 2. Собираем финальный бинарник
    return make_binary(std::move(functions), state);
}

// ============================================================================
// collect_functions - собирает ProgramBinaryElement для всех функций
// ============================================================================
std::vector<ProgramBinaryElement> FileNode::collect_functions(GlobalState& state) {
    std::vector<ProgramBinaryElement> functions;
    
    for (auto& child : m_children) {
        if (auto* fn = dynamic_cast<FunctionNode*>(child.get())) {
            fn->emit_body();
            functions.push_back(fn->generate(state));
            lg::info("Function '{}': {} instructions, {} constants", 
                     fn->name(), 
                     fn->instructions().size(),
                     fn->constants().size());
        }
    }
    
    return functions;
}

// ============================================================================
// make_binary - сборка финального бинарника
// ============================================================================
ProgramBinaryElement FileNode::make_binary(std::vector<ProgramBinaryElement> program_elements, 
                                            GlobalState& state) {
    printf("=== make_binary DEBUG ===\n");
    printf("program_elements.size() = %zu\n", program_elements.size());
    
    if (program_elements.empty()) {
        return ProgramBinaryElement(0);
    }

    constexpr sid64 ARRAY_SID = SID("array");
    constexpr u64 first_entry_offset = 0x28;
    constexpr u32 header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);
    
    const u64 num_entries = program_elements.size();
    const u64 entries_size = sizeof(DCEntry) * num_entries;
    
    // Вычисляем размеры
    const u64 entries_data_size = std::accumulate(
        program_elements.begin(), program_elements.end(), u64{0},
        [](u64 acc, const ProgramBinaryElement& element) {
            return acc + element.m_rawData.size();
        }
    );
    
    const u64 stringtable_size = std::accumulate(
        state.m_strings.begin(), state.m_strings.end(), u64{0},
        [](u64 acc, const std::string& s) {
            return acc + s.size() + 1;
        }
    );
    
    // Строим string table
    std::vector<char> stringtable;
    stringtable.reserve(stringtable_size);
    for (const auto& s : state.m_strings) {
        stringtable.insert(stringtable.end(), s.begin(), s.end());
        stringtable.push_back('\0');
    }
    
    const u64 data_size = header_size + entries_size + entries_data_size;
    const u64 total_size = data_size + stringtable_size + 4 + ((data_size + stringtable_size + 63) / 64);
    
    printf("total_size = %lu\n", (unsigned long)total_size);
    
    // Создаем элемент
    ProgramBinaryElement element(total_size);
    
    // ========================================
    // 1. ЗАГОЛОВОК
    // ========================================
    DC_Header header{
        DC_MAGIC,
        DC_VERSION,
        static_cast<uint32_t>(data_size + stringtable_size),
        static_cast<uint32_t>(data_size),
        0x1,
        static_cast<uint32_t>(num_entries),
        reinterpret_cast<DCEntry*>(first_entry_offset)
    };
    // header имеет 7 полей, последнее (индекс 6) - указатель, требует релокации
    element.push_bytes(header, 0,0,0,0,0,0,1);
    element.push_bytes(ARRAY_SID, 0b0);
    
    // ========================================
    // 2. ВСЕ ENTRY (без данных функций)
    // ========================================
    const u64 first_function_start = header_size + num_entries * sizeof(DCEntry);
    u64 prev_entry_size = 0;
    
    for (auto& fn : program_elements) {
        DCEntry entry = fn.m_entry;  // Entry уже создан в FunctionNode
        entry.m_entryPtr = reinterpret_cast<void*>(first_function_start + prev_entry_size);
        // Entry имеет 3 поля: nameID (0), typeId (1), entryPtr (2)
        // Только entryPtr требует релокации
        element.push_bytes(entry, 0, 0, 1);
        prev_entry_size += fn.m_rawData.size();
        lg::info("FileNode::make_binary entry {}", entry.to_string());
    }
    
    // ========================================
    // 3. ВСЕ ДАННЫЕ ФУНКЦИЙ
    // ========================================
    for (auto& fn : program_elements) {
        // Корректируем строковые оффсеты
        for (const auto offset : fn.m_stringOffsets) {
            const u64 str_index = *reinterpret_cast<u64*>(&fn.m_rawData[offset]);
            u64 relative_offset = data_size;
            for (u32 i = 0; i < str_index; ++i) {
                relative_offset += state.m_strings[i].size() + 1;
            }
            *reinterpret_cast<u64*>(&fn.m_rawData[offset]) = relative_offset - element.m_rawData.size();
        }
        
        fn.adjust_offsets(element.m_rawData.size());
        
        // Копируем данные функции
        element.m_rawData.insert(element.m_rawData.end(), 
                                  fn.m_rawData.begin(), 
                                  fn.m_rawData.end());
        
        // Копируем relocation биты
        for (size_t i = 0; i < fn.m_relocTable.size(); ++i) {
            if (fn.m_relocTable[i]) {
                element.m_relocTable.push_back(true);
            } else {
                element.m_relocTable.push_back(false);
            }
        }
    }
    
    // ========================================
    // 4. STRING TABLE
    // ========================================
    element.m_rawData.insert(element.m_rawData.end(), 
                              reinterpret_cast<const std::byte*>(stringtable.data()),
                              reinterpret_cast<const std::byte*>(stringtable.data()) + stringtable.size());
    
    // 5. Padding
    size_t padding = (4 - (stringtable.size() % 4)) % 4;
    element.m_rawData.insert(element.m_rawData.end(), padding, std::byte{0});
    
    // 6. Размер reloc table
    uint32_t reloc_size = static_cast<uint32_t>((data_size + stringtable_size + 63) / 64);
    element.push_bytes(reloc_size, 0);
    
    // 7. Relocation table (битовая карта)
    size_t reloc_bytes = (element.m_relocTable.size() + 7) / 8;
    for (size_t i = 0; i < reloc_bytes; ++i) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = i * 8 + bit;
            if (idx < element.m_relocTable.size() && element.m_relocTable[idx]) {
                byte |= (1 << bit);
            }
        }
        element.push_bytes(byte, 0);
    }
    
    element.dump();
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