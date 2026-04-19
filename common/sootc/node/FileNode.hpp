// FileNode.hpp
#pragma once

#include "Node.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace sootc {

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
    explicit FileNode(const std::string& name) : m_name(name) {}
    
    const char* node_type() const override { return "FileNode"; }
    
    std::string to_string() const override {
        return "FileNode(name=" + m_name + ", symbols=" + std::to_string(m_symbols.size()) + ")";
    }
    
    // ========================================================================
    // Имя
    // ========================================================================
    
    const std::string& name() const { return m_name; }
    
    // ========================================================================
    // Управление символами
    // ========================================================================
    
    Node* lookup(const std::string& name) override {
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
        
    void bind(const std::string& name, Node* node) {
        if (m_symbols.find(name) == m_symbols.end()) {
            m_ordered_symbols.push_back(node);
        }
        m_symbols[name] = node;
    }
    
    // ========================================================================
    // Импорты
    // ========================================================================
    
    void add_import(FileNode* file) {
        m_imports.push_back(file);
    }
    
    const std::vector<FileNode*>& imports() const { return m_imports; }
};

} // namespace sootc