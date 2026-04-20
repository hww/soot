// FileNode.hpp
#pragma once

#include "Node.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "common/sootc/libs/GlobalState.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <expected>

namespace sootc {

class FunctionNode;

class FileNode : public Node {
    std::string m_name;
    std::unordered_map<std::string, Node*> m_symbols;
    std::vector<Node*> m_ordered_symbols;
    std::vector<FileNode*> m_imports;
    
protected:
    void update_self_cache() override {
        m_cached_file = this;
    }
    
public:
    explicit FileNode(const std::string& name);
    
    const char* node_type() const override { return "FileNode"; }
    std::string to_string() const override;
    
    // ========================================================================
    // Имя
    // ========================================================================
    const std::string& name() const { return m_name; }
    
    // ========================================================================
    // Генерация бинарника (интерфейс Node)
    // ========================================================================
    ProgramBinaryElement generate(GlobalState& state) override;
    
    // ========================================================================
    // Управление символами
    // ========================================================================
    Node* lookup(const std::string& name) override;
    void bind(const std::string& name, Node* node);
    
    // ========================================================================
    // Импорты
    // ========================================================================
    void add_import(FileNode* file);
    const std::vector<FileNode*>& imports() const { return m_imports; }
    
    static void insert_into_reloctable(u8* reloc_table, u64& byte_offset, u64& bit_offset, u8 bits, u64 num_bits) noexcept;
private:
    // Вспомогательные методы для генерации
    std::vector<ProgramBinaryElement> collect_functions(GlobalState& state);
    ProgramBinaryElement make_binary(std::vector<ProgramBinaryElement> functions, GlobalState& state);
};

} // namespace sootc