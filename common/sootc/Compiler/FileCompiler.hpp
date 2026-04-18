// sootc/Compiler/FileCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/Env/GlobalEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include <expected>
#include <memory>
#include <vector>
#include <string>

using namespace carbon;

namespace sootc {

class Compiler;

// Структура для глобального состояния (аналог compilation::global_state)
struct GlobalState {
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, std::pair<TypeSpec, sid64>> m_sidAliases;
    
    u32 add_string(const std::string& str) {
        u32 index = static_cast<u32>(m_strings.size());
        m_strings.push_back(str);
        return index;
    }
};

class FileCompiler {
public:
    explicit FileCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Основной API - компиляция файла в бинарник
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(const script::Object& forms, FileEnv* env, const std::string& filename);
    
    // Компиляция с предварительным глобальным состоянием
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(const script::Object& forms, FileEnv* env, GlobalState& global, const std::string& filename);
    
    // Получение бинарных элементов (аналог compile_binary_elements)
    [[nodiscard]] std::expected<std::vector<ProgramBinaryElement>, std::string> 
    compile_binary_elements(FileEnv* env, GlobalState& global);
    
    // Сборка финального бинарника из элементов
    [[nodiscard]] std::expected<std::pair<BinaryFile::byte_uptr, u64>, std::string> 
    make_binary(std::vector<ProgramBinaryElement> program_elements, const GlobalState& global);

private:
    TypeSystem& ts_;
    Compiler* compiler_;
    
    // Вспомогательные методы для сериализации
    template<typename Ptr, typename T>
    static void insert_into_bytestream(Ptr& out, u64& size, const T& obj) noexcept {
        static_assert(sizeof(std::decay_t<decltype(*out.get())>) == 1, "Output pointer must be byte-oriented");
        std::memcpy(out.get() + size, std::addressof(obj), sizeof(T));
        size += sizeof(T);
    }

    template<typename Ptr>
    static void insert_into_bytestream(Ptr& out, u64& size, const std::vector<std::byte>& data) noexcept {
        std::memcpy(out.get() + size, data.data(), data.size());
        size += data.size();
    }
    
    static void insert_into_reloctable(u8* reloc_table, u64& byte_offset, u64& bit_offset, u8 bits, u64 num_bits) noexcept;
    
    // Получение смещения строки в string table
    u64 get_string_offset(const GlobalState& global, u32 index, u64 data_size) const noexcept;
};

} // namespace sootc