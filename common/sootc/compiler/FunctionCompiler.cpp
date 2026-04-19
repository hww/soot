// FunctionCompiler.cpp
#include "sootc/compiler/FunctionCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/ReturnNode.hpp"

namespace sootc {
namespace FunctionCompiler {

std::unique_ptr<FunctionNode> FunctionCompiler::compile_function(
    const script::Object& form, Node* context, NodeBuilder& builder) {
    
    auto rest = form.as_pair()->cdr;
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    auto fn = std::make_unique<FunctionNode>("lambda");
    
    // Парсим аргументы
    parse_arguments(args_list, fn.get(), context, builder);
    
    // Компилируем тело
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

void parse_arguments(const script::Object& args_form, 
                     FunctionNode* func_node, 
                     Node* node, 
                     NodeBuilder& builder) {
                    
    auto current = args_form;
    
    while (current.is_pair()) {
        auto arg = current.as_pair()->car;
        
        if (arg.is_symbol()) {
            std::string arg_name = arg.as_symbol();
            Type* type = builder.parse_type(arg, func_node);  // или object по умолчанию
            
            // Только добавляем параметр в функцию
            func_node->add_parameter(arg_name, type);
            
            // ❌ НЕТ bind! НЕТ env!
            // Переменная будет найдена через lookup_variable() при использовании
        }
        
        current = current.as_pair()->cdr;
    }
}

} // namespace FunctionCompiler
} // namespace sootc