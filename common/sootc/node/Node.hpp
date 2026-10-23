// Node.hpp
#pragma once

#include "carbon/file/ProgramBinaryElement.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <type_traits>

using namespace carbon;

namespace sootc {

    class GlobalNode;
    class FileNode;
    class FunctionNode;
    class DataSegment;
    
enum class NodeType {
    Node,
    BinaryNode,
    CallNode,
    CastNode,
    CompareNode,
    ConstNode,
    ControlNode,
    ExpressionNode,
    SequenceNode,
    FileNode,
    FunctionNode,
    GlobalNode,
    IfNode,
    LetNode, 
    SetNode, 
    ReturnNode,
    VariableInfo,
    VariableNode,
    WhileNode,
    StoreGlobalNode,
    // Compile-time-only declarations:
    TypeDeclarationNode,
    StateScriptNode, 
    EnumDeclarationNode,
    // Data instances:
    NewNode,
    DataDeclarationNode,
    HeapDefineNode,
    AtNode,
    StoreFieldNode,
    StaticDefineNode
    };

inline const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NodeType::Node:           return "Node";
        case NodeType::BinaryNode:     return "BinaryNode";
        case NodeType::CallNode:       return "CallNode";
        case NodeType::CastNode:       return "CastNode";
        case NodeType::CompareNode:    return "CompareNode";
        case NodeType::ConstNode:      return "ConstNode";
        case NodeType::ControlNode:    return "ControlNode";
        case NodeType::ExpressionNode: return "ExpressionNode";
        case NodeType::SequenceNode:   return "SequenceNode";
        case NodeType::FileNode:       return "FileNode";
        case NodeType::FunctionNode:   return "FunctionNode";
        case NodeType::GlobalNode:     return "GlobalNode";
        case NodeType::IfNode:         return "IfNode";
        case NodeType::LetNode:        return "LetNode";
        case NodeType::SetNode:        return "SetNode";
        case NodeType::ReturnNode:     return "ReturnNode";
        case NodeType::VariableInfo:   return "VariableInfo";
        case NodeType::VariableNode:   return "VariableNode";
        case NodeType::WhileNode:      return "WhileNode";
        case NodeType::StoreGlobalNode: return "StoreGlobalNode";
        case NodeType::TypeDeclarationNode: return "TypeDeclarationNode";
        case NodeType::StateScriptNode: return "StateScriptNode";
        case NodeType::EnumDeclarationNode: return "EnumDeclarationNode";
        case NodeType::NewNode: return "NewNode";
        case NodeType::DataDeclarationNode: return "DataDeclarationNode";
        case NodeType::HeapDefineNode: return "HeapDefineNode";
        case NodeType::AtNode: return "AtNode";
        case NodeType::StoreFieldNode: return "StoreFieldNode";
        case NodeType::StaticDefineNode: return "StaticDefineNode";
        }
    // Если добавили NodeType, а case не написали — сюда не попадём, компилятор предупредит.
    return "UnknownNode";
}

// ============================================================================
// Модель владения детьми
// ----------------------------------------------------------------------------
// Node сам детей НЕ хранит. Каждый конкретный узел владеет своими детьми
// как типизированными членами (unique_ptr<ExpressionNode> m_left, ...) или
// собственным вектором (SequenceNode::m_exprs, FileNode::m_children, ...).
//
// Для обобщённых обходов (инспектор, оптимизации, визиторы) каждый узел
// обязан реализовать for_each_child() и перечислить в нём ВСЕХ своих детей.
//
// Инвариант parent: у каждого ребёнка parent() == узел, который им владеет.
// Выставляется ТОЛЬКО через adopt() в конструкторе / сеттере владельца.
// ============================================================================
class Node {
    Node* m_parent = nullptr;
protected:    
    NodeType                           m_node_type = NodeType::Node;

    // ---- Усыновление: единственное место, где выставляется m_parent ----
    template<typename T>
    T* adopt(const std::unique_ptr<T>& child) {
        static_assert(std::is_base_of_v<Node, T>, "T must be derived from Node");
        if (child) child->m_parent = this;
        return child.get();
    }

     template <typename T> T *adopt(const std::shared_ptr<T> &child) {
        static_assert(std::is_base_of_v<Node, T>, "T must be derived from Node");
        if (child) child->m_parent = this;
        return child.get();
    }

    template<typename T>
    void adopt_all(const std::vector<std::unique_ptr<T>>& children) {
        for (const auto& c : children) adopt(c);
    }

    // Отвязать ребёнка (когда владение уходит наружу: release_child и т.п.)
    static void orphan(Node* child) {
        if (child) child->m_parent = nullptr;
    }

public:
    Node() = default;
    Node(NodeType type) : m_node_type(type) {}

    virtual ~Node() = default;

    // Узлы адресуются по указателю: на них ссылаются parent у детей и
    // temp-регистры в FunctionNode. Ни копирование, ни перемещение не нужны.
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;
    
    NodeType get_node_type() const { return m_node_type; }
    const char* get_node_type_string() { return node_type_to_string(m_node_type); }
    
    // ========================================================================
    // Навигация по дереву
    // ========================================================================
    
    Node* parent() const { return m_parent; }

    /// @brief Вызвать f для каждого прямого ребёнка в порядке "исходника".
    /// @details Единственный обобщённый интерфейс к детям. Реализация
    ///          узла ОБЯЗАНА перечислить всех детей, которыми он владеет.
    ///          Константность поверхностная: unique_ptr::get() в const-методе
    ///          возвращает неконстантный указатель (как и BinaryNode::left()).
    ///
    ///          TODO: после миграции всех узлов заменить тело по умолчанию
    ///          на `= 0`, чтобы компилятор нашёл забытые.
    virtual void for_each_child(const std::function<void(Node &)> &f) const { (void)f; }

    std::vector<const Node *> all_children() const {
        std::vector<const Node *> out;
        for_each_child([&](Node &c) { out.push_back(&c); });
        return out;
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
    class FunctionNode* function();

    class FileNode *file();

       
    // ========================================================================
    // Виртуальные методы
    // ========================================================================
 
    virtual std::string to_string() const = 0;
    virtual const char* node_type() const = 0;
    virtual ProgramBinaryElement generate(DataSegment &) = 0;
};

} // namespace sootc