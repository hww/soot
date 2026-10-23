// Node.cpp
#include "Node.hpp"
#include "GlobalNode.hpp"
#include "FileNode.hpp"
#include "FunctionNode.hpp"

namespace sootc {
    
GlobalNode* Node::global() {
    if (auto *self = dynamic_cast<GlobalNode *>(this)) { return self; }
    return find_ancestor<GlobalNode>();
}

FileNode *Node::file() {
    // Если этот узел и есть FileNode
    if (auto *self = dynamic_cast<FileNode *>(this)) { return self; }
    // Иначе ищем среди предков
    return find_ancestor<FileNode>();
}

FunctionNode* Node::function() {
    if (auto *self = dynamic_cast<FunctionNode *>(this)) { return self; }
    return find_ancestor<FunctionNode>();
}

} // namespace sootc