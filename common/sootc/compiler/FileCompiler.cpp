#include "common/sootc/compiler/FileCompiler.hpp"
#include "common/sootc/compiler/NodeBuilder.hpp"
#include "common/sootc/node/GlobalNode.hpp"
#include "common/sootc/node/FileNode.hpp"
#include "common/sootc/node/FunctionNode.hpp"
#include "common/carbon/file/DCHeader.hpp"
#include "common/util/Log.hpp"
#include <cassert>
#include <cstring>
#include <numeric>

using namespace carbon;

namespace sootc {
namespace FileCompiler {

// ============================================================================
// compile - основная функция
// ============================================================================

std::expected<std::unique_ptr<BinaryFile>, std::string> 
compile(const script::Object& forms, const std::string& filename, Compiler& compiler) {
    lg::info("Compiling file: {}", filename);
    
    // 1. Создаем строитель узлов
    NodeBuilder builder(compiler.ts(), &compiler);
    
    // 2. Строим дерево
    auto global = std::make_unique<GlobalNode>();
    auto file_node = std::make_unique<FileNode>(filename);
    
    auto current = forms;
    while (current.is_pair()) {
        auto node = builder.build(current.as_pair()->car, file_node.get());
        if (node) {
            file_node->add_child(std::move(node));
        }
        current = current.as_pair()->cdr;
    }
    global->add_child(std::move(file_node));
    
    // 3. Собираем бинарные элементы
    GlobalState state;
    auto elements = collect_binary_elements(global.get(), state);
    if (!elements) {
        return std::unexpected(elements.error());
    }
    
    // 4. Собираем финальный бинарник
    auto result = make_binary(std::move(*elements), state);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    auto& [bytes, size] = *result;
    
    // 5. Создаем BinaryFile
    return BinaryFile::from_buffer(filename, std::move(bytes), size)
        .transform([](BinaryFile&& file) {
            return std::make_unique<BinaryFile>(std::move(file));
        });
}

// ============================================================================
// collect_binary_elements
// ============================================================================

std::expected<std::vector<ProgramBinaryElement>, std::string> 
collect_binary_elements(GlobalNode* global, GlobalState& state) {
    std::vector<ProgramBinaryElement> functions;
    
    for (auto& child : global->children()) {
        if (auto* file = dynamic_cast<FileNode*>(child.get())) {
            for (auto& fn_child : file->children()) {
                if (auto* fn = dynamic_cast<FunctionNode*>(fn_child.get())) {
                    fn->emit_body();
                    functions.push_back(fn->build_binary(fn->name()));
                    lg::info("Function '{}': {} instructions, {} constants", 
                             fn->name(), 
                             fn->instructions().size(),
                             fn->constants().size());
                }
            }
        }
    }
    
    if (functions.empty()) {
        return std::unexpected("No functions found in file");
    }
    
    return functions;
}

// ============================================================================
// make_binary - сборка финального бинарника
// ============================================================================

std::expected<std::pair<BinaryFile::byte_uptr, u64>, std::string> 
make_binary(std::vector<ProgramBinaryElement> program_elements, const GlobalState& global) {
    constexpr sid64 ARRAY_SID = SID("array");
    constexpr sid64 FUNCTION_SID = SID("function");
    constexpr u64 reloc_table_size_offset = 0x4;
    constexpr u64 first_entry_offset = 0x28;
    constexpr u32 header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);
    
    const u64 num_entries = program_elements.size();
    const u32 entries_size = static_cast<u32>(sizeof(DCEntry) * num_entries);
    
    // Вычисляем размеры
    const u64 entries_data_size = std::accumulate(
        program_elements.begin(), program_elements.end(), u64{0},
        [](u64 acc, const ProgramBinaryElement& element) {
            return acc + element.m_rawData.size();
        }
    );
    
    const u64 stringtable_size = std::accumulate(
        global.m_strings.begin(), global.m_strings.end(), u64{0},
        [](u64 acc, const std::string& s) {
            return acc + s.size() + 1;
        }
    );
    
    // Строим string table
    std::vector<char> stringtable;
    stringtable.reserve(stringtable_size);
    for (const auto& s : global.m_strings) {
        stringtable.insert(stringtable.end(), s.begin(), s.end());
        stringtable.push_back('\0');
    }
    
    const u32 data_size = header_size + entries_size + static_cast<u32>(entries_data_size);
    const u32 stringtable_reloctable_padding = stringtable_size % 4 == 0 ? 0 : 4 - (stringtable_size % 4);
    const u32 relocatable_data_size = data_size + static_cast<u32>(stringtable_size);
    const u32 reloc_table_size = static_cast<u32>(std::ceil(relocatable_data_size / 64.f));
    const u32 non_relocatable_size = stringtable_reloctable_padding + reloc_table_size;
    const u32 total_size = relocatable_data_size + reloc_table_size_offset + non_relocatable_size;
    
    // Выделяем память
    BinaryFile::byte_uptr out(static_cast<std::byte*>(::operator new[](total_size, std::align_val_t(64))));
    std::memset(out.get(), 0, total_size);
    
    u64 current_size = 0;
    const u32 reloc_table_start = relocatable_data_size + stringtable_reloctable_padding + reloc_table_size_offset;
    u8* reloc_table_ptr = reinterpret_cast<u8*>(out.get() + reloc_table_start);
    u64 byte_offset = 0, bit_offset = 0;
    
    // Лямбда для записи с релокацией
    auto push_bytes = [&](auto&& arg, auto&&... bits) {
        std::memcpy(out.get() + current_size, std::addressof(arg), sizeof(arg));
        current_size += sizeof(arg);
        const std::vector<u8> bits_list = {static_cast<u8>(bits)...};
        for (size_t i = 0; i < bits_list.size() - 1; ++i) {
            // insert_into_reloctable...
        }
        // insert_into_reloctable...
    };
    
    // Пишем заголовок
    DC_Header header{
        DC_MAGIC,
        DC_VERSION,
        relocatable_data_size + stringtable_reloctable_padding,
        data_size,
        0x1,
        static_cast<u32>(num_entries),
        reinterpret_cast<DCEntry*>(first_entry_offset)
    };
    push_bytes(header, 0b1000);
    push_bytes(ARRAY_SID, 0b0);
    
    // Пишем таблицу entry point'ов
    const u64 first_function_start = header_size + num_entries * sizeof(DCEntry);
    u64 prev_entry_size = sizeof(FUNCTION_SID);
    
    for (auto& element : program_elements) {
        element.m_entry.m_entryPtr = reinterpret_cast<void*>(first_function_start + prev_entry_size);
        push_bytes(element.m_entry, 0b100);
        prev_entry_size += element.m_rawData.size();
    }
    
    // Пишем функции
    for (auto& fn : program_elements) {
        // Корректируем строковые оффсеты
        for (const auto offset : fn.m_stringOffsets) {
            const u64 str_index = *reinterpret_cast<u64*>(&fn.m_rawData[offset]);
            u64 relative_offset = 0;
            for (u32 i = 0; i < str_index; ++i) {
                relative_offset += global.m_strings[i].size() + 1;
            }
            relative_offset += data_size;
            *reinterpret_cast<u64*>(&fn.m_rawData[offset]) = relative_offset - current_size;
        }
        
        fn.adjust_offsets(current_size);
        std::memcpy(out.get() + current_size, fn.m_rawData.data(), fn.m_rawData.size());
        current_size += fn.m_rawData.size();
        
        for (const auto& bit : fn.m_relocTable) {
            // insert_into_reloctable...
        }
    }
    
    // Пишем string table
    std::memcpy(out.get() + current_size, stringtable.data(), stringtable.size());
    current_size += stringtable.size();
    std::memset(out.get() + current_size, 0, stringtable_reloctable_padding);
    current_size += stringtable_reloctable_padding;
    
    // Пишем размер таблицы релокации
    std::memcpy(out.get() + current_size, &reloc_table_size, sizeof(reloc_table_size));
    current_size += sizeof(reloc_table_size);
    current_size += reloc_table_size;
    
    assert(current_size == total_size);
    
    return std::pair(std::move(out), total_size);
}

} // namespace FileCompiler
} // namespace sootc