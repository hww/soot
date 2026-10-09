// FunctionCompiler.cpp
#include "sootc/compiler/FunctionCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/ReturnNode.hpp"
#include <stdexcept>

namespace sootc {

    namespace FunctionCompiler {
    std::unique_ptr<FunctionNode> compile_function(const soot::Object &form, Node *node,
                                                   NodeBuilder &builder) {

        auto rest = form.as_pair()->cdr;

        // Пропускаем keyword-опции: :name, :immediate, :segment, :no-typecheck
        soot::Object current = rest;
        std::string  function_name;

        while (current.is_pair()) {
            auto head = current.as_pair()->car;
            if (!head.is_symbol() || !head.is_keyword()) break;

            std::string kw = head.as_symbol().name_ptr;
            current = current.as_pair()->cdr;
            if (!current.is_pair()) {
                throw builder.m_compiler->make_error(form, "FunctionCompiler::compile_function")
                    .expected("value after " + kw)
                    .got("end of form");
            }
            auto value = current.as_pair()->car;
            current = current.as_pair()->cdr;

            if (kw == ":name") {
                if (value.is_symbol() || value.is_string()) {
                    function_name = value.to_std_string();
                }
            }
            // :immediate, :segment, :no-typecheck — игнорируем
        }

        if (!current.is_pair()) {
            throw builder.m_compiler->make_error(form, "FunctionCompiler::compile_function")
                .expected("parameter list after keyword options")
                .got("end of form");
        }

        auto args_list = current.as_pair()->car;
        auto body_forms = current.as_pair()->cdr;

        auto fn = std::make_unique<FunctionNode>(function_name.empty() ? "lambda" : function_name);

        parse_arguments(args_list, fn.get(), node, builder);

        auto                            current_body = body_forms;
        std::unique_ptr<ExpressionNode> last_expr;

        while (current_body.is_pair()) {
            last_expr = builder.build_expression(current_body.as_pair()->car, fn.get());
            current_body = current_body.as_pair()->cdr;
        }

        if (last_expr) {
            // Save return type before moving `last_expr` into the ReturnNode.
            Type *return_type = last_expr->get_type();
            if (return_type) { fn->set_return_type(return_type); }

            auto ret = std::make_unique<ReturnNode>(std::move(last_expr));
            fn->set_body(std::move(ret));
        }

        return fn;
    }
    void parse_arguments(const soot::Object &args_form, FunctionNode *func_node, Node *node,
                         NodeBuilder &builder) {

        (void)node; // убираем warning

        auto current = args_form;

        while (current.is_pair()) {
            auto arg = current.as_pair()->car;

            if (arg.is_pair()) {
                // (a int) - параметр с типом
                auto  name = arg.as_pair()->car.as_symbol();
                auto  type_name = arg.as_pair()->cdr.as_pair()->car;
                Type *type = builder.parse_type(type_name, func_node);
                func_node->add_parameter(name, type); // ← ДОЛЖНО БЫТЬ
            } else if (arg.is_symbol()) {

#if ALLOW_SIMPLE_ARGUMENT_SYNTAX
                Type *type = builder.parse_type(arg, func_node);
                func_node->add_parameter(arg.as_symbol(), type); // ← ДОЛЖНО БЫТЬ
#else
                throw std::runtime_error(
                    fmt::format("Invalid argument definition {}", arg.print()));
#endif
            }

            current = current.as_pair()->cdr;
        }
    }

} // namespace FunctionCompiler
} // namespace sootc