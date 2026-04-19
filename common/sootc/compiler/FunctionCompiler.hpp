// FunctionCompiler.hpp
#pragma once

#include "sooti/Object.hpp"
#include "sootc/node/Node.hpp"
#include <memory>

namespace sootc {

class Env;
class FunctionNode;
class NodeBuilder;

namespace FunctionCompiler {

// Компилирует lambda/function формы в FunctionNode
std::unique_ptr<FunctionNode> compile_function(const script::Object& form, 
                                                Node* node, 
                                                NodeBuilder& builder);

// Парсит аргументы функции
void parse_arguments(const script::Object& args_form, 
                     FunctionNode* fn, 
                     Node* node, 
                     NodeBuilder& builder);

} // namespace FunctionCompiler

} // namespace sootc