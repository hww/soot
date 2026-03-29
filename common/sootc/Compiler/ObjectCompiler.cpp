#include "common/sootc/Compiler/ObjectCompiler.hpp"
#include "common/sootc/IR/IR_Node.hpp"
#include "common/util/Log.hpp"
#include "files/RelocatableBuffer.hpp"
#include <vector>

namespace sootc {

ObjectCompiler::ObjectCompiler(TypeSystem& ts) : ts_(ts) {}

RelocatableBuffer  ObjectCompiler::compile_function(const Object& form, const std::string& func_name) {
    // Ожидаем: (lambda (args) body)
    if (!form.is_pair()) {
        lg::error("Expected lambda form, got: {}", form.print());
        return {};
    }
    
    Type* int_type = ts_.lookup_type_no_throw("int");
    Type* function_type = ts_.lookup_type_no_throw("function");
    
    if (!int_type || !function_type) {
        lg::error("Required types not found");
        return {};
    }
    
    FunctionCompiler compiler(ts_, function_type, func_name);
    
    // Распаковываем lambda форму
    auto lambda = form.as_pair();
    auto args_form = lambda->cdr.as_pair()->car;
    auto body_form = lambda->cdr.as_pair()->cdr;
    
    // Регистрируем аргументы
    int arg_index = 0;
    if (args_form.is_pair()) {
        auto current = args_form;
        while (current.is_pair()) {
            auto arg_pair = current.as_pair();
            if (arg_pair->car.is_symbol()) {
                std::string arg_name = arg_pair->car.as_symbol().c_str();
                IR_Reg* arg_reg = compiler.create_arg_reg(int_type, arg_index++);
                local_vars_[arg_name] = arg_reg;
            }
            current = arg_pair->cdr;
        }
    }
    
    // Компилируем тело
    if (body_form.is_pair()) {
        auto body = body_form.as_pair()->car;
        IR_Reg* result = compile_form(body, compiler);
        if (result) {
            compiler.add_node(std::make_unique<IR_Return>(result));
        }
    }
    
    return compiler.compile();
}

IR_Reg* ObjectCompiler::compile_form(const script::Object& form, FunctionCompiler& compiler) {
    if (form.is_number()) {
        // Литерал числа
        Type* int_type = ts_.lookup_type_no_throw("int");
        IR_Reg* reg = create_temp_reg(int_type, compiler);
        s64 val = form.as_integer();
        auto* cnst = new IR_Const(int_type, val);
        compiler.add_node(std::make_unique<IR_LoadConst>(reg, cnst));
        return reg;
    }
    else if (form.is_symbol()) {
        // Переменная
        std::string name = form.as_symbol().c_str();
        auto it = local_vars_.find(name);
        if (it != local_vars_.end()) {
            return it->second;
        }
        lg::error("Undefined variable: {}", name);
        return nullptr;
    }
    else if (form.is_pair()) {
        // Список - либо вызов функции, либо специальная форма
        auto pair = form.as_pair();
        auto first = pair->car;
        
        if (first.is_symbol()) {
            std::string op = first.as_symbol().c_str();
            
            if (op == "+") {
                return compile_binary_op(form, compiler, IR_Binary::Op::ADD);
            }
            else if (op == "-") {
                return compile_binary_op(form, compiler, IR_Binary::Op::SUB);
            }
            else if (op == "*") {
                return compile_binary_op(form, compiler, IR_Binary::Op::MUL);
            }
            else if (op == "/") {
                return compile_binary_op(form, compiler, IR_Binary::Op::DIV);
            }
            else if (op == "define") {
                return compile_define(form, compiler);
            }
            else if (op == "lambda") {
                return compile_lambda(form, compiler);
            }
            else if (op == "if") {
                return compile_if(form, compiler);
            }
            else if (op == "begin") {
                return compile_begin(form, compiler);
            }
            else if (op == "define-type") {
                
            }
            else if (op == "define-state") {

            }
            else if (op == "define-method") {

            }
            else if (op == "define") {

   
            }            
            else {
                // Обычный вызов функции
                return compile_call(form, compiler);
            }
        }
        
        lg::error("Invalid form: {}", form.print());
        return nullptr;
    }
    
    lg::error("Unsupported form type: {}", form.print());
    return nullptr;
}

IR_Reg* ObjectCompiler::compile_binary_op(const script::Object& form, FunctionCompiler& compiler, IR_Binary::Op op) {
    auto pair = form.as_pair();
    
    // Получаем аргументы
    auto args = pair->cdr;
    if (!args.is_pair()) {
        lg::error("Binary op requires two arguments");
        return nullptr;
    }
    
    auto left_form = args.as_pair()->car;
    auto right_form = args.as_pair()->cdr.as_pair()->car;
    
    Type* int_type = ts_.lookup_type_no_throw("int");
    IR_Reg* result = create_temp_reg(int_type, compiler);
    
    IR_Reg* left = compile_form(left_form, compiler);
    IR_Reg* right = compile_form(right_form, compiler);
    
    if (left && right) {
        compiler.add_node(std::make_unique<IR_Binary>(op, result, left, right));
    }
    
    return result;
}

IR_Reg* ObjectCompiler::compile_call(const script::Object& form, FunctionCompiler& compiler) {
    // (function arg1 arg2 ...)
    auto pair = form.as_pair();
    auto func_form = pair->car;
    
    if (!func_form.is_symbol()) {
        lg::error("Function call with non-symbol: {}", func_form.print());
        return nullptr;
    }
    
    std::string func_name = func_form.as_symbol().c_str();
    
    // Пока поддерживаем только внешние функции
    // TODO: вызов скомпилированных функций
    
    lg::debug("Call to function: {}", func_name);
    
    // Временная заглушка
    Type* int_type = ts_.lookup_type_no_throw("int");
    return create_temp_reg(int_type, compiler);
}

IR_Reg* ObjectCompiler::compile_define(const script::Object& form, FunctionCompiler& compiler) {
    // (define name value)
    auto pair = form.as_pair();
    auto args = pair->cdr;
    
    if (!args.is_pair()) {
        lg::error("define requires name and value");
        return nullptr;
    }
    
    auto name_form = args.as_pair()->car;
    auto value_form = args.as_pair()->cdr.as_pair()->car;
    
    if (!name_form.is_symbol()) {
        lg::error("define name must be a symbol");
        return nullptr;
    }
    
    std::string var_name = name_form.as_symbol().c_str();
    IR_Reg* value = compile_form(value_form, compiler);
    
    if (value) {
        local_vars_[var_name] = value;
    }
    
    return value;
}

IR_Reg* ObjectCompiler::compile_lambda(const script::Object& form, FunctionCompiler& compiler) {
    // (lambda (args) body)
    auto lambda = form.as_pair();
    auto args_form = lambda->cdr.as_pair()->car;
    auto body_form = lambda->cdr.as_pair()->cdr;
    
    // Сохраняем текущие локальные переменные
    auto saved_locals = local_vars_;
    
    // Регистрируем аргументы
    int arg_index = 0;
    Type* int_type = ts_.lookup_type_no_throw("int");
    
    if (args_form.is_pair()) {
        auto current = args_form;
        while (current.is_pair()) {
            auto arg_pair = current.as_pair();
            if (arg_pair->car.is_symbol()) {
                std::string arg_name = arg_pair->car.as_symbol().c_str();
                IR_Reg* arg_reg = compiler.create_arg_reg(int_type, arg_index++);
                local_vars_[arg_name] = arg_reg;
            }
            current = arg_pair->cdr;
        }
    }
    
    // Компилируем тело
    if (body_form.is_pair()) {
        auto body = body_form.as_pair()->car;
        IR_Reg* result = compile_form(body, compiler);
        if (result) {
            compiler.add_node(std::make_unique<IR_Return>(result));
        }
    }
    
    // Восстанавливаем локальные переменные
    local_vars_ = saved_locals;
    
    Type* function_type = ts_.lookup_type_no_throw("function");
    return create_temp_reg(function_type, compiler);
}

IR_Reg* ObjectCompiler::compile_if(const script::Object& form, FunctionCompiler& compiler) {
    // (if cond then else)
    lg::debug("IF form not yet implemented");
    Type* int_type = ts_.lookup_type_no_throw("int");
    return create_temp_reg(int_type, compiler);
}

IR_Reg* ObjectCompiler::compile_begin(const script::Object& form, FunctionCompiler& compiler) {
    // (begin expr1 expr2 ...)
    auto pair = form.as_pair();
    auto args = pair->cdr;
    
    IR_Reg* last_result = nullptr;
    auto current = args;
    
    while (current.is_pair()) {
        auto expr = current.as_pair()->car;
        last_result = compile_form(expr, compiler);
        current = current.as_pair()->cdr;
    }
    
    return last_result;
}

IR_Reg* ObjectCompiler::create_temp_reg(Type* type, FunctionCompiler& compiler) {
    return compiler.create_local_reg(type);
}

RelocatableBuffer ObjectCompiler::compile_file(const script::Object& forms, const std::string& module_name) {
    // Ожидаем список форм (top-level)
    if (!forms.is_pair() && !forms.is_null()) {
        lg::error("Expected list of forms");
        return {};
    }
    
    // Пока компилируем только первую форму как функцию
    if (forms.is_pair()) {
        auto first_form = forms.as_pair()->car;
        return compile_function(first_form, module_name);
    }
    
    return {};
}

} // namespace sootc