// Node.hpp
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <type_traits>

namespace sootc {

    class GlobalNode;
    class FileNode;
    class FunctionNode;

class Node {
    Node* m_parent = nullptr;
    std::vector<std::unique_ptr<Node>> m_children;
    
protected:    
    // Кэш для быстрого доступа к узлам по типу
    GlobalNode * m_cached_global = nullptr;
    FileNode* m_cached_file = nullptr;
    FunctionNode* m_cached_function = nullptr;
    
protected:
    void invalidate_cache() {
        m_cached_global = nullptr;
        m_cached_file = nullptr;
        m_cached_function = nullptr;
        
        // Дети не должны кэшировать родителей заново
        for (auto& child : m_children) {
            child->invalidate_cache();
        }
    }
    
    void update_parent_cache(Node* parent) {
        if (!parent) return;
        
        // Копируем кэш от родителя
        m_cached_global = parent->m_cached_global;
        m_cached_file = parent->m_cached_file;
        m_cached_function = parent->m_cached_function;
        
        // И обновляем для своего типа
        update_self_cache();
    }
    
    virtual void update_self_cache() {}
    
public:
    virtual ~Node() = default;
    
    // ========================================================================
    // Навигация по дереву
    // ========================================================================
    
    Node* parent() const { return m_parent; }
    const std::vector<std::unique_ptr<Node>>& children() const { return m_children; }
    
    void add_child(std::unique_ptr<Node> child) {
        child->m_parent = this;
        child->update_parent_cache(this);
        m_children.push_back(std::move(child));
    }
    
    virtual Node* lookup(const std::string& name) {
        return nullptr;  // базовый класс не умеет искать
    }

    // ========================================================================
    // Поиск предков с кэшированием
    // ========================================================================
    
    template<typename T>
    T* find_ancestor() {
        static_assert(std::is_base_of_v<Node, T>, "T must be derived from Node");
        
        Node* current = this;
        while (current) {
            if (auto* casted = dynamic_cast<T*>(current)) {
                return casted;
            }
            current = current->m_parent;
        }
        return nullptr;
    }
    
    // ========================================================================
    // Быстрые геттеры с кэшем
    // ========================================================================
    
    class GlobalNode* global();
    class FileNode* file();
    class FunctionNode* function();
    
    // ========================================================================
    // Виртуальные методы
    // ========================================================================
    
    virtual std::string to_string() const = 0;
    virtual const char* node_type() const = 0;
};

} // namespace sootc