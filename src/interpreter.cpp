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
        {"/", &Interpreter::eval_divide}, 
        {"print", &Interpreter::eval_print_builtin},
        {"cons", &Interpreter::eval_cons_builtin},
        {"car", &Interpreter::eval_car_builtin},
        {"cdr", &Interpreter::eval_cdr_builtin}
        
    };
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
    
    // ВАЖНО: Сначала ищем в переданном окружении env
    if (env && sym.is_symbol()) {
        try {
            *dest = env->get(std::static_pointer_cast<SymbolObject>(sym.heap_obj)->name);
            return true;
        } catch (const std::runtime_error&) {
            // Не найдено в env, продолжаем поиск
        }
    }
    
    // Затем ищем в глобальных переменных
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
    std::cout << "=== DEBUG eval_pair START ===" << std::endl;
    std::cout << "obj: " << obj.print() << std::endl;
    std::cout << "obj.inspect(): " << obj.inspect() << std::endl;
    
    auto pair = obj.as_pair();
    Object head = pair->car;
    Object rest = pair->cdr;

    std::cout << "head: " << head.print() << " type: " << (int)head.type << std::endl;
    std::cout << "rest: " << rest.print() << std::endl;

    // Сначала проверяем специальные формы
    if (head.is_symbol()) {
        auto head_sym = std::static_pointer_cast<SymbolObject>(head.heap_obj)->name;
        std::cout << "head is symbol: " << head_sym << std::endl;

        auto kv_sf = special_forms.find(head_sym);
        if (kv_sf != special_forms.end()) {
            std::cout << "found special form: " << head_sym << std::endl;
            return ((*this).*(kv_sf->second))(obj, rest, env);
        }

        auto kv_b = builtin_forms.find(head_sym);
        if (kv_b != builtin_forms.end()) {
            std::cout << "found builtin: " << head_sym << std::endl;
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }

    // Оцениваем голову
    std::cout << "evaluating head..." << std::endl;
    Object evaluated_head = eval_with_rewind(head, env);
    std::cout << "evaluated_head: " << evaluated_head.print() << " type: " << (int)evaluated_head.type << std::endl;

    // Проверяем лямбду
    if (evaluated_head.type == ObjectType::LAMBDA) {
        std::cout << "APPLYING LAMBDA!" << std::endl;
        auto lambda = std::static_pointer_cast<LambdaObject>(evaluated_head.heap_obj);
        
        std::cout << "lambda parameters: " << lambda->parameters.size() << std::endl;
        std::cout << "lambda body: " << lambda->body.print() << std::endl;
        
        // Вычисляем аргументы
        Arguments args = get_args(obj, rest, make_varargs());
        std::cout << "args before eval: " << args.unnamed.size() << std::endl;
        eval_args(&args, env);
        std::cout << "args after eval: " << args.unnamed.size() << std::endl;
        
        // Проверяем количество аргументов
        if (args.unnamed.size() != lambda->parameters.size()) {
            throw_eval_error(obj, "lambda: wrong number of arguments");
        }
        
        // Создаем новое окружение для вызова
        auto call_env = std::make_shared<EnvironmentObject>(lambda->closure_env);
        
        // Связываем параметры - ДОБАВИМ ОТЛАДКУ
        std::cout << "DEBUG: Binding parameters:" << std::endl;
        for (size_t i = 0; i < lambda->parameters.size(); ++i) {
            std::cout << "  " << lambda->parameters[i] << " = " << args.unnamed[i].print() << std::endl;
            call_env->set(lambda->parameters[i], args.unnamed[i]);
        }
        
        // Проверим что символ действительно связан
        std::cout << "DEBUG: Checking if 'n' exists in call_env..." << std::endl;
        try {
            Object test_n = call_env->get("n");
            std::cout << "DEBUG: n = " << test_n.print() << " (found in call_env)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "DEBUG: n NOT found in call_env: " << e.what() << std::endl;
        }
        
        // Выполняем тело лямбды
        std::cout << "evaluating lambda body..." << std::endl;
        Object result = eval_with_rewind(lambda->body, call_env);
        std::cout << "lambda result: " << result.print() << std::endl;
        return result;
    }
    
    // Проверяем встроенные функции после оценки
    if (evaluated_head.is_symbol()) {
        auto head_sym = std::static_pointer_cast<SymbolObject>(evaluated_head.heap_obj)->name;
        std::cout << "evaluated_head is symbol: " << head_sym << std::endl;
        auto kv_b = builtin_forms.find(head_sym);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }
    
    std::cout << "=== DEBUG eval_pair FAILED ===" << std::endl;
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
    std::cout << "=== DEBUG eval_lambda START ===" << std::endl;
    std::cout << "form: " << form.print() << std::endl;
    std::cout << "rest: " << rest.print() << std::endl;
    
    if (rest.is_empty_list()) {
        throw_eval_error(form, "lambda: expected parameter list and body");
    }
    
    // Получаем список параметров
    Object params_obj = rest.car();
    Object body_obj = rest.cdr();
    
    std::cout << "params_obj: " << params_obj.print() << std::endl;
    std::cout << "body_obj: " << body_obj.print() << std::endl;
    
    if (!params_obj.is_list()) {
        throw_eval_error(form, "lambda: parameter list must be a list");
    }
    
    // Парсим параметры
    std::vector<std::string> parameters;
    Object current_param = params_obj;
    while (!current_param.is_empty_list()) {
        if (!current_param.is_pair()) {
            throw_eval_error(form, "lambda: malformed parameter list");
        }
        
        Object param = current_param.car();
        if (!param.is_symbol()) {
            throw_eval_error(form, "lambda: parameters must be symbols");
        }
        
        parameters.push_back(param.as_symbol());
        current_param = current_param.cdr();
    }
    
    // Тело лямбды - это просто body_obj.car()
    Object body = body_obj.car();
    
    std::cout << "DEBUG eval_lambda: parameters = ";
    for (const auto& p : parameters) {
        std::cout << p << " ";
    }
    std::cout << std::endl;
    std::cout << "DEBUG eval_lambda: body = " << body.print() << std::endl;
    
    // Создаем лямбда-объект с замыканием
    Object result = Object::make_lambda(parameters, body, env);
    std::cout << "DEBUG eval_lambda: result = " << result.print() << std::endl;
    std::cout << "=== DEBUG eval_lambda END ===" << std::endl;
    
    return result;
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

Object Interpreter::eval_divide(const Object& form, Arguments& args,
                               const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        throw_eval_error(form, "/ requires at least one argument");
    }
    
    // Проверяем тип первого аргумента
    if (args.unnamed[0].is_integer()) {
        if (args.unnamed.size() == 1) {
            IntType val = args.unnamed[0].as_integer();
            if (val == 0) {
                throw_eval_error(form, "/: division by zero");
            }
            return Object::make_integer(1 / val);
        }
        
        IntType result = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_integer()) {
                throw_eval_error(form, "/: all arguments must be integers");
            }
            IntType divisor = args.unnamed[i].as_integer();
            if (divisor == 0) {
                throw_eval_error(form, "/: division by zero");
            }
            result /= divisor;
        }
        return Object::make_integer(result);
        
    } else if (args.unnamed[0].is_float()) {
        if (args.unnamed.size() == 1) {
            FloatType val = args.unnamed[0].as_float();
            if (val == 0.0) {
                throw_eval_error(form, "/: division by zero");
            }
            return Object::make_float(1.0 / val);
        }
        
        FloatType result = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_float()) {
                throw_eval_error(form, "/: all arguments must be floats");
            }
            FloatType divisor = args.unnamed[i].as_float();
            if (divisor == 0.0) {
                throw_eval_error(form, "/: division by zero");
            }
            result /= divisor;
        }
        return Object::make_float(result);
        
    } else {
        throw_eval_error(form, "/: arguments must be numbers");
    }
        return Object::make_empty_list(); 
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

