// FunctionCompiler.cpp
#include "sootc/compiler/FunctionCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/ReturnNode.hpp"
#include <stdexcept>

namespace sootc {
namespace FunctionCompiler {

std::unique_ptr<FunctionNode> compile_function(
    const soot::Object& form, 
    Node* node, 
    NodeBuilder& builder) {
    
    auto rest = form.as_pair()->cdr;
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    auto fn = std::make_unique<FunctionNode>("lambda");

    parse_arguments(args_list, fn.get(), node, builder);
    
    auto current = body_forms;
    std::unique_ptr<ExpressionNode> last_expr;
    
    while (current.is_pair()) {
        last_expr = builder.build_expression(current.as_pair()->car, fn.get());
        current = current.as_pair()->cdr;
    }
    
    if (last_expr) {
        auto ret = std::make_unique<ReturnNode>(std::move(last_expr));
        fn->set_body(std::move(ret));
    }

    return fn;
}
void parse_arguments(
    const soot::Object& args_form,
    FunctionNode* func_node,
    Node* node,
    NodeBuilder& builder) {
    
    (void)node;  // убираем warning
    
    auto current = args_form;
    
    while (current.is_pair()) {
        auto arg = current.as_pair()->car;
        
        if (arg.is_pair()) {
            // (a int) - параметр с типом
            auto name = arg.as_pair()->car.as_symbol();
            auto type_name = arg.as_pair()->cdr.as_pair()->car;
            Type* type = builder.parse_type(type_name, func_node);
            func_node->add_parameter(name, type);  // ← ДОЛЖНО БЫТЬ
        } else if (arg.is_symbol()) {
            // a - параметр без типа
#if ALLOW_SIMPLE_ARGUMENT_SYNTAX            
            Type* type = builder.parse_type(arg, func_node);
            func_node->add_parameter(arg.as_symbol(), type);  // ← ДОЛЖНО БЫТЬ
#else
            throw std::runtime_error(fmt::format("Invalid argument definition {}", arg.print()));
#endif
        }
        
        current = current.as_pair()->cdr;
    }
}

} // namespace FunctionCompiler
} // namespace sootc