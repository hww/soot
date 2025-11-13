#include "interpreter.h"
#include <sstream>

Interpreter::Interpreter() {
    // Инициализация может быть добавлена позже
}

Object Interpreter::eval(const Object& obj) {
    switch (obj.type) {
        case ObjectType::SYMBOL:
            return eval_symbol(obj);
        case ObjectType::PAIR:
            return eval_pair(obj);
        case ObjectType::INTEGER:
        case ObjectType::FLOAT:
        case ObjectType::STRING:
        case ObjectType::CHAR:
        case ObjectType::BOOLEAN:
        case ObjectType::EMPTY_LIST:
            return obj;
        default:
            throw_eval_error(obj, "cannot evaluate this object");
    }
    return Object::make_empty_list(); 
}
Object Interpreter::eval_pair(const Object& obj) {
    if (!obj.is_pair()) {
        throw_eval_error(obj, "expected pair");
    }
    
    Object head = obj.car();
    Object rest = obj.cdr();
    
    // СНАЧАЛА ОЦЕНИТЕ ГОЛОВУ!
    Object evaluated_head = eval_with_rewind(head);
    
    std::cout << "DEBUG: head=" << head.print() << ", evaluated_head=" << evaluated_head.print() 
              << ", is_symbol=" << evaluated_head.is_symbol() << std::endl;
    
    // Проверяем специальные формы по evaluated_head
    if (evaluated_head.is_symbol()) {
        auto head_sym = std::static_pointer_cast<SymbolObject>(evaluated_head.heap_obj)->name;
        
        std::cout << "DEBUG: head_sym='" << head_sym << "'" << std::endl;
        
        if (head_sym == "quote") {
            std::cout << "DEBUG: calling eval_quote" << std::endl;
            return eval_quote(obj, rest);
        } else if (head_sym == "define") {
            return eval_define(obj, rest);
        } else if (head_sym == "begin") {
            return eval_begin(obj, rest);
        } else if (head_sym == "print") {
            return eval_print(obj, rest);
        } else if (head_sym == "cons") {
            return eval_cons(obj, rest);
        } else if (head_sym == "car") {
            return eval_car(obj, rest);
        } else if (head_sym == "cdr") {
            return eval_cdr(obj, rest);
        } else if (head_sym == "+") {
            std::cout << "DEBUG: calling eval_plus" << std::endl;
            return eval_plus(obj, rest);
        }
    }
    
    std::cout << "DEBUG: falling back to eval_application" << std::endl;
    // Если не специальная форма, оцениваем как вызов функции
    return eval_application(evaluated_head, rest, obj);
}
Object Interpreter::eval_with_rewind(const Object& obj) {
    try {
        return eval(obj);
    } catch (std::runtime_error& e) {
        std::cout << "-----------------------------------------\n";
        std::cout << "From object " << obj.inspect() << "\n";
        throw;
    }
}

bool Interpreter::try_symbol_lookup(const Object& sym, Object* dest) {
    // Булевы значения жестко закодированы
    if (sym.is_symbol()) {
        auto sym_name = std::static_pointer_cast<SymbolObject>(sym.heap_obj)->name;
        if (sym_name == "#t" || sym_name == "#f") {
            *dest = sym;
            return true;
        }
    }
    
    // Ищем в глобальных переменных
    if (sym.is_symbol()) {
        auto sym_ptr = std::static_pointer_cast<SymbolObject>(sym.heap_obj).get();
        auto it = global_vars.find(sym_ptr);
        if (it != global_vars.end()) {
            *dest = it->second;
            return true;
        }
    }
    
    return false;
}

Object Interpreter::eval_symbol(const Object& sym) {
    Object result;
    if (!try_symbol_lookup(sym, &result)) {
        throw_eval_error(sym, "symbol is not defined");
    }
    return result;
}


Object Interpreter::eval_application(const Object& function, const Object& args, const Object& form) {
    // Сначала оцениваем функцию
    Object evaluated_func = eval_with_rewind(function);
    
    // ПРОВЕРКА: является ли evaluated_func функцией?
    if (!evaluated_func.is_symbol()) {  // Пока проверяем только символы
        throw_eval_error(function, "cannot apply non-function object: " + evaluated_func.print());
    }
    
    // Затем оцениваем аргументы
    std::vector<Object> evaluated_args = eval_list(args);
    
    // Применяем функцию
    return apply_function(evaluated_func, evaluated_args);
}

std::vector<Object> Interpreter::eval_list(const Object& list) {
    std::vector<Object> result;
    Object current = list;
    
    while (current.is_pair()) {
        result.push_back(eval_with_rewind(current.car()));
        current = current.cdr();
    }
    
    if (!current.is_empty_list()) {
        throw_eval_error(list, "malformed argument list");
    }
    
    return result;
}

Object Interpreter::apply_function(const Object& function, const std::vector<Object>& args) {
    // Если это символ - возможно встроенная функция
    if (function.is_symbol()) {
        auto sym_name = std::static_pointer_cast<SymbolObject>(function.heap_obj)->name;
        return eval_builtin(sym_name, args, function);
    }
    
    // TODO: добавить обработку лямбда-функций
    
    throw_eval_error(function, "cannot apply non-function object");
    return Object::make_empty_list();
}

Object Interpreter::eval_builtin(const std::string& name, const std::vector<Object>& args, const Object& form) {
    if (name == "+") {
        if (args.empty()) {
            return Object::make_integer(0);
        }
        
        IntType result = 0;
        for (const auto& arg : args) {
            if (!arg.is_integer()) {
                throw_eval_error(form, "+ requires integer arguments");
            }
            result += arg.as_integer();
        }
        return Object::make_integer(result);
    }
    else if (name == "-") {
        if (args.empty()) {
            throw_eval_error(form, "- requires at least one argument");
        }
        if (args.size() == 1) {
            return Object::make_integer(-args[0].as_integer());
        }
        
        IntType result = args[0].as_integer();
        for (size_t i = 1; i < args.size(); ++i) {
            result -= args[i].as_integer();
        }
        return Object::make_integer(result);
    }
    else if (name == "*") {
        IntType result = 1;
        for (const auto& arg : args) {
            if (!arg.is_integer()) {
                throw_eval_error(form, "* requires integer arguments");
            }
            result *= arg.as_integer();
        }
        return Object::make_integer(result);
    }
    
    throw_eval_error(form, "unknown function: " + name);
}

// === РЕАЛИЗАЦИИ ФОРМ ===

Object Interpreter::eval_quote(const Object& form, const Object& rest) {
    (void)form;
    if (!rest.is_pair()) {
        throw_eval_error(form, "quote must have one argument");
    }
    return rest.car();
}

Object Interpreter::eval_define(const Object& form, const Object& rest) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "define must have arguments");
    }
    
    Object name_obj = rest.car();
    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "define name must be a symbol");
    }
    
    Object value_part = rest.cdr();
    if (!value_part.is_pair()) {
        throw_eval_error(form, "define must have a value");
    }
    
    Object value = eval_with_rewind(value_part.car());
    auto sym_ptr = std::static_pointer_cast<SymbolObject>(name_obj.heap_obj).get();
    global_vars[sym_ptr] = value;
    return value;
}

Object Interpreter::eval_begin(const Object& form, const Object& rest) {
    (void)form;
    Object current = rest;
    Object result = Object::make_empty_list();
    
    while (current.is_pair()) {
        result = eval_with_rewind(current.car());
        current = current.cdr();
    }
    
    return result;
}

Object Interpreter::eval_print(const Object& form, const Object& rest) {
    (void)form;
    Object current = rest;
    
    while (current.is_pair()) {
        Object arg = eval_with_rewind(current.car());
        std::cout << arg.print();
        if (current.cdr().is_pair()) {
            std::cout << " ";
        }
        current = current.cdr();
    }
    std::cout << std::endl;
    
    return Object::make_empty_list();
}

Object Interpreter::eval_cons(const Object& form, const Object& rest) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "cons requires two arguments");
    }
    
    Object first = eval_with_rewind(rest.car());
    Object second_part = rest.cdr();
    if (!second_part.is_pair()) {
        throw_eval_error(form, "cons requires two arguments");
    }
    
    Object second = eval_with_rewind(second_part.car());
    return Object::make_pair(first, second);
}

Object Interpreter::eval_car(const Object& form, const Object& rest) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "car requires one argument");
    }
    
    Object arg = eval_with_rewind(rest.car());
    if (!arg.is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    
    return arg.car();
}

Object Interpreter::eval_cdr(const Object& form, const Object& rest) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "cdr requires one argument");
    }
    
    Object arg = eval_with_rewind(rest.car());
    if (!arg.is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    
    return arg.cdr();
}

Object Interpreter::eval_plus(const Object& form, const Object& rest) {
    Object current = rest;
    IntType result = 0;
    
    while (current.is_pair()) {
        Object arg = eval_with_rewind(current.car());
        if (!arg.is_integer()) {
            throw_eval_error(form, "+ requires integer arguments");
        }
        result += arg.as_integer();
        current = current.cdr();
    }
    
    return Object::make_integer(result);
}

void Interpreter::throw_eval_error(const Object& o, const std::string& err) {
    throw std::runtime_error("[Z80-Lisp] Evaluation error on " + o.print() + ": " + err);
}

void Interpreter::execute_repl() {
    want_exit = false;
    std::string input;
    std::string accumulated_input;
    
    std::cout << "Z80 Lisp REPL (type 'quit' to exit)\n";
    std::cout << "Multi-line input supported - close all parens and strings to execute\n\n";
    
    while (!want_exit) {
        try {
            if (accumulated_input.empty()) {
                std::cout << "z80-lisp> ";
            } else {
                std::cout << "......... ";  // Показываем что ждем продолжение
            }
            
            if (!std::getline(std::cin, input)) {
                break; // EOF
            }
            
            if (input.empty()) {
                // Пустая строка в середине многострочного ввода - игнорируем
                if (!accumulated_input.empty()) {
                    continue;
                }
                // Пустая строка в начале - ничего не делаем
                continue;
            }
            
            if (input == "quit" || input == "exit") break;
            
            // Добавляем к накопленному вводу
            if (!accumulated_input.empty()) {
                accumulated_input += "\n";
            }
            accumulated_input += input;
            
            // Проверяем завершен ли ввод
            if (reader.is_input_complete(accumulated_input)) {
                try {
                    Object code = reader.read_from_string(accumulated_input, "REPL input");
                    std::cout << "DEBUG parsed: " << code.print() << std::endl;
                    std::cout << "DEBUG parsed inspect: " << code.inspect() << std::endl;
                    Object result = eval_with_rewind(code);
                    
                    // Выводим результат только если он не пустой список ИЛИ если ввод не был просто "()"
                    if (!result.is_empty_list() || accumulated_input.find_first_not_of("() \t\n") != std::string::npos) {
                        std::cout << result.print() << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
                
                accumulated_input.clear(); // Сбрасываем для следующего ввода
            }
            // else: продолжаем накапливать ввод (НЕ выполняем код)
            
        } 
        catch (const std::runtime_error& e) {
            std::cout << "Error: " << e.what() << std::endl;
            accumulated_input.clear(); // Сбрасываем при ошибке
        }
        catch (const std::exception& e) {
            std::cout << "Unexpected error: " << e.what() << std::endl;
            accumulated_input.clear(); // Сбрасываем при ошибке
        }
    }
    
    std::cout << "Goodbye!\n";
}