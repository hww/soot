// sootc/Compiler/FileCompiler.cpp
#include "sootc/Compiler/FileCompiler.hpp"
#include "file/BinaryFile.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "common/util/Log.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/Env/GlobalEnv.hpp"
#include <cstring>
#include <numeric>

using namespace carbon;
using namespace carbon;

namespace sootc {

// Константы из эталонного кода
constexpr sid64 SCRIPT_LAMBDA_SID = SID("script-lambda");
constexpr sid64 ARRAY_SID = SID("array");
constexpr sid64 GLOBAL_SID = SID("global");
constexpr sid64 FUNCTION_SID = SID("function");
constexpr u64 DEADBEEF = 0xDEAD'BEEF'1337'F00D;

FileCompiler::FileCompiler(TypeSystem& ts, Compiler* compiler)
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// Основной API
// ============================================================================

std::expected<std::unique_ptr<BinaryFile>, std::string> 
FileCompiler::compile_file(const script::Object& forms, FileEnv* env, const std::string& filename) {
    GlobalState global;
    return compile_file(forms, env, global, filename);
}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
FileCompiler::compile_file(const script::Object& forms, FileEnv* env, GlobalState& global, const std::string& filename) {
    lg::info("Compiling file: {}", filename);
    
    // Фаза 1: DECLARE - компилируем все формы в IR
    auto current = forms;
    while (current.is_pair()) {
        compiler_->compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    
    
    // Фаза 2: RESOLVE - резолвим все символы
    // ВАЖНО: нужно реализовать resolve для всех IR_Value
    
    // Фаза 3: BUILD - получаем бинарные элементы
    auto binary_elements = compile_binary_elements(env, global);
    if (!binary_elements) {
        return std::unexpected(binary_elements.error());
    }
    
    // Фаза 4: MAKE_BINARY - собираем финальный бинарник
    auto resutl = make_binary(std::move(*binary_elements), global);
    auto bytes = std::move(resutl->first);
    auto size = resutl->second;

    return BinaryFile::from_buffer(filename, std::move(bytes), size)
        .transform([](BinaryFile&& file) {
            // Если успех, перемещаем объект в кучу и оборачиваем в unique_ptr
            return std::make_unique<BinaryFile>(std::move(file));
        });
}

// ============================================================================
// compile_binary_elements - сбор бинарных элементов из окружения файла
// ============================================================================

std::expected<std::vector<ProgramBinaryElement>, std::string> 
FileCompiler::compile_binary_elements(FileEnv* env, GlobalState& global) {
    std::vector<ProgramBinaryElement> functions;
    
    // Обходим все определения в файле
    for (auto& [name, value] : env->symbols_map()) {
        // Вызываем build для каждого значения
        // Каждый компилятор (FunctionCompiler, MethodCompiler, StateCompiler)
        // должен возвращать ProgramBinaryElement
        
        auto element = value->build(compiler_);
        if (!element.is_empty()) {
            // Регистрируем строки в глобальном состоянии
            // (если есть строки в таблице символов элемента)
            functions.push_back(std::move(element));
        }
    }
    
    return functions;
}

// ============================================================================
// make_binary - сборка финального бинарника (аналог эталонного)
// ============================================================================

std::expected<std::pair<BinaryFile::byte_uptr, u64>, std::string> 
FileCompiler::make_binary(std::vector<ProgramBinaryElement> program_elements, const GlobalState& global) {
    constexpr u64 reloc_table_size_offset = 0x4;
    constexpr u64 reloctable_bit_offset   = 0x8;
    constexpr u8  text_size_offset        = 0xC;
    constexpr u64 first_entry_offset      = 0x28;
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
    
    // 1. Выделяем память с выравниванием 64 байта
    BinaryFile::byte_uptr out(static_cast<std::byte*>(::operator new[](total_size, std::align_val_t(64))));
    std::memset(out.get(), 0, total_size);

    u64 current_size = 0;
    const u32 reloc_table_start = relocatable_data_size + stringtable_reloctable_padding + reloc_table_size_offset;
    u8* reloc_table_ptr = reinterpret_cast<u8*>(out.get() + reloc_table_start);
    u64 byte_offset = 0, bit_offset = 0;
    
    // Лямбда для записи с релокацией
    auto push_bytes = [&](auto&& arg, auto&&... bits) {
        insert_into_bytestream(out, current_size, arg);
        const std::vector<u8> bits_list = {static_cast<u8>(bits)...};
        for (u32 i = 0; i < bits_list.size() - 1; ++i) {
            insert_into_reloctable(reloc_table_ptr, byte_offset, bit_offset, bits_list[i], 8);
        }
        insert_into_reloctable(reloc_table_ptr, byte_offset, bit_offset, bits_list.back(), sizeof(arg) / 8);
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
    
    for (auto& element : program_elements) {
        element.m_entry.m_entryPtr = reinterpret_cast<void*>(first_function_start + prev_entry_size);
        push_bytes(element.m_entry, 0b100);
        prev_entry_size += element.m_rawData.size();
    }
    
    // Пишем сами функции с корректировкой смещений
    for (auto& fn : program_elements) {
        // Корректируем строковые оффсеты
        for (const auto offset : fn.m_stringOffsets) {
            const u64 str_index = *reinterpret_cast<u64*>(&fn.m_rawData[offset]);
            const u64 relative_offset = get_string_offset(global, static_cast<u32>(str_index), data_size);
            *reinterpret_cast<u64*>(&fn.m_rawData[offset]) = relative_offset - current_size;
        }
        
        fn.adjust_offsets(current_size);
        insert_into_bytestream(out, current_size, fn.m_rawData);
        
        for (const auto& bit : fn.m_relocTable) {
            insert_into_reloctable(reloc_table_ptr, byte_offset, bit_offset, bit ? 1 : 0, sizeof(bool));
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

// ============================================================================
// Вспомогательные методы
// ============================================================================

u64 FileCompiler::get_string_offset(const GlobalState& global, u32 index, u64 data_size) const noexcept {
    u64 res = data_size;
    for (u32 i = 0; i < index; ++i) {
        res += global.m_strings[i].size() + 1;
    }
    return res;
}


void FileCompiler::insert_into_reloctable(u8* reloc_table, u64& byte_offset, u64& bit_offset, u8 bits, u64 num_bits) noexcept {
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