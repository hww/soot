#pragma once

#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "common/sooti/Object.hpp"
#include <vector>
#include <expected>
#include <string>

namespace sootc {

class FileNode;
class GlobalNode;
class Compiler;
class NodeBuilder;

// Глобальное состояние для сериализации
struct GlobalState {
    std::vector<std::string> m_strings;
    
    u32 add_string(const std::string& str) {
        u32 index = static_cast<u32>(m_strings.size());
        m_strings.push_back(str);
        return index;
    }
};

namespace FileCompiler {

// Компилирует файл: AST -> дерево узлов -> бинарник
std::expected<std::unique_ptr<BinaryFile>, std::string> 
compile(const script::Object& forms, const std::string& filename, Compiler& compiler);

// Собирает бинарные элементы из готового дерева
std::expected<std::vector<ProgramBinaryElement>, std::string> 
collect_binary_elements(GlobalNode* global, GlobalState& state);

// Собирает финальный бинарник из элементов
std::expected<std::pair<BinaryFile::byte_uptr, u64>, std::string> 
make_binary(std::vector<ProgramBinaryElement> elements, const GlobalState& state);

} // namespace FileCompiler

} // namespace sootc