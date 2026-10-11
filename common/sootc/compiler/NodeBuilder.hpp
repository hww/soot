#pragma once

#include "script/Object.hpp"
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
        std::unique_ptr<Node> build(const script::Object &form, Node *node);

        // ========================================================================
        // Public form handlers — native signatures.
        //
        // These are the "real" builders; they preserve the return type that
        // callers (and constructors) expect. The compiler dispatch does NOT
        // call these directly: it goes through the _wrap variants below.
        // ========================================================================

        // Declarations — return unique_ptr<Node>.
        std::unique_ptr<Node> build_begin(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_define(const script::Object &form, Node *context);
        std::unique_ptr<Node> build_define_extern(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_define_native(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defmacro(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_seval(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_deftype(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defenum(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defmethod(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_lambda(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_update_macro_metadata(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defconstant(const script::Object &form, Node *node);

        // Expressions — native types.
        std::unique_ptr<ExpressionNode> build_expression(const script::Object &form, Node *node);
        std::unique_ptr<IfNode>         build_if(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode> build_cond(const script::Object &form, Node *node);
        std::unique_ptr<WhileNode>      build_while(const script::Object &form, Node *node);
        std::unique_ptr<LetNode>        build_let(const script::Object &form, Node *node);
        std::unique_ptr<SetNode>        build_set(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode> build_deref(const script::Object &form, Node *node);
        std::unique_ptr<NewNode>        build_new(const script::Object &form, Node *node);
        std::unique_ptr<BinaryNode>     build_binary(const script::Object &form, Node *node);
        std::unique_ptr<CompareNode>    build_compare(const script::Object &form, Node *node);
        std::unique_ptr<CallNode>       build_call(const script::Object &form, Node *node);
        std::unique_ptr<VariableNode>   build_variable(const script::Object &form, Node *node);
        std::unique_ptr<ConstNode>      build_const(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_abs(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_neg(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_not(const script::Object &form, Node *node);
        std::unique_ptr<ExpressionNode>    build_lognot(const script::Object &form, Node *node);

        /// @brief Build (require "path/to/file.sc").
        ///
        /// @details `require` is a LANGUAGE form: it is handled by the
        ///          compiler during compilation, not by the REPL. The
        ///          actual work (resolving the path, compiling the
        ///          file, loading the module into the active runtime)
        ///          is delegated to Compiler::require_file.
        ///
        ///          The form produces no IR: after the compiler has
        ///          loaded the required module, it continues with the
        ///          next form. An empty SequenceNode is returned.
        std::unique_ptr<Node>              build_require(const script::Object &form, Node *node);

        // ---- Helpers ----
        Type                              *parse_type(const script::Object &type_form, Node *node);
        std::vector<std::unique_ptr<Node>> parse_args(const script::Object &args_form, Node *node);
        std::unique_ptr<Node> build_body_as_sequence(const script::Object &body_forms, Node *node);
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
        using BuildMethod = std::unique_ptr<Node> (NodeBuilder::*)(const script::Object &, Node *);

        std::unique_ptr<Node> build_begin_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_define_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defmacro_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_seval_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_deftype_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defenum_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_defmethod_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_lambda_wrap(const script::Object &form, Node *node);

        std::unique_ptr<Node> build_if_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_cond_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_while_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_let_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_set_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_deref_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_new_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_binary_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_compare_wrap(const script::Object &form, Node *node);


        std::unique_ptr<ExpressionNode> build_unary_common(const script::Object &form, Node *node,
                                                           UnaryNode::Op op, const char *op_name);

        std::unique_ptr<Node> build_abs_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_neg_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_not_wrap(const script::Object &form, Node *node);
        std::unique_ptr<Node> build_lognot_wrap(const script::Object &form, Node *node);

        std::unique_ptr<Node> build_define_extern_or_native(const script::Object &form,
                                                                         Node               *node,
                                                                         bool is_native);
        // Table of built-in form handlers.
        std::unordered_map<std::string, BuildMethod> m_form_table;
        void                                         init_form_table();
    };

} // namespace sootc