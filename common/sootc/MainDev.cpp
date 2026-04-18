// main.cpp - пример
#include "sootc/tree/BinaryNode.hpp"
#include "sootc/tree/ConstNode.hpp"
#include "tree/GlobalNode.hpp"
#include "tree/FileNode.hpp"
#include "tree/FunctionNode.hpp"
#include "tree/ExpressionNode.hpp"

int main() {
    using namespace sootc;
    
    // 1. Создаем дерево
    auto global = std::make_unique<GlobalNode>();
    auto file = std::make_unique<FileNode>("main.dc");
    auto fn = std::make_unique<FunctionNode>("main");
    
    // 2. Строим дерево
    global->add_child(std::move(file));
    global->file()->add_child(std::move(fn));
    
    // 3. Создаем тело функции: (5 + 3)
    auto five = ConstNode::make_int(5);
    auto three = ConstNode::make_int(3);
    auto add = std::make_unique<BinaryNode>(BinaryNode::Op::ADD, five.get(), three.get());
    
    // 4. Добавляем тело в функцию
    auto* fn_ptr = global->file()->function();
    fn_ptr->add_child(std::move(five));
    fn_ptr->add_child(std::move(three));
    fn_ptr->add_child(std::move(add));
    
    // 5. Генерируем код
    fn_ptr->set_body(std::move(add));  // add это unique_ptr<BinaryNode>
    fn_ptr->emit_body();  // без параметров
    
    // 6. Получаем результат
    auto binary = fn_ptr->build_binary("main");
    
    return 0;
}