#pragma once

#include "common/soot/Object.hpp"
#include "sootc/compiler/Compiler.hpp"
#include "sootc/node/BinaryNode.hpp"
#include "sootc/node/CallNode.hpp"
#include "sootc/node/CompareNode.hpp"
#include "sootc/node/ConstNode.hpp"
#include "sootc/node/IfNode.hpp"
#include "sootc/node/LetNode.hpp"
#include "sootc/node/SetNode.hpp"
#include "sootc/node/VariableNode.hpp"
#include "sootc/node/WhileNode.hpp"
#include "type_system/TypeSystem.hpp"
#include <memory>
#include <sootc/node/DerefNode.hpp>
#include <sootc/node/NewNode.hpp>

#include <string>
#include <unordered_map>
#include <sootc/node/UnaryNode.hpp>

namespace sootc {

    class Compiler;

    class NodeBuilder {
    public:
        // ---- Public entry point ----
        std::unique_ptr<Node> build(const soot::Object &form, Node *node);

        // ========================================================================
        // Public form handlers — native signatures.
        //
        // These are the "real" builders; they preserve the return type that
        // callers (and constructors) expect. The compiler dispatch does NOT
        // call these directly: it goes through the _wrap variants below.
        // ========================================================================

        // Declarations — return unique_ptr<Node>.
        std::unique_ptr<Node> build_begin(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_define(const soot::Object &form, Node *context);
        std::unique_ptr<Node> build_define_extern(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defmacro(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_seval(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_deftype(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defenum(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defmethod(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_lambda(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_update_macro_metadata(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defconstant(const soot::Object &form, Node *node);

        // Expressions — native types.
        std::unique_ptr<ExpressionNode> build_expression(const soot::Object &form, Node *node);
        std::unique_ptr<IfNode>         build_if(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode> build_cond(const soot::Object &form, Node *node);
        std::unique_ptr<WhileNode>      build_while(const soot::Object &form, Node *node);
        std::unique_ptr<LetNode>        build_let(const soot::Object &form, Node *node);
        std::unique_ptr<SetNode>        build_set(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode> build_deref(const soot::Object &form, Node *node);
        std::unique_ptr<NewNode>        build_new(const soot::Object &form, Node *node);
        std::unique_ptr<BinaryNode>     build_binary(const soot::Object &form, Node *node);
        std::unique_ptr<CompareNode>    build_compare(const soot::Object &form, Node *node);
        std::unique_ptr<CallNode>       build_call(const soot::Object &form, Node *node);
        std::unique_ptr<VariableNode>   build_variable(const soot::Object &form, Node *node);
        std::unique_ptr<ConstNode>      build_const(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_abs(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_neg(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_not(const soot::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_lognot(const soot::Object &form, Node *node);

        // ---- Helpers ----
        Type                              *parse_type(const soot::Object &type_form, Node *node);
        std::vector<std::unique_ptr<Node>> parse_args(const soot::Object &args_form, Node *node);
        std::unique_ptr<Node> build_body_as_sequence(const soot::Object &body_forms, Node *node);
        TypeSpec              build_function_signature(FunctionNode *fn, const std::string &name);

        TypeSystem &m_ts;
        Compiler   *m_compiler;

        // Constructor
        NodeBuilder(TypeSystem &ts, Compiler *compiler);

    private:
        // ========================================================================
        // Private wrappers — uniform signature for the dispatch table.
        //
        // Each wrapper simply forwards to the corresponding public builder and
        // upcasts the result to unique_ptr<Node>. This is the only place where
        // the type erasure happens; the public builders keep their native types.
        // ========================================================================
        using BuildMethod = std::unique_ptr<Node> (NodeBuilder::*)(const soot::Object &, Node *);

        std::unique_ptr<Node> build_begin_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_define_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defmacro_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_seval_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_deftype_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defenum_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_defmethod_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_lambda_wrap(const soot::Object &form, Node *node);

        std::unique_ptr<Node> build_if_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_cond_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_while_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_let_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_set_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_deref_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_new_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_binary_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_compare_wrap(const soot::Object &form, Node *node);


        std::unique_ptr<ExpressionNode> build_unary_common(const soot::Object &form, Node *node,
                                                           UnaryNode::Op op, const char *op_name);

        std::unique_ptr<Node> build_abs_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_neg_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_not_wrap(const soot::Object &form, Node *node);
        std::unique_ptr<Node> build_lognot_wrap(const soot::Object &form, Node *node);

        // Table of built-in form handlers.
        std::unordered_map<std::string, BuildMethod> m_form_table;
        void                                         init_form_table();
    };

} // namespace sootc