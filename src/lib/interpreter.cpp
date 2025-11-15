#include "interpreter.h"
#include <sstream>
#include <filesystem>

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
        {"cdr", &Interpreter::eval_cdr},
        {"if", &Interpreter::eval_if},
        {"and", &Interpreter::eval_and},
        {"or", &Interpreter::eval_or},
        {"cond", &Interpreter::eval_cond},
        {"set!", &Interpreter::eval_set},
        {"let", &Interpreter::eval_let},
        {"while", &Interpreter::eval_while},
        {"macro", &Interpreter::eval_macro},
        {"let*", &Interpreter::eval_let_star},
        {"quasiquote", &Interpreter::eval_quasiquote},
        {"`", &Interpreter::eval_quasiquote},
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
        {"cdr", &Interpreter::eval_cdr_builtin},
        {"=", &Interpreter::eval_equals},
        {"<", &Interpreter::eval_lt},
        {">", &Interpreter::eval_gt},
        {"<=", &Interpreter::eval_leq},
        {">=", &Interpreter::eval_geq},
        {"pair?", &Interpreter::eval_pair_p},
        {"null?", &Interpreter::eval_null_p},
        {"symbol?", &Interpreter::eval_symbol_p},
        {"number?", &Interpreter::eval_number_p},
        {"string?", &Interpreter::eval_string_p},
        {"list", &Interpreter::eval_list_func},
        {"length", &Interpreter::eval_length},
        {"append", &Interpreter::eval_append},
        {"eq?", &Interpreter::eval_eq},
        {"gensym", &Interpreter::eval_gensym},
        {"eval", &Interpreter::eval_eval},
        {"set-car!", &Interpreter::eval_set_car},
        {"set-cdr!", &Interpreter::eval_set_cdr},
        {"exit", &Interpreter::eval_exit},
        {"read", &Interpreter::eval_read},
        {"load-file", &Interpreter::eval_load_file},
        {"string-length", &Interpreter::eval_string_length},
        {"string-ref", &Interpreter::eval_string_ref},
        {"string-append", &Interpreter::eval_string_append},
        {"substring", &Interpreter::eval_substring},
        {"string->symbol", &Interpreter::eval_string_to_symbol},
        {"symbol->string", &Interpreter::eval_symbol_to_string},
        {"vector", &Interpreter::eval_vector},
        {"vector-ref", &Interpreter::eval_vector_ref},
        {"vector-set!", &Interpreter::eval_vector_set},
        {"vector-length", &Interpreter::eval_vector_length},
        {"vector?", &Interpreter::eval_vector_p},
        {"make-hash-table", &Interpreter::eval_make_hash_table},
        {"hash-table-set!", &Interpreter::eval_hash_table_set},
        {"hash-table-ref", &Interpreter::eval_hash_table_ref},
        {"hash-table?", &Interpreter::eval_hash_table_p},
        {"read-file", &Interpreter::eval_read_file},
        {"read-line", &Interpreter::eval_load_file},
        {"file-exists?", &Interpreter::eval_file_exists_p},
        {"system", &Interpreter::eval_system},
        // Преобразования типов
        {"number->string", &Interpreter::eval_number_to_string},
        {"string->number", &Interpreter::eval_string_to_number},
        {"char->integer", &Interpreter::eval_char_to_integer},
        {"integer->char", &Interpreter::eval_integer_to_char},
        // Дополнительные предикаты
        {"char?", &Interpreter::eval_char_p},
        {"procedure?", &Interpreter::eval_procedure_p},
        {"vector?", &Interpreter::eval_vector_p},
        {"eqv?", &Interpreter::eval_eqv},
        {"boolean?", &Interpreter::eval_boolean_p},
        // Математические функции
        {"abs", &Interpreter::eval_abs},
        {"max", &Interpreter::eval_max},
        {"min", &Interpreter::eval_min},
        {"expt", &Interpreter::eval_expt},
        {"sqrt", &Interpreter::eval_sqrt},
        // Системные утилиты
        {"current-directory", &Interpreter::eval_current_directory},
    };

    // Инициализируем boolean объекты как символы
    m_true_object = Object::make_symbol(&symbol_table, "#t");
    m_false_object = Object::make_symbol(&symbol_table, "#f");
}

bool Interpreter::try_symbol_lookup(const Object& sym, const std::shared_ptr<EnvironmentObject>& env, Object* dest) {
    // Булевы значения жестко закодированы
    if (sym.is_symbol()) {
        const auto& sym_name = sym.as_symbol();

        // Сравниваем УКАЗАТЕЛИ, а не строки
        if (sym_name.name_ptr == m_true_object.as_symbol().name_ptr) {
            *dest = m_true_object;
            return true;
        }
        if (sym_name.name_ptr == m_false_object.as_symbol().name_ptr) {
            *dest = m_false_object;
            return true;
        }
    }

    // Ищем в environment по имени символа
    if (env && sym.is_symbol()) {
        std::string name = sym.as_symbol().name_ptr ? sym.as_symbol().name_ptr : "";
        return env->try_get(name, dest);
    }

    return false;
}

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
    return Object::make_symbol(&symbol_table, name.c_str());
}

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
    }
    catch (std::runtime_error& e) {
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
        auto head_sym = head.as_symbol();
        std::string head_str = head_sym.name_ptr ? head_sym.name_ptr : "";

        auto kv_sf = special_forms.find(head_str);
        if (kv_sf != special_forms.end()) {
            return ((*this).*(kv_sf->second))(obj, rest, env);
        }

        auto kv_b = builtin_forms.find(head_str);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }

    // Оцениваем голову
    Object evaluated_head = eval_with_rewind(head, env);

    // Проверяем макрос
    if (evaluated_head.is_macro()) {
        auto macro_ptr = evaluated_head.as_macro();
        Arguments args = get_args(obj, rest, make_varargs());

        // Для макросов аргументы НЕ оцениваются!
        auto macro_env = std::make_shared<EnvironmentObject>(macro_ptr->parent_env);

        // Создаем простой ArgumentSpec для макроса
        ArgumentSpec macro_spec;
        for (size_t i = 0; i < macro_ptr->args.size(); ++i) {
            if (i < args.unnamed.size()) {
                macro_spec.unnamed.push_back(macro_ptr->args[i]);
            }
        }

        set_args_in_env(obj, args, macro_spec, macro_env);

        // Выполняем макрос для получения расширения
        Object expansion = eval_with_rewind(macro_ptr->body, macro_env);

        // Оцениваем расширение в исходном окружении
        return eval_with_rewind(expansion, env);
    }

    // Проверяем лямбду
    if (evaluated_head.is_lambda()) {
        auto lambda_ptr = evaluated_head.as_lambda();
        Arguments args = get_args(obj, rest, make_varargs());
        eval_args(&args, env);

        if (args.unnamed.size() != lambda_ptr->args.size()) {
            throw_eval_error(obj, "lambda: wrong number of arguments");
        }

        auto call_env = std::make_shared<EnvironmentObject>(lambda_ptr->parent_env);
        for (size_t i = 0; i < lambda_ptr->args.size(); ++i) {
            call_env->set(lambda_ptr->args[i], args.unnamed[i]);
        }

        return eval_with_rewind(lambda_ptr->body, call_env);
    }

    // Проверяем встроенные функции после оценки
    if (evaluated_head.is_symbol()) {
        auto head_sym = evaluated_head.as_symbol();
        std::string head_str = head_sym.name_ptr ? head_sym.name_ptr : "";
        auto kv_b = builtin_forms.find(head_str);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }
    }

    throw_eval_error(obj, "cannot apply non-function object");
    return Object::make_empty_list();
}

Object Interpreter::eval_quote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!rest.is_pair()) {
        throw_eval_error(form, "quote requires one argument");
    }
    return rest.car();
}

Object Interpreter::eval_define(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
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

    // Сохраняем в ПЕРЕДАННЫЙ environment
    std::string var_name = name_obj.as_symbol().name_ptr ? name_obj.as_symbol().name_ptr : "";
    env->set(var_name, value);

    return value;
}

Object Interpreter::eval_lambda(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (rest.is_empty_list()) {
        throw_eval_error(form, "lambda: expected parameter list and body");
    }

    Object params_obj = rest.car();
    Object body_obj = rest.cdr();

    if (!params_obj.is_list()) {
        throw_eval_error(form, "lambda: parameter list must be a list");
    }

    // Парсим параметры через ArgumentSpec
    ArgumentSpec args = parse_arg_spec(form, params_obj);

    // Тело лямбды
    if (body_obj.is_empty_list()) {
        throw_eval_error(form, "lambda: expected body after parameter list");
    }

    // Создаем лямбда-объект
    Object lambda_obj = LambdaObject::make_new();
    auto lambda = lambda_obj.as_lambda();
    lambda->args = args;
    lambda->body = body_obj.car();
    lambda->parent_env = env;

    return lambda_obj;
}

Object Interpreter::eval_begin(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    Object current = rest;
    Object result = Object::make_empty_list();

    while (current.is_pair()) {
        result = eval_with_rewind(current.car(), env);
        current = current.cdr();
    }

    return result;
}

Object Interpreter::eval_print(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
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

Object Interpreter::eval_cons(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
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

Object Interpreter::eval_car(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "car requires one argument");
    }

    Object arg = eval_with_rewind(rest.car(), env);
    if (!arg.is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }

    return arg.car();
}

Object Interpreter::eval_cdr(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "cdr requires one argument");
    }

    Object arg = eval_with_rewind(rest.car(), env);
    if (!arg.is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }

    return arg.cdr();
}

Object Interpreter::eval_plus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;

    if (args.unnamed.empty()) {
        return Object::make_integer(0);
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 0;
        for (const auto& arg : args.unnamed) {
            result += number_to_integer(arg);
        }
        return Object::make_integer(result);
    }
    else if (args.unnamed[0].is_float()) {
        FloatType result = 0.0;
        for (const auto& arg : args.unnamed) {
            result += number_to_float(arg);
        }
        return Object::make_float(result);
    }
    else {
        throw_eval_error(form, "+ requires number arguments");
    }
}

Object Interpreter::eval_minus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        throw_eval_error(form, "- requires at least one argument");
    }

    if (args.unnamed[0].is_integer()) {
        if (args.unnamed.size() == 1) {
            return Object::make_integer(-number_to_integer(args.unnamed[0]));
        }
        IntType result = number_to_integer(args.unnamed[0]);
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            result -= number_to_integer(args.unnamed[i]);
        }
        return Object::make_integer(result);
    }
    else if (args.unnamed[0].is_float()) {
        if (args.unnamed.size() == 1) {
            return Object::make_float(-number_to_float(args.unnamed[0]));
        }
        FloatType result = number_to_float(args.unnamed[0]);
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            result -= number_to_float(args.unnamed[i]);
        }
        return Object::make_float(result);
    }
    else {
        throw_eval_error(form, "- requires number arguments");
    }
}

Object Interpreter::eval_times(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;

    if (args.unnamed.empty()) {
        return Object::make_integer(1);
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 1;
        for (const auto& arg : args.unnamed) {
            result *= number_to_integer(arg);
        }
        return Object::make_integer(result);
    }
    else if (args.unnamed[0].is_float()) {
        FloatType result = 1.0;
        for (const auto& arg : args.unnamed) {
            result *= number_to_float(arg);
        }
        return Object::make_float(result);
    }
    else {
        throw_eval_error(form, "* requires number arguments");
    }
}

Object Interpreter::eval_divide(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "/ requires exactly two arguments");
    }

    FloatType numerator = number_to_float(args.unnamed[0]);
    FloatType denominator = number_to_float(args.unnamed[1]);

    if (denominator == 0.0) {
        throw_eval_error(form, "/: division by zero");
    }

    return Object::make_float(numerator / denominator);
}

Object Interpreter::eval_print_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
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

Object Interpreter::eval_cons_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "cons requires two arguments");
    }
    return Object::make_pair(args.unnamed[0], args.unnamed[1]);
}

Object Interpreter::eval_car_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    return args.unnamed[0].car();
}

Object Interpreter::eval_cdr_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    return args.unnamed[0].cdr();
}

void Interpreter::throw_eval_error(const Object& o, const std::string& err) {
    throw std::runtime_error("[Z80-Lisp] Evaluation error on " + o.print() + ": " + err);
}

void Interpreter::execute_repl() {
    want_exit = false;
    std::string input;

    auto repl_env = std::make_shared<EnvironmentObject>();

    std::cout << "Lisp REPL (type 'quit' to exit)\n";

    while (!want_exit) {
        std::cout << "lisp> ";
        std::cout.flush();

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty()) continue;
        if (input == "quit" || input == "exit") break;

        try {
            Object code = reader.read_from_string(input, "REPL input");
            Object result = eval_with_rewind(code, repl_env);

            if (!result.is_empty_list() || input != "()") {
                std::cout << "=> " << result.print() << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    std::cout << "Goodbye!\n";
}

Object Interpreter::eval_if(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "if requires condition and branches");
    }

    Object condition_obj = rest.car();
    Object then_part_obj = rest.cdr();

    if (!then_part_obj.is_pair()) {
        throw_eval_error(form, "if requires then branch");
    }

    Object condition_result = eval_with_rewind(condition_obj, env);

    if (truthy(condition_result)) {
        return eval_with_rewind(then_part_obj.car(), env);
    }
    else {
        Object else_part = then_part_obj.cdr();
        if (else_part.is_pair()) {
            return eval_with_rewind(else_part.car(), env);
        }
        else {
            return Object::make_empty_list();
        }
    }
}

Object Interpreter::eval_cond(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    Object current_clause = rest;

    while (current_clause.is_pair()) {
        Object clause = current_clause.car();

        if (!clause.is_pair()) {
            throw_eval_error(form, "cond clause must be a pair");
        }

        Object condition = clause.car();
        Object body = clause.cdr();

        // Особый случай: (else ...)
        if (condition.is_symbol() && condition.as_symbol().name_ptr &&
            strcmp(condition.as_symbol().name_ptr, "else") == 0) {
            if (!body.is_pair()) {
                throw_eval_error(form, "cond else clause must have body");
            }
            return eval_with_rewind(body.car(), env);
        }

        Object condition_result = eval_with_rewind(condition, env);

        if (truthy(condition_result)) {
            if (body.is_pair()) {
                return eval_with_rewind(body.car(), env);
            }
            else {
                return condition_result;
            }
        }

        current_clause = current_clause.cdr();
    }

    return Object::make_empty_list();
}

Object Interpreter::eval_and(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    Object current = rest;
    Object result = m_true_object;

    while (current.is_pair()) {
        result = eval_with_rewind(current.car(), env);
        if (!truthy(result)) {
            return result;
        }
        current = current.cdr();
    }

    return result;
}

Object Interpreter::eval_or(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    Object current = rest;

    while (current.is_pair()) {
        Object result = eval_with_rewind(current.car(), env);
        if (truthy(result)) {
            return result;
        }
        current = current.cdr();
    }

    return m_false_object;
}

Object Interpreter::eval_equals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "= requires exactly two arguments");
    }

    const Object& a = args.unnamed[0];
    const Object& b = args.unnamed[1];

    if (!is_number(a) || !is_number(b)) {
        throw_eval_error(form, "= requires number arguments");
    }

    FloatType a_val = number_to_float(a);
    FloatType b_val = number_to_float(b);

    return make_bool(a_val == b_val);
}

Object Interpreter::eval_lt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "< requires exactly two arguments");
    }

    const Object& a = args.unnamed[0];
    const Object& b = args.unnamed[1];

    if (!is_number(a) || !is_number(b)) {
        throw_eval_error(form, "< requires number arguments");
    }

    FloatType a_val = number_to_float(a);
    FloatType b_val = number_to_float(b);

    return make_bool(a_val < b_val);
}

Object Interpreter::eval_gt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "> requires exactly two arguments");
    }

    const Object& a = args.unnamed[0];
    const Object& b = args.unnamed[1];

    if (!is_number(a) || !is_number(b)) {
        throw_eval_error(form, "> requires number arguments");
    }

    FloatType a_val = number_to_float(a);
    FloatType b_val = number_to_float(b);

    return make_bool(a_val > b_val);
}

Object Interpreter::eval_leq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "<= requires exactly two arguments");
    }

    const Object& a = args.unnamed[0];
    const Object& b = args.unnamed[1];

    if (!is_number(a) || !is_number(b)) {
        throw_eval_error(form, "<= requires number arguments");
    }

    FloatType a_val = number_to_float(a);
    FloatType b_val = number_to_float(b);

    return make_bool(a_val <= b_val);
}

Object Interpreter::eval_geq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, ">= requires exactly two arguments");
    }

    const Object& a = args.unnamed[0];
    const Object& b = args.unnamed[1];

    if (!is_number(a) || !is_number(b)) {
        throw_eval_error(form, ">= requires number arguments");
    }

    FloatType a_val = number_to_float(a);
    FloatType b_val = number_to_float(b);

    return make_bool(a_val >= b_val);
}

Object Interpreter::eval_set(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "set! requires variable and value");
    }

    Object name_obj = rest.car();
    Object value_part = rest.cdr();

    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "set! variable must be a symbol");
    }

    if (!value_part.is_pair()) {
        throw_eval_error(form, "set! requires a value");
    }

    std::string var_name = name_obj.as_symbol().name_ptr ? name_obj.as_symbol().name_ptr : "";
    Object value = eval_with_rewind(value_part.car(), env);

    if (env) {
        env->set(var_name, value);
        return value;
    }

    return value;
}

Object Interpreter::eval_let(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "let requires bindings and body");
    }

    Object bindings_obj = rest.car();
    Object body_obj = rest.cdr();

    if (!bindings_obj.is_list()) {
        throw_eval_error(form, "let bindings must be a list");
    }

    auto let_env = std::make_shared<EnvironmentObject>(env);

    Object current_binding = bindings_obj;
    while (current_binding.is_pair()) {
        Object binding = current_binding.car();

        if (!binding.is_pair()) {
            throw_eval_error(form, "let binding must be a pair (name value)");
        }

        Object name_obj = binding.car();
        Object value_part = binding.cdr();

        if (!name_obj.is_symbol()) {
            throw_eval_error(form, "let binding name must be a symbol");
        }

        if (!value_part.is_pair()) {
            throw_eval_error(form, "let binding must have a value");
        }

        Object value = eval_with_rewind(value_part.car(), env);
        let_env->set(name_obj.as_symbol().name_ptr ? name_obj.as_symbol().name_ptr : "", value);

        current_binding = current_binding.cdr();
    }

    Object result = Object::make_empty_list();
    Object current_body = body_obj;

    while (current_body.is_pair()) {
        result = eval_with_rewind(current_body.car(), let_env);
        current_body = current_body.cdr();
    }
    return result;
}

Object Interpreter::eval_pair_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "pair? requires one argument");
    }
    return make_bool(args.unnamed[0].is_pair());
}

Object Interpreter::eval_null_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "null? requires one argument");
    }
    return make_bool(args.unnamed[0].is_empty_list());
}

Object Interpreter::eval_symbol_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "symbol? requires one argument");
    }
    return make_bool(args.unnamed[0].is_symbol());
}

Object Interpreter::eval_number_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "number? requires one argument");
    }
    return make_bool(args.unnamed[0].is_integer() || args.unnamed[0].is_float());
}

Object Interpreter::eval_string_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "string? requires one argument");
    }
    return make_bool(args.unnamed[0].is_string());
}

Object Interpreter::eval_while(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "while requires condition and body");
    }

    Object condition_obj = rest.car();
    Object body_obj = rest.cdr();

    if (!body_obj.is_pair()) {
        throw_eval_error(form, "while requires a body");
    }

    Object result = Object::make_empty_list();
    int iteration = 0;

    while (true) {
        Object condition_result = eval_with_rewind(condition_obj, env);

        if (!truthy(condition_result)) {
            break;
        }

        Object current_body = body_obj;
        while (current_body.is_pair()) {
            result = eval_with_rewind(current_body.car(), env);
            current_body = current_body.cdr();
        }
    }

    return result;
}

Object Interpreter::eval_list_func(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    Object result = Object::make_empty_list();

    for (auto it = args.unnamed.rbegin(); it != args.unnamed.rend(); ++it) {
        result = Object::make_pair(*it, result);
    }

    return result;
}

Object Interpreter::eval_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "length requires one argument");
    }

    Object lst = args.unnamed[0];
    int count = 0;

    while (lst.is_pair()) {
        count++;
        lst = lst.cdr();
    }

    if (!lst.is_empty_list()) {
        throw_eval_error(form, "length requires a proper list");
    }

    return Object::make_integer(count);
}

Object Interpreter::eval_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        return Object::make_empty_list();
    }

    Object result = args.unnamed.back();

    for (int i = args.unnamed.size() - 2; i >= 0; --i) {
        Object current = args.unnamed[i];

        Object reversed = Object::make_empty_list();
        while (current.is_pair()) {
            reversed = Object::make_pair(current.car(), reversed);
            current = current.cdr();
        }

        while (reversed.is_pair()) {
            result = Object::make_pair(reversed.car(), result);
            reversed = reversed.cdr();
        }
    }

    return result;
}

Object Interpreter::eval_macro(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (rest.is_empty_list()) {
        throw_eval_error(form, "macro: expected name, parameter list and body");
    }

    Object name_obj = rest.car();
    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "macro: name must be a symbol");
    }
    std::string macro_name = name_obj.as_symbol().name_ptr ? name_obj.as_symbol().name_ptr : "";

    Object rrest = rest.cdr();
    if (rrest.is_empty_list()) {
        throw_eval_error(form, "macro: expected parameter list after name");
    }

    Object params_obj = rrest.car();
    Object body_obj = rrest.cdr();

    if (!params_obj.is_list()) {
        throw_eval_error(form, "macro: parameter list must be a list");
    }

    ArgumentSpec args = parse_arg_spec(form, params_obj);

    if (body_obj.is_empty_list()) {
        throw_eval_error(form, "macro: expected body after parameter list");
    }

    Object macro_obj = MacroObject::make_new();
    auto macro = macro_obj.as_macro();
    macro->name = macro_name;
    macro->args = args;
    macro->body = body_obj.car();
    macro->parent_env = env;

    env->set(macro_name, macro_obj);

    return macro_obj;
}

Object Interpreter::eval_let_star(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "let* requires bindings and body");
    }

    Object bindings_obj = rest.car();
    Object body_obj = rest.cdr();

    if (!bindings_obj.is_list()) {
        throw_eval_error(form, "let* bindings must be a list");
    }

    auto current_env = env;

    Object current_binding = bindings_obj;
    while (current_binding.is_pair()) {
        Object binding = current_binding.car();

        if (!binding.is_pair()) {
            throw_eval_error(form, "let* binding must be a pair (name value)");
        }

        Object name_obj = binding.car();
        Object value_part = binding.cdr();

        if (!name_obj.is_symbol()) {
            throw_eval_error(form, "let* binding name must be a symbol");
        }

        if (!value_part.is_pair()) {
            throw_eval_error(form, "let* binding must have a value");
        }

        auto new_env = std::make_shared<EnvironmentObject>(current_env);

        Object value = eval_with_rewind(value_part.car(), current_env);
        new_env->set(name_obj.as_symbol().name_ptr ? name_obj.as_symbol().name_ptr : "", value);

        current_env = new_env;
        current_binding = current_binding.cdr();
    }

    Object result = Object::make_empty_list();
    Object current_body = body_obj;
    while (current_body.is_pair()) {
        result = eval_with_rewind(current_body.car(), current_env);
        current_body = current_body.cdr();
    }
    return result;
}

Object Interpreter::eval_quasiquote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (rest.type != ObjectType::PAIR || rest.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
        throw_eval_error(form, "quasiquote must have one argument!");
    }
    return quasiquote_helper(rest.as_pair()->car, env);
}

Object Interpreter::quasiquote_helper(const Object& form, const std::shared_ptr<EnvironmentObject>& env) {
    if (!form.is_pair()) {
        return form;
    }

    const Object& car = form.as_pair()->car;
    const Object& cdr = form.as_pair()->cdr;

    if (car.is_symbol() && car.as_symbol().name_ptr && strcmp(car.as_symbol().name_ptr, "unquote") == 0) {
        if (!cdr.is_pair() || !cdr.as_pair()->cdr.is_empty_list()) {
            throw_eval_error(form, "unquote must have exactly one argument");
        }
        return eval_with_rewind(cdr.as_pair()->car, env);
    }

    if (car.is_pair() &&
        car.as_pair()->car.is_symbol() &&
        car.as_pair()->car.as_symbol().name_ptr &&
        strcmp(car.as_pair()->car.as_symbol().name_ptr, "unquote-splicing") == 0) {

        const Object& splicing_args = car.as_pair()->cdr;
        if (!splicing_args.is_pair() || !splicing_args.as_pair()->cdr.is_empty_list()) {
            throw_eval_error(form, "unquote-splicing must have exactly one argument");
        }

        Object spliced = eval_with_rewind(splicing_args.as_pair()->car, env);
        Object processed_cdr = quasiquote_helper(cdr, env);

        if (!spliced.is_list()) {
            throw_eval_error(form, "unquote-splicing requires a list");
        }

        if (spliced.is_empty_list()) {
            return processed_cdr;
        }

        std::vector<Object> elements;
        Object current = spliced;
        while (current.is_pair()) {
            elements.push_back(current.as_pair()->car);
            current = current.as_pair()->cdr;
        }

        Object result = processed_cdr;
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            result = Object::make_pair(*it, result);
        }

        return result;
    }

    Object processed_car = quasiquote_helper(car, env);
    Object processed_cdr = quasiquote_helper(cdr, env);

    if (processed_car.is_pair() &&
        processed_car.as_pair()->car.is_symbol() &&
        processed_car.as_pair()->car.as_symbol().name_ptr &&
        strcmp(processed_car.as_pair()->car.as_symbol().name_ptr, "unquote-splicing") == 0) {

        const Object& splicing_args = processed_car.as_pair()->cdr;
        if (!splicing_args.is_pair() || !splicing_args.as_pair()->cdr.is_empty_list()) {
            throw_eval_error(form, "unquote-splicing must have exactly one argument");
        }

        Object spliced = eval_with_rewind(splicing_args.as_pair()->car, env);

        if (!spliced.is_list()) {
            throw_eval_error(form, "unquote-splicing requires a list");
        }

        if (spliced.is_empty_list()) {
            return processed_cdr;
        }

        std::vector<Object> elements;
        Object current = spliced;
        while (current.is_pair()) {
            elements.push_back(current.as_pair()->car);
            current = current.as_pair()->cdr;
        }

        Object result = processed_cdr;
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            result = Object::make_pair(*it, result);
        }

        return result;
    }

    return Object::make_pair(processed_car, processed_cdr);
}

Object Interpreter::eval_gensym(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form;
    (void)args;
    (void)env;
    std::string name = "gensym" + std::to_string(gensym_id++);
    return Object::make_symbol(&symbol_table, name.c_str());
}

Object Interpreter::eval_eq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "eq? requires two arguments");
    }
    return make_bool(args.unnamed[0] == args.unnamed[1]);
}

Object Interpreter::eval_eval(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "eval requires one argument");
    }
    return eval_with_rewind(args.unnamed[0], env);
}

Object Interpreter::eval_set_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "set-car! requires two arguments");
    }
    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-car! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->car = args.unnamed[1];
    return args.unnamed[0];
}

Object Interpreter::eval_set_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "set-cdr! requires two arguments");
    }
    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-cdr! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->cdr = args.unnamed[1];
    return args.unnamed[0];
}

Object Interpreter::eval_exit(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form; (void)args; (void)env;
    want_exit = true;
    return Object::make_empty_list();
}

Object Interpreter::eval_read(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "read requires one string argument");
    }
    try {
        return reader.read_from_string(args.unnamed[0].as_string(), "read input");
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("read error: ") + e.what());
    }
}

Object Interpreter::eval_load_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "load-file requires one string argument");
    }
    try {
        Object code = reader.read_from_file(args.unnamed[0].as_string());
        return eval_with_rewind(code, env);
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("load-file error: ") + e.what());
    }
}

ArgumentSpec Interpreter::parse_arg_spec(const Object& form, Object& rest) {
    ArgumentSpec spec;

    Object current = rest;
    while (!current.is_empty_list()) {
        Object arg = current.as_pair()->car;
        if (!arg.is_symbol()) {
            throw_eval_error(form, "args must be symbols");
        }

        std::string arg_name = arg.as_symbol().name_ptr ? arg.as_symbol().name_ptr : "";

        if (arg_name == "&rest") {
            current = current.as_pair()->cdr;
            if (!current.is_pair()) {
                throw_eval_error(form, "rest arg must have a name");
            }
            Object rest_name = current.as_pair()->car;
            if (!rest_name.is_symbol()) {
                throw_eval_error(form, "rest name must be a symbol");
            }
            spec.rest = rest_name.as_symbol().name_ptr ? rest_name.as_symbol().name_ptr : "";
            break;
        }
        else {
            spec.unnamed.push_back(arg_name);
        }

        current = current.as_pair()->cdr;
    }
    return spec;
}

void Interpreter::set_args_in_env(const Object& form, const Arguments& args,
    const ArgumentSpec& arg_spec,
    const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.size() < arg_spec.unnamed.size()) {
        throw_eval_error(form, "not enough arguments");
    }

    for (size_t i = 0; i < arg_spec.unnamed.size(); ++i) {
        env->set(arg_spec.unnamed[i], args.unnamed[i]);
    }

    if (!arg_spec.rest.empty()) {
        Object rest_list = Object::make_empty_list();
        for (size_t i = arg_spec.unnamed.size(); i < args.unnamed.size(); ++i) {
            rest_list = Object::make_pair(args.unnamed[i], rest_list);
        }
        env->set(arg_spec.rest, rest_list);
    }
}

Object Interpreter::eval_string_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "string-length requires one string argument");
    }
    return Object::make_integer(args.unnamed[0].as_string().length());
}

Object Interpreter::eval_string_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2 || !args.unnamed[0].is_string() || !args.unnamed[1].is_integer()) {
        throw_eval_error(form, "string-ref requires string and integer arguments");
    }

    const std::string& str = args.unnamed[0].as_string();
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(str.length())) {
        throw_eval_error(form, "string-ref: index out of range");
    }

    return Object::make_char(str[index]);
}

Object Interpreter::eval_string_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    std::string result;

    for (const auto& arg : args.unnamed) {
        if (!arg.is_string()) {
            throw_eval_error(form, "string-append requires string arguments");
        }
        result += arg.as_string();
    }

    return Object::make_string(result);
}

Object Interpreter::eval_substring(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 3 || !args.unnamed[0].is_string() ||
        !args.unnamed[1].is_integer() || !args.unnamed[2].is_integer()) {
        throw_eval_error(form, "substring requires string, start, and end arguments");
    }

    const std::string& str = args.unnamed[0].as_string();
    int64_t start = args.unnamed[1].as_integer();
    int64_t end = args.unnamed[2].as_integer();

    if (start < 0 || end > static_cast<int64_t>(str.length()) || start > end) {
        throw_eval_error(form, "substring: invalid start or end index");
    }

    return Object::make_string(str.substr(start, end - start));
}

Object Interpreter::eval_string_to_symbol(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "string->symbol requires one string argument");
    }
    return Object::make_symbol(&symbol_table, args.unnamed[0].as_string().c_str());
}

Object Interpreter::eval_symbol_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_symbol()) {
        throw_eval_error(form, "symbol->string requires one symbol argument");
    }
    return Object::make_string(args.unnamed[0].as_symbol().name_ptr ? args.unnamed[0].as_symbol().name_ptr : "");
}

Object Interpreter::eval_vector(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    return Object::make_array(args.unnamed);
}

Object Interpreter::eval_vector_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2 || !args.unnamed[0].is_array() || !args.unnamed[1].is_integer()) {
        throw_eval_error(form, "vector-ref requires vector and integer arguments");
    }

    auto elements = args.unnamed[0].as_vector();
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(elements.size())) {
        throw_eval_error(form, "vector-ref: index out of range");
    }

    return elements[index];
}

Object Interpreter::eval_vector_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 3 || !args.unnamed[0].is_array() ||
        !args.unnamed[1].is_integer()) {
        throw_eval_error(form, "vector-set! requires vector, index, and value arguments");
    }

    auto vec_ptr = dynamic_cast<ArrayObject*>(args.unnamed[0].heap_obj.get());
    if (!vec_ptr) {
        throw_eval_error(form, "vector-set!: not a vector");
    }

    int64_t index = args.unnamed[1].as_integer();
    if (index < 0 || index >= static_cast<int64_t>(vec_ptr->elements.size())) {
        throw_eval_error(form, "vector-set!: index out of range");
    }

    vec_ptr->elements[index] = args.unnamed[2];
    return args.unnamed[2];
}

Object Interpreter::eval_vector_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_array()) {
        throw_eval_error(form, "vector-length requires one vector argument");
    }
    return Object::make_integer(args.unnamed[0].as_vector().size());
}

Object Interpreter::eval_vector_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "vector? requires one argument");
    }
    return make_bool(args.unnamed[0].is_array());
}

Object Interpreter::eval_make_hash_table(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    return Object::make_hash_table();
}

Object Interpreter::eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 3 || !args.unnamed[0].is_hash_table()) {
        throw_eval_error(form, "hash-table-set! requires hash-table, key, and value arguments");
    }

    auto ht = args.unnamed[0].as_hash_table();
    std::string key;

    if (args.unnamed[1].is_string()) {
        key = args.unnamed[1].as_string();
    }
    else if (args.unnamed[1].is_symbol()) {
        key = args.unnamed[1].as_symbol().name_ptr ? args.unnamed[1].as_symbol().name_ptr : "";
    }
    else {
        throw_eval_error(form, "hash-table key must be string or symbol");
    }

    ht->data[key] = args.unnamed[2];
    return args.unnamed[2];
}

Object Interpreter::eval_hash_table_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2 || !args.unnamed[0].is_hash_table()) {
        throw_eval_error(form, "hash-table-ref requires hash-table and key arguments");
    }

    auto ht = args.unnamed[0].as_hash_table();
    std::string key;

    if (args.unnamed[1].is_string()) {
        key = args.unnamed[1].as_string();
    }
    else if (args.unnamed[1].is_symbol()) {
        key = args.unnamed[1].as_symbol().name_ptr ? args.unnamed[1].as_symbol().name_ptr : "";
    }
    else {
        throw_eval_error(form, "hash-table key must be string or symbol");
    }

    auto it = ht->data.find(key);
    if (it == ht->data.end()) {
        throw_eval_error(form, "hash-table-ref: key not found: " + key);
    }

    return it->second;
}

Object Interpreter::eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "hash-table? requires one argument");
    }
    return make_bool(args.unnamed[0].is_hash_table());
}

Object Interpreter::eval_read_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    std::string filename = args.unnamed[0].as_string();
    std::string content = read_entire_file(filename);
    return reader.read_from_string(content, filename);
}

std::string Interpreter::read_entire_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("Cannot open file");
    return std::string((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
}

Object Interpreter::eval_file_exists_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "file-exists? requires one string argument");
    }

    std::string filename = args.unnamed[0].as_string();
    std::ifstream file(filename);
    bool exists = file.good();
    file.close();

    return make_bool(exists);
}

Object Interpreter::eval_get_environment_variable(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "get-environment-variable requires one string argument");
    }

    std::string var_name = args.unnamed[0].as_string();
    const char* value = std::getenv(var_name.c_str());

    if (value) {
        return Object::make_string(value);
    }
    else {
        return m_false_object;
    }
}

Object Interpreter::eval_system(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "system requires one string argument");
    }

    std::string command = args.unnamed[0].as_string();
    int result = std::system(command.c_str());

    return Object::make_integer(result);
}

Object Interpreter::eval_number_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() < 1 || args.unnamed.size() > 2) {
        throw_eval_error(form, "number->string requires 1 or 2 arguments");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t num = args.unnamed[0].as_integer();
        if (args.unnamed.size() == 2 && args.unnamed[1].is_integer()) {
            int64_t base = args.unnamed[1].as_integer();
            if (base == 16) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%llx", (long long)num);
                return Object::make_string(buffer);
            }
        }
        return Object::make_string(std::to_string(num));
    }
    else if (args.unnamed[0].is_float()) {
        double num = args.unnamed[0].as_float();
        return Object::make_string(std::to_string(num));
    }
    else {
        throw_eval_error(form, "number->string requires a number argument");
    }
}

Object Interpreter::eval_string_to_number(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() < 1 || args.unnamed.size() > 2 || !args.unnamed[0].is_string()) {
        throw_eval_error(form, "string->number requires string and optional base arguments");
    }

    std::string str = args.unnamed[0].as_string();
    int base = 10;

    if (args.unnamed.size() == 2 && args.unnamed[1].is_integer()) {
        base = args.unnamed[1].as_integer();
    }

    try {
        if (str.find('.') != std::string::npos || str.find('e') != std::string::npos) {
            double value = std::stod(str);
            return Object::make_float(value);
        }
        else {
            if (base == 16 && str.substr(0, 2) == "0x") {
                str = str.substr(2);
            }
            int64_t value = std::stoll(str, nullptr, base);
            return Object::make_integer(value);
        }
    }
    catch (const std::exception&) {
        return m_false_object;
    }
}

Object Interpreter::eval_char_to_integer(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_char()) {
        throw_eval_error(form, "char->integer requires one char argument");
    }
    return Object::make_integer(static_cast<int64_t>(args.unnamed[0].as_char()));
}

Object Interpreter::eval_integer_to_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1 || !args.unnamed[0].is_integer()) {
        throw_eval_error(form, "integer->char requires one integer argument");
    }

    int64_t code = args.unnamed[0].as_integer();
    if (code < 0 || code > 255) {
        throw_eval_error(form, "integer->char: code out of range 0-255");
    }

    return Object::make_char(static_cast<char>(code));
}

Object Interpreter::eval_char_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "char? requires one argument");
    }
    return make_bool(args.unnamed[0].is_char());
}

Object Interpreter::eval_procedure_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "procedure? requires one argument");
    }
    bool is_proc = args.unnamed[0].is_lambda() ||
        args.unnamed[0].is_macro() ||
        (args.unnamed[0].is_symbol() && builtin_forms.find(args.unnamed[0].as_symbol().name_ptr ? args.unnamed[0].as_symbol().name_ptr : "") != builtin_forms.end());
    return make_bool(is_proc);
}

Object Interpreter::eval_eqv(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "eqv? requires two arguments");
    }
    return make_bool(args.unnamed[0] == args.unnamed[1]);
}

Object Interpreter::eval_boolean_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "boolean? requires one argument");
    }
    const Object& obj = args.unnamed[0];
    bool is_bool = (obj.is_symbol() && obj.as_symbol().name_ptr &&
        (strcmp(obj.as_symbol().name_ptr, "#t") == 0 ||
            strcmp(obj.as_symbol().name_ptr, "#f") == 0));
    return make_bool(is_bool);
}

Object Interpreter::eval_abs(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "abs requires one argument");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t val = args.unnamed[0].as_integer();
        return Object::make_integer(val < 0 ? -val : val);
    }
    else if (args.unnamed[0].is_float()) {
        double val = args.unnamed[0].as_float();
        return Object::make_float(val < 0 ? -val : val);
    }
    else {
        throw_eval_error(form, "abs requires a number argument");
    }
}

Object Interpreter::eval_max(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        throw_eval_error(form, "max requires at least one argument");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t max_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_integer()) {
                throw_eval_error(form, "max: all arguments must be integers");
            }
            int64_t val = args.unnamed[i].as_integer();
            if (val > max_val) max_val = val;
        }
        return Object::make_integer(max_val);
    }
    else if (args.unnamed[0].is_float()) {
        double max_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_float()) {
                throw_eval_error(form, "max: all arguments must be floats");
            }
            double val = args.unnamed[i].as_float();
            if (val > max_val) max_val = val;
        }
        return Object::make_float(max_val);
    }
    else {
        throw_eval_error(form, "max requires number arguments");
    }
}

Object Interpreter::eval_min(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.empty()) {
        throw_eval_error(form, "min requires at least one argument");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t min_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_integer()) {
                throw_eval_error(form, "min: all arguments must be integers");
            }
            int64_t val = args.unnamed[i].as_integer();
            if (val < min_val) min_val = val;
        }
        return Object::make_integer(min_val);
    }
    else if (args.unnamed[0].is_float()) {
        double min_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            if (!args.unnamed[i].is_float()) {
                throw_eval_error(form, "min: all arguments must be floats");
            }
            double val = args.unnamed[i].as_float();
            if (val < min_val) min_val = val;
        }
        return Object::make_float(min_val);
    }
    else {
        throw_eval_error(form, "min requires number arguments");
    }
}

Object Interpreter::eval_expt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "expt requires two arguments");
    }

    if (args.unnamed[0].is_integer() && args.unnamed[1].is_integer()) {
        int64_t base = args.unnamed[0].as_integer();
        int64_t exponent = args.unnamed[1].as_integer();

        if (exponent < 0) {
            return Object::make_float(std::pow(static_cast<double>(base), exponent));
        }

        int64_t result = 1;
        for (int64_t i = 0; i < exponent; ++i) {
            result *= base;
        }
        return Object::make_integer(result);
    }
    else {
        double base = number_to_float(args.unnamed[0]);
        double exponent = number_to_float(args.unnamed[1]);
        return Object::make_float(std::pow(base, exponent));
    }
}

Object Interpreter::eval_sqrt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() != 1) {
        throw_eval_error(form, "sqrt requires one argument");
    }

    double val = number_to_float(args.unnamed[0]);
    if (val < 0) {
        throw_eval_error(form, "sqrt: negative argument");
    }

    return Object::make_float(std::sqrt(val));
}

Object Interpreter::eval_current_directory(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    if (!args.unnamed.empty()) {
        throw_eval_error(form, "current-directory takes no arguments");
    }

    try {
        std::string cwd = std::filesystem::current_path().string();
        return Object::make_string(cwd);
    }
    catch (const std::exception& e) {
        throw_eval_error(form, "cannot get current directory");
    }
}

int64_t Interpreter::number_to_integer(const Object& obj) {
    if (obj.is_integer()) {
        return obj.as_integer();
    }
    else if (obj.is_float()) {
        return static_cast<int64_t>(obj.as_float());
    }
    else {
        throw_eval_error(obj, "object cannot be converted to integer");
    }
}

double Interpreter::number_to_float(const Object& obj) {
    if (obj.is_float()) {
        return obj.as_float();
    }
    else if (obj.is_integer()) {
        return static_cast<double>(obj.as_integer());
    }
    else {
        throw_eval_error(obj, "object cannot be converted to float");
    }
}

bool Interpreter::is_number(const Object& obj) {
    return obj.is_integer() || obj.is_float();
}