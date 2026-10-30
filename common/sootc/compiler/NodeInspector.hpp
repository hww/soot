#pragma once

#include "sootc/node/Node.hpp"

#include <ostream>
#include <string>

namespace sootc {

    /// @brief Pretty-print an AST subtree, with per-node fields.
    ///
    /// @details Output format (sexpr-like, indented):
    ///
    ///     (FileNode name="<repl-0>" export_all=#t symbols=3 children=2
    ///       (FunctionNode name="get" params=0 locals=0 method_of=""
    ///         (SequenceNode
    ///           (MethodCallNode full_name="acc-add-twice" is_native=#f
    ///             (VariableNode name="a" type="acc" is_heap=#t elem="")
    ///             (ConstNode int=5)))
    ///       ...)
    ///
    ///          Every node prints its own fields, then its children. Nodes
    ///          whose field values are pointers print the resolved name, not
    ///          the address (e.g. type="acc", not type=0x...).
    ///
    ///          The printer does NOT recurse into Compiler/TypeSystem; it
    ///          only uses what is reachable from the node itself. For
    ///          `Type*` it prints `get_name()` or `(null)`. For `Node*`
    ///          it prints `node_type()` and, if it's a named node,
    ///          `name()`.
    ///
    /// @param root   The subtree to print. May be null.
    /// @param os     Output stream.
    /// @param indent Initial indentation (number of spaces).
    void inspect_ast(const Node *root, std::ostream &os, int indent = 0);

    /// @brief Convenience overload: print to stdout.
    void inspect_ast(const Node *root);

} // namespace sootc