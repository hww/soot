// GlobalNode.hpp
#pragma once

#include "Node.hpp"
#include <unordered_map>
#include <string>

namespace sootc {

class GlobalNode : public Node {
    std::unordered_map<std::string, Node*> m_symbols;
    std::vector<Node*> m_ordered_symbols;
    
protected:
    void update_self_cache() override {
        m_cached_global = this;
    }
    
public:
    GlobalNode() = default;
    
    const char* node_type() const override { return "GlobalNode"; }
    
    std::string to_string() const override {
        return "GlobalNode(symbols=" + std::to_string(m_symbols.size()) + ")";
    }
    
    // ========================================================================
    // Управление символами
    // ========================================================================
    
    Node* lookup(const std::string& name) override {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end()) return it->second;
        return nullptr;
    }
    
    void bind(const std::string& name, Node* node) {
        if (m_symbols.find(name) == m_symbols.end()) {
            m_ordered_symbols.push_back(node);
        }
        m_symbols[name] = node;
    }
    
    const std::unordered_map<std::string, Node*>& symbols() const { return m_symbols; }
};

} // namespace sootc