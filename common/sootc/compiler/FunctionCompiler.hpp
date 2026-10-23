// FunctionCompiler.hpp
#pragma once

#include "script/Object.hpp"
#include "sootc/node/Node.hpp"
#include <memory>

namespace sootc {

class Env;
class FunctionNode;
class NodeBuilder;

namespace FunctionCompiler {

    /// @brief Compile a lambda form into a FunctionNode.
    ///
    /// @details The returned FunctionNode is ALREADY attached to the
    ///          enclosing FileNode (via add_child), and its body is
    ///          already built. Ownership belongs to the FileNode, NOT
    ///          to the caller. The returned pointer is a non-owning
    ///          observer, valid until the FileNode is destroyed.
    ///
    ///          Callers must NOT call add_child on it, and must NOT
    ///          put it into a unique_ptr that outlives the FileNode.
    std::unique_ptr<FunctionNode> compile_function(const script::Object &form, Node *node,
                                                   NodeBuilder &builder);

    // @brief Compile the arguments
    void parse_arguments(const script::Object& args_form, 
                         FunctionNode* fn, 
                         Node* node, 
                         NodeBuilder& builder);

} // namespace FunctionCompiler

} // namespace sootc