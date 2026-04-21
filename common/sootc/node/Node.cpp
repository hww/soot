// Node.cpp
#include "Node.hpp"
#include "GlobalNode.hpp"
#include "FileNode.hpp"
#include "FunctionNode.hpp"

namespace sootc {
    
GlobalNode* Node::global() {
    if (!m_cached_global) {
        m_cached_global = find_ancestor<GlobalNode>();
    }
    return static_cast<GlobalNode*>(m_cached_global);
}

FileNode* Node::file() {
    if (!m_cached_file) {
        m_cached_file = find_ancestor<FileNode>();
    }
    return static_cast<FileNode*>(m_cached_file);
}

FunctionNode* Node::function() {
    if (!m_cached_function) {
        m_cached_function = find_ancestor<FunctionNode>();
    }
    return static_cast<FunctionNode*>(m_cached_function);
}

} // namespace sootc