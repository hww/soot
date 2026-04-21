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
        printf("ERROR: program_elements is EMPTY!\n");
        return ProgramBinaryElement(0);
    }

    for (size_t i = 0; i < program_elements.size(); i++) {
        printf("  element[%zu]: m_rawData.size() = %zu\n", 
               i, program_elements[i].m_rawData.size());
        printf("  element[%zu]: m_relocTable.size() = %zu\n", 
               i, program_elements[i].m_relocTable.size());
    }

    constexpr sid64 ARRAY_SID = SID("array");
    constexpr sid64 FUNCTION_SID = SID("function");
    constexpr u64 reloc_table_size_offset = 0x4;
    constexpr u64 first_entry_offset = 0x28;
    constexpr u32 header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);
    
    const u64 num_entries = program_elements.size();
    const u32 entries_size = static_cast<u32>(sizeof(Entry) * num_entries);
    
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
    
    const u32 data_size = header_size + entries_size + static_cast<u32>(entries_data_size);
    const u32 stringtable_reloctable_padding = stringtable_size % 4 == 0 ? 0 : 4 - (stringtable_size % 4);
    const u32 relocatable_data_size = data_size + static_cast<u32>(stringtable_size);
    const u32 reloc_table_size = static_cast<u32>(std::ceil(relocatable_data_size / 64.f));
    const u32 non_relocatable_size = stringtable_reloctable_padding + reloc_table_size;
    const u32 total_size = relocatable_data_size + reloc_table_size_offset + non_relocatable_size;
    
    // Создаем ProgramBinaryElement
    ProgramBinaryElement element(total_size);
    
    u64 current_size = 0;
    const u32 reloc_table_start = relocatable_data_size + stringtable_reloctable_padding + reloc_table_size_offset;
    u8* reloc_table_ptr = reinterpret_cast<u8*>(element.m_rawData.data() + reloc_table_start);
    u64 byte_offset = 0, bit_offset = 0;
    
    // Лямбда для записи с релокацией
    auto push_bytes = [&](auto&& arg, auto&&... bits) {
        size_t before = element.m_rawData.size();
        
        // Используем insert вместо memcpy
        const std::byte* p = reinterpret_cast<const std::byte*>(std::addressof(arg));
        element.m_rawData.insert(element.m_rawData.end(), p, p + sizeof(arg));
        
        current_size += sizeof(arg);
        size_t after = element.m_rawData.size();
        printf("push_bytes: added %zu bytes, before=%zu, after=%zu, current_size=%llu\n", 
            sizeof(arg), before, after, current_size);
        
        const std::vector<u8> bits_list = {static_cast<u8>(bits)...};
        for (size_t i = 0; i < bits_list.size() - 1; ++i) {
            insert_into_reloctable(reloc_table_ptr, byte_offset, bit_offset, bits_list[i], 8);
        }
        if (bits_list.size() > 0) {
            insert_into_reloctable(reloc_table_ptr, byte_offset, bit_offset, bits_list.back(), sizeof(arg) * 8 / 8);
        }
    };
    
    // Пишем заголовок
    DC_Header header{
        DC_MAGIC,
        DC_VERSION,
        relocatable_data_size + stringtable_reloctable_padding,
        data_size,
        0x1,
        static_cast<u32>(num_entries),
        reinterpret_cast<Entry*>(first_entry_offset)
    };
    push_bytes(header, 0b1000);
    push_bytes(ARRAY_SID, 0b0);
    
    // Пишем таблицу entry point'ов
    const u64 first_function_start = header_size + num_entries * sizeof(Entry);
    u64 prev_entry_size = sizeof(FUNCTION_SID);
    
    for (auto& element_item : program_elements) {
        element_item.m_entry.m_entryPtr = reinterpret_cast<void*>(first_function_start + prev_entry_size);
        push_bytes(element_item.m_entry, 0b100);
        prev_entry_size += element_item.m_rawData.size();
    }
    
    // Пишем функции
    for (auto& fn : program_elements) {
        // Корректируем строковые оффсеты
        for (const auto offset : fn.m_stringOffsets) {
            const u64 str_index = *reinterpret_cast<u64*>(&fn.m_rawData[offset]);
            u64 relative_offset = data_size;
            for (u32 i = 0; i < str_index; ++i) {
                relative_offset += state.m_strings[i].size() + 1;
            }
            *reinterpret_cast<u64*>(&fn.m_rawData[offset]) = relative_offset - current_size;
        }
        
        fn.adjust_offsets(current_size);
        std::memcpy(element.m_rawData.data() + current_size, fn.m_rawData.data(), fn.m_rawData.size());
        current_size += fn.m_rawData.size();
        
        // Копируем relocation биты
        for (size_t i = 0; i < fn.m_relocTable.size(); ++i) {
            if (fn.m_relocTable[i]) {
                u64 byte = (current_size - fn.m_rawData.size() + i * 8) / 8;
                u64 bit = ((current_size - fn.m_rawData.size() + i * 8) % 8);
                reloc_table_ptr[byte] |= (1 << bit);
            }
        }
    }
    
    // Пишем string table
    std::memcpy(element.m_rawData.data() + current_size, stringtable.data(), stringtable.size());
    current_size += stringtable.size();
    std::memset(element.m_rawData.data() + current_size, 0, stringtable_reloctable_padding);
    current_size += stringtable_reloctable_padding;
    
    // Пишем размер таблицы релокации
    std::memcpy(element.m_rawData.data() + current_size, &reloc_table_size, sizeof(reloc_table_size));
    current_size += sizeof(reloc_table_size);
    current_size += reloc_table_size;
    
    assert(current_size == total_size);
    printf("total_size = %u\n", total_size);
    printf("=====================\n");
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