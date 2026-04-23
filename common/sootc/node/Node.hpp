// Node.hpp
#pragma once

#include "carbon/file/ProgramBinaryElement.hpp"
#include <memory>
#include <vector>
#include <string>
#include <type_traits>

namespace sootc {

    class GlobalNode;
    class FileNode;
    class FunctionNode;
    struct GlobalState;
    
enum class NodeType {
    Node,
    BinaryNode,
    CallNode,
    CompareNode,
    ConstNode,
    ControlNode,
    ExpressionNode,
    FileNode,
    FunctionNode,
    GlobalNode,
    IfNode,
    ReturnNode,
    VariableInfo,
    VariableNode,
    WhileNode
};

inline const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NodeType::Node:           return "Node";
        case NodeType::BinaryNode:     return "BinaryNode";
        case NodeType::CallNode:       return "CallNode";
        case NodeType::CompareNode:    return "CompareNode";
        case NodeType::ConstNode:      return "ConstNode";
        case NodeType::ControlNode:    return "ControlNode";
        case NodeType::ExpressionNode: return "ExpressionNode";
        case NodeType::FileNode:       return "FileNode";
        case NodeType::FunctionNode:   return "FunctionNode";
        case NodeType::GlobalNode:     return "GlobalNode";
        case NodeType::IfNode:         return "IfNode";
        case NodeType::ReturnNode:     return "ReturnNode";
        case NodeType::VariableInfo:   return "VariableInfo";
        case NodeType::VariableNode:   return "VariableNode";
        case NodeType::WhileNode:      return "WhileNode";
        default:                       return "UnknownNode";
    }
}

class Node {
    Node* m_parent = nullptr;
protected:    
    std::vector<std::unique_ptr<Node>> m_children;
    // Кэш для быстрого доступа к узлам по типу
    GlobalNode * m_cached_global = nullptr;
    FileNode* m_cached_file = nullptr;
    FunctionNode* m_cached_function = nullptr;
    NodeType m_node_type;
    
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
    Node() = default;
    Node(NodeType type) : m_node_type(type) {}

    virtual ~Node() = default;

    // Запрещаем копирование (unique_ptr нельзя копировать)
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    
    // Разрешаем перемещение
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;

    
    NodeType get_node_type() const { return m_node_type; }
    const char* get_node_type_string() { return node_type_to_string(m_node_type); }
    
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

    template<typename T, typename... Args>
    T* add_child(Args&&... args) {
        // 1. Создаем узел нужного типа
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        
        // 2. Берем сырой указатель для возврата
        T* ptr = child.get();
        
        // 3. Устанавливаем связи (parent/child)
        ptr->set_parent(this); // Если у тебя есть такая логика
        
        // 4. Передаем владение в вектор детей
        m_children.push_back(std::move(child));
        
        return ptr;
    }
    
    // Безопасное приведение без dynamic_cast
    template<typename T>
    T* as() {
        // Мы проверяем тип по enum, который ты определила
        if (m_node_type == T::StaticType) {
            return static_cast<T*>(this);
        }
        return nullptr;
    }

    virtual Node* lookup(const std::string& name) {
        (void)name;
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
    virtual ProgramBinaryElement generate(GlobalState& state) = 0;
};

} // namespace sootc