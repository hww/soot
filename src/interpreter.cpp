#include "interpreter.h"
#include <sstream>
#include "libs/linenoise/linenoise.h"

Interpreter::Interpreter() {
    // Инициализируем специальные формы
    special_forms = {
        {"quote", &Interpreter::eval_quote},
        {"define", &Interpreter::eval_define},
        {"lambda", &Interpreter::eval_lambda},
        {"begin", &Interpreter::eval_begin},
        {"print", &Interpreter::eval_print},
        {"cons", &Interpreter::eval_cons},
        {"car", &Interpreter::eval_car},
        {"cdr", &Interpreter::eval_cdr}
    };

    // Инициализируем встроенные функции
    builtin_forms = {
        {"+", &Interpreter::eval_plus},
        {"-", &Interpreter::eval_minus},
        {"*", &Interpreter::eval_times},
        {"print", &Interpreter::eval_print_builtin},
        {"cons", &Interpreter::eval_cons_builtin},
        {"car", &Interpreter::eval_car_builtin},
        {"cdr", &Interpreter::eval_cdr_builtin}
    };
}

// Вспомогательные методы
Arguments Interpreter::get_args(const Object& form, const Object& rest, const ArgumentSpec& spec) {
    Arguments args;
    Object current = rest;
    while (current.is_pair()) {
        args.unnamed.push_back(current.car());
        current = current.cdr();
    }
    return args;
}

void Interpreter::eval_args(Arguments* args, const std::shared_ptr<EnvironmentObject>& env) {
    for (auto& arg : args->unnamed) {
        arg = eval_with_rewind(arg, env);
    }
}

ArgumentSpec Interpreter::make_varargs() {
    ArgumentSpec spec;
    spec.varargs = true;
    return spec;
}

std::vector<Object> Interpreter::eval_list(const Object& list, const std::shared_ptr<EnvironmentObject>& env) {
    std::vector<Object> result;
    Object current = list;
    
    while (current.is_pair()) {
        result.push_back(eval_with_rewind(current.car(), env));
        current = current.cdr();
    }
    
    if (!current.is_empty_list()) {
        throw_eval_error(list, "malformed argument list");
    }
    
    return result;
}

Object Interpreter::intern(const std::string& name) {
    return reader.get_symbol_table().intern(name);
}

bool Interpreter::try_symbol_lookup(const Object& sym, const std::shared_ptr<EnvironmentObject>& env, Object* dest) {
    // Булевы значения жестко закодированы
    if (sym.is_symbol()) {
        auto sym_name = std::static_pointer_cast<SymbolObject>(sym.heap_obj)->name;
        if (sym_name == "#t" || sym_name == "#f") {
            *dest = sym;
            return true;
        }
    }
    
    // Упрощенная версия - ищем только в глобальных переменных
    // В реальной реализации нужно искать в переданном env и его родителях
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

// Основные методы оценки
Object Interpreter::eval(const Object& obj, const std::shared_ptr<EnvironmentObject>& env) {
    switch (obj.type) {
        case ObjectType::SYMBOL:
            return eval_symbol(obj, env);
        case ObjectType::PAIR:
            return eval_pair(obj, env);
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

Object Interpreter::eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env) {
    try {
        return eval(obj, env);
    } catch (std::runtime_error& e) {
        std::cout << "-----------------------------------------\n";
        std::cout << "From object " << obj.inspect() << "\n";
        throw;
    }
}

Object Interpreter::eval_symbol(const Object& sym, const std::shared_ptr<EnvironmentObject>& env) {
    Object result;
    if (!try_symbol_lookup(sym, env, &result)) {
        throw_eval_error(sym, "symbol is not defined");
    }
    return result;
}

Object Interpreter::eval_pair(const Object& obj, const std::shared_ptr<EnvironmentObject>& env) {
    auto pair = obj.as_pair();
    Object head = pair->car;
    Object rest = pair->cdr;

    // Сначала проверяем специальные формы
    if (head.is_symbol()) {
        auto head_sym = std::static_pointer_cast<SymbolObject>(head.heap_obj)->name;

        // 1. Проверяем специальные формы
        auto kv_sf = special_forms.find(head_sym);
        if (kv_sf != special_forms.end()) {
            return ((*this).*(kv_sf->second))(obj, rest, env);
        }

        // 2. Проверяем встроенные функции
        auto kv_b = builtin_forms.find(head_sym);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }

    // 3. Если не нашли - пробуем оценить голову и применить
    Object evaluated_head = eval_with_rewind(head, env);
    
    if (evaluated_head.is_symbol()) {
        // Символ может быть встроенной функцией, проверяем еще раз
        auto head_sym = std::static_pointer_cast<SymbolObject>(evaluated_head.heap_obj)->name;
        auto kv_b = builtin_forms.find(head_sym);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }
    
    throw_eval_error(obj, "cannot apply non-function object");
    return Object::make_empty_list();
}

// Специальные формы
Object Interpreter::eval_quote(const Object& form, const Object& rest, 
                              const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!rest.is_pair()) {
        throw_eval_error(form, "quote requires one argument");
    }
    return rest.car();
}

Object Interpreter::eval_define(const Object& form, const Object& rest,
                               const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "define requires arguments");
    }
    
    Object name_obj = rest.car();
    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "define name must be a symbol");
    }
    
    Object value_part = rest.cdr();
    if (!value_part.is_pair()) {
        throw_eval_error(form, "define must have a value");
    }
    
    Object value = eval_with_rewind(value_part.car(), env);
    auto sym_ptr = std::static_pointer_cast<SymbolObject>(name_obj.heap_obj).get();
    global_vars[sym_ptr] = value;
    return value;
}

Object Interpreter::eval_lambda(const Object& form, const Object& rest,
                               const std::shared_ptr<EnvironmentObject>& env) {
    (void)form; (void)rest; (void)env;
    // Упрощенная версия - пока возвращаем символ lambda
    return intern("lambda");
}

Object Interpreter::eval_begin(const Object& form, const Object& rest,
                              const std::shared_ptr<EnvironmentObject>& env) {
    Object current = rest;
    Object result = Object::make_empty_list();
    
    while (current.is_pair()) {
        result = eval_with_rewind(current.car(), env);
        current = current.cdr();
    }
    
    return result;
}

Object Interpreter::eval_print(const Object& form, const Object& rest,
                              const std::shared_ptr<EnvironmentObject>& env) {
    Object current = rest;
    
    while (current.is_pair()) {
        Object arg = eval_with_rewind(current.car(), env);
        std::cout << arg.print();
        if (current.cdr().is_pair()) {
            std::cout << " ";
        }
        current = current.cdr();
    }
    std::cout << std::endl;
    
    return Object::make_empty_list();
}

Object Interpreter::eval_cons(const Object& form, const Object& rest,
                             const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "cons requires two arguments");
    }
    
    Object first = eval_with_rewind(rest.car(), env);
    Object second_part = rest.cdr();
    if (!second_part.is_pair()) {
        throw_eval_error(form, "cons requires two arguments");
    }
    
    Object second = eval_with_rewind(second_part.car(), env);
    return Object::make_pair(first, second);
}

Object Interpreter::eval_car(const Object& form, const Object& rest,
                            const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "car requires one argument");
    }
    
    Object arg = eval_with_rewind(rest.car(), env);
    if (!arg.is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    
    return arg.car();
}

Object Interpreter::eval_cdr(const Object& form, const Object& rest,
                            const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "cdr requires one argument");
    }
    
    Object arg = eval_with_rewind(rest.car(), env);
    if (!arg.is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    
    return arg.cdr();
}

// Встроенные функции
Object Interpreter::eval_plus(const Object& form, Arguments& args,
                             const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    IntType result = 0;
    for (const auto& arg : args.unnamed) {
        if (!arg.is_integer()) {
            throw_eval_error(form, "+ requires integer arguments");
        }
        result += arg.as_integer();
    }
    return Object::make_integer(result);
}

Object Interpreter::eval_minus(const Object& form, Arguments& args,
                              const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        throw_eval_error(form, "- requires at least one argument");
    }
    if (args.unnamed.size() == 1) {
        return Object::make_integer(-args.unnamed[0].as_integer());
    }
    
    IntType result = args.unnamed[0].as_integer();
    for (size_t i = 1; i < args.unnamed.size(); ++i) {
        result -= args.unnamed[i].as_integer();
    }
    return Object::make_integer(result);
}

Object Interpreter::eval_times(const Object& form, Arguments& args,
                              const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    IntType result = 1;
    for (const auto& arg : args.unnamed) {
        if (!arg.is_integer()) {
            throw_eval_error(form, "* requires integer arguments");
        }
        result *= arg.as_integer();
    }
    return Object::make_integer(result);
}

Object Interpreter::eval_print_builtin(const Object& form, Arguments& args,
                                      const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    for (const auto& arg : args.unnamed) {
        std::cout << arg.print();
        if (&arg != &args.unnamed.back()) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;
    return Object::make_empty_list();
}

Object Interpreter::eval_cons_builtin(const Object& form, Arguments& args,
                                     const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "cons requires two arguments");
    }
    return Object::make_pair(args.unnamed[0], args.unnamed[1]);
}

Object Interpreter::eval_car_builtin(const Object& form, Arguments& args,
                                    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    return args.unnamed[0].car();
}

Object Interpreter::eval_cdr_builtin(const Object& form, Arguments& args,
                                    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    return args.unnamed[0].cdr();
}

// Обработка ошибок
void Interpreter::throw_eval_error(const Object& o, const std::string& err) {
    throw std::runtime_error("[Z80-Lisp] Evaluation error on " + o.print() + ": " + err);
}

// REPL
#include "libs/linenoise/linenoise.h"

// REPL
void Interpreter::execute_repl() {
    want_exit = false;
    
    // Инициализация linenoise
    linenoiseHistorySetMaxLen(100);
    linenoiseSetMultiLine(1);
    
    std::cout << "Z80 Lisp REPL (type 'quit' to exit, Ctrl+D to quit)\n";
    std::cout << "Features: line editing, history (↑↓), multi-line input\n";
    
    while (!want_exit) {
        std::string input;
        bool complete = false;
        
        // Многострочный ввод
        while (!complete) {
            char* line;
            if (input.empty()) {
                line = linenoise("z80-lisp> ");
            } else {
                line = linenoise("...> ");  // Продолжение для многострочного ввода
            }
            
            if (line == nullptr) {
                // Ctrl+D
                if (!input.empty()) {
                    std::cout << "Input cancelled.\n";
                    input.clear();
                    continue;
                } else {
                    want_exit = true;
                    break;
                }
            }
            
            std::string line_str(line);
            free(line);
            
            if (line_str.empty()) {
                // Пустая строка - проверяем завершенность
                complete = reader.is_input_complete(input);
                if (!complete) {
                    continue; // Ждем продолжения
                }
            } else {
                input += line_str + "\n";
                complete = reader.is_input_complete(input);
            }
            
            // Проверяем команды выхода
            if (input.find("quit") != std::string::npos || 
                input.find("exit") != std::string::npos) {
                want_exit = true;
                break;
            }
        }
        
        if (want_exit) break;
        if (input.empty()) continue;
        
        try {
            // Убираем последний \n если есть
            if (!input.empty() && input.back() == '\n') {
                input.pop_back();
            }
            
            // Добавляем в историю если не пустая
            linenoiseHistoryAdd(input.c_str());
            
            Object code = reader.read_from_string(input, "REPL input");
            
            // Создаем временное окружение для REPL
            auto repl_env = std::make_shared<EnvironmentObject>();
            
            Object result = eval_with_rewind(code, repl_env);
            
            if (!result.is_empty_list() || input != "()") {
                std::cout << "=> " << result.print() << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Goodbye!\n";
}