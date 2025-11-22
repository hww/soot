#include "interpreter.h"
#include "pretty_printer.h"

#include "fmt/args.h"
#include "fmt/base.h"
#include "fmt/format.h"
#include "util/log.h"
#include "util/crc32.h"
#include "util/file_util.h"
#include "util/string_util.h"
#include "util/unicode_util.h"
#include "common_types.h"

#include <sstream>
#include <filesystem>

namespace script 
{
    Interpreter::Interpreter(const std::string& username, bool load_libs) {
        // Инициализируем boolean объекты как символы
        auto& symbols = reader.get_symbol_table();
        m_true_object = Object::make_symbol(&symbols, "#t");
        m_false_object = Object::make_symbol(&symbols, "#f");

        // Создаем глобальное окружение
        global_environment = EnvironmentObject::make_new("global");

        // create the environment which is be visible from GOAL
        comp_env = EnvironmentObject::make_new("goal");

        define_var_in_env(global_environment, global_environment, "*global-env*");
        define_var_in_env(global_environment, comp_env, "*comp-env*");
        define_var_in_env(comp_env, comp_env, "*comp-env*");
        define_var_in_env(comp_env, global_environment, "*global-env*");

        auto user = Object::make_symbol(&symbols, username.c_str());
        define_var_in_env(global_environment, user, "*user*");

        // Инициализация string_to_type для type?
        string_to_type = {
            {"empty-list", ObjectType::EMPTY_LIST},
            {"integer", ObjectType::INTEGER},
            {"float", ObjectType::FLOAT},
            {"char", ObjectType::CHAR},
            {"symbol", ObjectType::SYMBOL},
            {"string", ObjectType::STRING},
            {"pair", ObjectType::PAIR},
            {"array", ObjectType::ARRAY},
            {"lambda", ObjectType::LAMBDA},
            {"macro", ObjectType::MACRO},
            {"environment", ObjectType::ENVIRONMENT}
        };

        // === СПЕЦИАЛЬНЫЕ ФОРМЫ (не вычисляют аргументы) ===
        init_special_forms({
            {"define", &Interpreter::eval_define},
            {"quote", &Interpreter::eval_quote},
            {"set!", &Interpreter::eval_set},
            {"let", &Interpreter::eval_let},
            {"let*", &Interpreter::eval_let_star},
            {"lambda", &Interpreter::eval_lambda},
            {"cond", &Interpreter::eval_cond},
            {"begin", &Interpreter::eval_begin},
            {"or", &Interpreter::eval_or},
            {"and", &Interpreter::eval_and},
            {"if", &Interpreter::eval_if},
            {"macro", &Interpreter::eval_macro},
            {"quasiquote", &Interpreter::eval_quasiquote},
            {"while", &Interpreter::eval_while},
            {"top-level", &Interpreter::eval_begin} // top level evaluation
            });

    // === ВСТРОЕННЫЕ ФУНКЦИИ (вычисляют аргументы) ===
    init_builtin_forms({ {
            // Математические
            {"+", &Interpreter::eval_plus},
            {"-", &Interpreter::eval_minus},
            {"*", &Interpreter::eval_times},
            {"/", &Interpreter::eval_divide},
            {"=", &Interpreter::eval_numequals},  // было eval_equals
            {"<", &Interpreter::eval_lt},
            {">", &Interpreter::eval_gt},
            {"<=", &Interpreter::eval_leq},
            {">=", &Interpreter::eval_geq},

            // Списки и пары
            {"cons", &Interpreter::eval_cons},      // было eval_cons_builtin
            {"car", &Interpreter::eval_car},        // было eval_car_builtin
            {"cdr", &Interpreter::eval_cdr},        // было eval_cdr_builtin
            {"set-car!", &Interpreter::eval_set_car},
            {"set-cdr!", &Interpreter::eval_set_cdr},

            {"list", &Interpreter::eval_list_func},
            {"length", &Interpreter::eval_length},
            {"append", &Interpreter::eval_append},
            {"null?", &Interpreter::eval_null_p},     // было eval_null_p
            {"pair?", &Interpreter::eval_pair_p},

            // Предикаты типов
            {"symbol?", &Interpreter::eval_symbol_p},
            {"number?", &Interpreter::eval_number_p},
            {"string?", &Interpreter::eval_string_p},
            {"char?", &Interpreter::eval_char_p},
            {"vector?", &Interpreter::eval_vector_p},
            {"procedure?", &Interpreter::eval_procedure_p},
            {"boolean?", &Interpreter::eval_boolean_p},
            {"type?", &Interpreter::eval_type},

            // Сравнение
            {"eq?", &Interpreter::eval_equals},     // было eval_eq
            {"eqv?", &Interpreter::eval_eqv},

            // Строки
            {"string-append", &Interpreter::eval_string_append},
            {"string-length", &Interpreter::eval_string_length},
            {"string-ref", &Interpreter::eval_string_ref},
            {"string-substr", &Interpreter::eval_string_substr}, // было eval_substring

            // Векторы
            {"vector", &Interpreter::eval_vector},
            {"vector-ref", &Interpreter::eval_vector_ref},
            {"vector-set!", &Interpreter::eval_vector_set},
            {"vector-length", &Interpreter::eval_vector_length},

            // Хэш-таблицы
            {"make-hash-table", &Interpreter::eval_make_hash_table},
            {"hash-table-set!", &Interpreter::eval_hash_table_set},
            {"hash-table-ref", &Interpreter::eval_hash_table_ref},
            {"hash-table?", &Interpreter::eval_hash_table_p},
            {"hash-table-try-ref", &Interpreter::eval_hash_table_try_ref},

            // Системные и ввод-вывод
            {"print", &Interpreter::eval_print},
            {"pprint", &Interpreter::eval_pprint},
            {"inspect", &Interpreter::eval_inspect},
            {"fmt", &Interpreter::eval_format},
            {"error", &Interpreter::eval_error},

            // Files
            {"read", &Interpreter::eval_read},
            {"load-file", &Interpreter::eval_load_file},
            {"read-file", &Interpreter::eval_read_file},
            {"file-exists?", &Interpreter::eval_file_exists_p},
            {"read-data-file", &Interpreter::eval_read_data_file},
            {"try-load-file", &Interpreter::eval_try_load_file},

            // System
            {"system", &Interpreter::eval_system},
            {"get-environment-variable", &Interpreter::eval_get_env}, // было eval_get_environment_variable
            {"current-directory", &Interpreter::eval_current_directory},
            {"exit", &Interpreter::eval_exit},

            // Прочие
            {"gensym", &Interpreter::eval_gensym},
            {"eval", &Interpreter::eval_eval},

            // Преобразования типов
            {"number->string", &Interpreter::eval_number_to_string},
            {"string->number", &Interpreter::eval_string_to_number},
            {"char->integer", &Interpreter::eval_char_to_integer},
            {"integer->char", &Interpreter::eval_integer_to_char},
            {"string->symbol", &Interpreter::eval_string_to_symbol},
            {"symbol->string", &Interpreter::eval_symbol_to_string},

            // Математические функции
            {"abs", &Interpreter::eval_abs},
            {"max", &Interpreter::eval_max},
            {"min", &Interpreter::eval_min},
            {"expt", &Interpreter::eval_expt},
            {"sqrt", &Interpreter::eval_sqrt},
            {"ash", &Interpreter::eval_ash},
        } });
    // load the standard library
    if (load_libs) load_library();
}

void Interpreter::load_library() {
    auto cmd = "(load-file \"lib.gs\")";
    eval_with_rewind(reader.read_from_string(cmd), global_environment.as_env_ptr());
}

void Interpreter::init_builtin_forms(
    const std::unordered_map<std::string,
    Object(Interpreter::*)(const Object&,
        Arguments&,
        const std::shared_ptr<EnvironmentObject>&)>&
    forms) {
    for (const auto& [name, fn] : forms) {
        builtin_forms[(void*)intern_ptr(name).name_ptr] = fn;
    }
}

void Interpreter::init_special_forms(
    const std::unordered_map<std::string,
    Object(Interpreter::*)(const Object&,
        const Object&,
        const std::shared_ptr<EnvironmentObject>&)>&
    forms) {
    for (const auto& [name, fn] : forms) {
        special_forms.push_back(std::make_pair((void*)intern_ptr(name).name_ptr, fn));
    }
}
// ==============================================
// Environment 
// ==============================================

bool Interpreter::try_symbol_lookup(const Object& sym,
    const std::shared_ptr<EnvironmentObject>& env,
    Object* dest) {
    // Boolean проверка
    if (sym.as_symbol().name_ptr == m_true_object.as_symbol().name_ptr) {
        *dest = m_true_object;
        return true;
    }
    if (sym.as_symbol().name_ptr == m_false_object.as_symbol().name_ptr) {
        *dest = m_false_object;
        return true;
    }

    // Итеративный поиск по цепочке окружений
    EnvironmentObject* search_env = env.get();
    while (search_env != nullptr) {
        Object* obj = search_env->find(sym.as_symbol());
        if (obj) {
            *dest = *obj;
            return true;
        }
        search_env = search_env->parent_env.get();
    }

    return false;
}

void Interpreter::set_args_in_env(const Object& form,
    const Arguments& args,
    const ArgumentSpec& arg_spec,
    const std::shared_ptr<EnvironmentObject>& env) {
    if (arg_spec.rest.empty() && args.unnamed.size() != arg_spec.unnamed.size()) {
        throw_eval_error(form, "did not get the expected number of unnamed arguments (got " +
            std::to_string(args.unnamed.size()) + ", expected " +
            std::to_string(arg_spec.unnamed.size()) + ")");
    }
    else if (!arg_spec.rest.empty() && args.unnamed.size() < arg_spec.unnamed.size()) {
        throw_eval_error(form, "args with rest didn't get enough arguments (got " +
            std::to_string(args.unnamed.size()) + " but need at least " +
            std::to_string(arg_spec.unnamed.size()) + ")");
    }

    // unnamed args
    for (size_t i = 0; i < arg_spec.unnamed.size(); i++) {
        env->vars.set(intern_ptr(arg_spec.unnamed.at(i)), args.unnamed.at(i));
    }

    // named args
    for (const auto& kv : arg_spec.named) {
        env->vars.set(intern_ptr(kv.first), args.named.at(kv.first));
    }

    // rest args
    if (!arg_spec.rest.empty()) {
        // will correctly handle the '() case
        Object rest_list = Object::make_empty_list();
        for (auto it = args.rest.rbegin(); it != args.rest.rend(); ++it) {
            rest_list = Object::make_pair(*it, rest_list);
        }
        env->vars.set(intern_ptr(arg_spec.rest), rest_list);
    }
    else {
        if (!args.rest.empty()) {
            throw_eval_error(form, "got too many arguments");
        }
    }
}

void Interpreter::define_var_in_env(const Object& env, const Object& var, const char* name) {
    env.as_env()->vars.set(InternedSymbolPtr{ intern_ptr(name) }, var);
}

// ==============================================
// Tools and utilities 
// ==============================================

Object Interpreter::intern(const std::string& name) {
    return Object::make_symbol(&reader.get_symbol_table(), name.c_str());
}

void Interpreter::throw_eval_error(const Object& o, const std::string& err) {
    throw std::runtime_error("Evaluation error on `" + o.print() + "`: " + err + "\n" +
        reader.get_db().get_info_for(o));
}

InternedSymbolPtr Interpreter::intern_ptr(const std::string& name) {
    return reader.get_symbol_table().intern(name.c_str());
}

// ==============================================
// REPL
// ==============================================

void Interpreter::execute_repl() {
    want_exit = false;
    std::string input;

    //auto repl_env = std::make_shared<EnvironmentObject>();

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
            // read something from the user
            Object code = reader.read_from_string(input, "repl");
            fmt::print("Reader Returned: {}\n", pretty_print::to_string(code));
            // evaluate
            Object result = eval_with_rewind(code, global_environment.as_env_ptr());
            // Print
            printf("%s\n", result.print().c_str());
        }
        catch (const std::exception& e) {
            printf("REPL Error: %s\n", e.what());
        }
    }

    std::cout << "Goodbye!\n";
}


Object Interpreter::eval_string(const std::string& expression, const std::string& filename)
{
    // read something from the user
    Object code = reader.read_from_string(expression, true, filename);
    // evaluate
    return eval_with_rewind(code, global_environment.as_env_ptr());
}

// ==============================================
// Eval 
// ==============================================

std::vector<Object> Interpreter::eval_list(const Object& list, const std::shared_ptr<EnvironmentObject>& env) {
    std::vector<Object> result;
    Object current = list;

    while (current.is_pair()) {
        result.push_back(eval_with_rewind(current.as_pair()->car, env));
        current = current.as_pair()->cdr;
    }

    if (!current.is_empty_list()) {
        throw_eval_error(list, "malformed argument list");
    }

    return result;
}

Object Interpreter::eval_list_return_last(const Object& form,
    Object rest,
    const std::shared_ptr<EnvironmentObject>& env) {
    if (rest.is_empty_list()) {
        return rest;
    }

    const Object* iter = &rest;
    while (true) {
        const Object* next = &iter->as_pair()->cdr;
        const Object* item = &iter->as_pair()->car;
        if (next->is_empty_list()) {
            return eval_with_rewind(*item, env);
        }
        else {
            eval_with_rewind(*item, env);
            iter = next;
        }
    }
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
        if (!disable_printing) {
            printf("-----------------------------------------\n");
            printf("From object %s\nat %s\n", obj.inspect().c_str(), reader.get_db().get_info_for(obj).c_str());
        }
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
    const auto& pair = obj.as_pair();
    const Object& head = pair->car;
    const Object& rest = pair->cdr;

    // first see if we got a symbol:
    if (head.type == ObjectType::SYMBOL) {
        const auto& head_sym = head.as_symbol();

        // try a special form first
        for (const auto& sf : special_forms) {
            if (sf.first == head_sym.name_ptr) {
                return ((*this).*(sf.second))(obj, rest, env);
            }
        }

        // try builtins next
        const auto& kv_b = builtin_forms.find((void*)head_sym.name_ptr);
        if (kv_b != builtin_forms.end()) {
            Arguments args = get_args(obj, rest, make_varargs());
            // all "built-in" forms expect arguments to be evaluated (that's why they aren't special)
            eval_args(&args, env);
            return ((*this).*(kv_b->second))(obj, args, env);
        }

        // try custom forms next
        for (const auto& cf : m_custom_forms) {
            if (cf.first == head_sym.name_ptr) {
                Arguments args = get_args(obj, rest, make_varargs());
                return (cf.second)(obj, args, env);
            }
        }

        // try macros next
        Object macro_obj;
        if (try_symbol_lookup(head, env, &macro_obj) && macro_obj.is_macro()) {
            const auto& macro = macro_obj.as_macro();
            Arguments args = get_args(obj, rest, macro->args);

            auto mac_env_obj = EnvironmentObject::make_new();
            auto mac_env = mac_env_obj.as_env_ptr();
            mac_env->parent_env = env;  // not 100% clear that this is right
            set_args_in_env(obj, args, macro->args, mac_env);
            // expand the macro!
            return eval_with_rewind(eval_list_return_last(macro->body, macro->body, mac_env), env);
        }
    }


    // eval the head and try it as a lambda
    Object eval_head = eval_with_rewind(head, env);

    // Пробуем применить как макрос (вычисленный или найденный по символу)
    if (eval_head.is_macro()) {
        const auto& macro = eval_head.as_macro();
        Arguments args = get_args(obj, rest, macro->args);

        auto mac_env_obj = std::make_shared<EnvironmentObject>();
        auto mac_env = mac_env_obj;
        mac_env->parent_env = env;
        set_args_in_env(obj, args, macro->args, mac_env);

        Object expanded_body = quasiquote_helper(macro->body, mac_env);
        Object expansion = eval_list_return_last(expanded_body, expanded_body, mac_env);
        return eval_with_rewind(expansion, env);
    }


    if (eval_head.type != ObjectType::LAMBDA) {
        throw_eval_error(obj, "head of form didn't evaluate to lambda");
    }

    const auto& lam = eval_head.as_lambda();
    Arguments args = get_args(obj, rest, lam->args);
    eval_args(&args, env);
    auto lam_env_obj = EnvironmentObject::make_new();
    auto lam_env = lam_env_obj.as_env_ptr();
    lam_env->parent_env = lam->parent_env;
    set_args_in_env(obj, args, lam->args, lam_env);
    return eval_list_return_last(lam->body, lam->body, lam_env);
}

Object Interpreter::eval_quote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!rest.is_pair()) {
        throw_eval_error(form, "quote requires one argument");
    }
    return rest.as_pair()->car;
}

Object Interpreter::eval_define(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "define requires arguments");
    }

    Object name_obj = rest.as_pair()->car;
    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "define name must be a symbol");
    }

    Object value_part = rest.as_pair()->cdr;
    if (!value_part.is_pair()) {
        throw_eval_error(form, "define must have a value");
    }

    Object value = eval_with_rewind(value_part.as_pair()->car, env);

    // Сохраняем в ПЕРЕДАННЫЙ environment
    env->vars.set(name_obj.as_symbol(), value);

    return value;
}

Object Interpreter::eval_lambda(const Object& form, const Object& rest,
    const std::shared_ptr<EnvironmentObject>& env) {
    // ...
    Object params_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr;  // ВСЁ тело после параметров

    if (!params_obj.is_list()) {
        throw_eval_error(form, "lambda: parameter list must be a list");
    }

    ArgumentSpec args = parse_arg_spec(form, params_obj);

    if (body_obj.is_empty_list()) {
        throw_eval_error(form, "lambda: expected body after parameter list");
    }

    Object lambda_obj = LambdaObject::make_new();
    auto lambda = lambda_obj.as_lambda();
    lambda->args = args;
    lambda->body = body_obj;  // ← ВСЁ тело, а не только .as_pair()->car!
    lambda->parent_env = env;

    return lambda_obj;
}

Object Interpreter::eval_begin(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form;

    return eval_list_return_last(rest, rest, env);
}


Object Interpreter::eval_if(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "if requires condition and branches");
    }

    Object condition_obj = rest.as_pair()->car;
    Object then_part_obj = rest.as_pair()->cdr;

    if (!then_part_obj.is_pair()) {
        throw_eval_error(form, "if requires then branch");
    }

    Object condition_result = eval_with_rewind(condition_obj, env);

    if (truthy(condition_result)) {
        return eval_with_rewind(then_part_obj.as_pair()->car, env);
    }
    else {
        Object else_part = then_part_obj.as_pair()->cdr;
        if (else_part.is_pair()) {
            return eval_with_rewind(else_part.as_pair()->car, env);
        }
        else {
            return Object::make_empty_list();
        }
    }
}

Object Interpreter::eval_cond(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    Object current_clause = rest;

    while (current_clause.is_pair()) {
        Object clause = current_clause.as_pair()->car;

        if (!clause.is_pair()) {
            throw_eval_error(form, "cond clause must be a pair");
        }

        Object condition = clause.as_pair()->car;
        Object body = clause.as_pair()->cdr;

        // Особый случай: (else ...)
        if (condition.is_symbol() && condition.as_symbol().name_ptr &&
            strcmp(condition.as_symbol().name_ptr, "else") == 0) {
            if (!body.is_pair()) {
                throw_eval_error(form, "cond else clause must have body");
            }
            return eval_with_rewind(body.as_pair()->car, env);
        }

        Object condition_result = eval_with_rewind(condition, env);

        if (truthy(condition_result)) {
            if (body.is_pair()) {
                return eval_with_rewind(body.as_pair()->car, env);
            }
            else {
                return condition_result;
            }
        }

        current_clause = current_clause.as_pair()->cdr;
    }

    return Object::make_empty_list();
}

Object Interpreter::eval_and(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form;
    Object current = rest;
    Object result = m_true_object;

    while (current.is_pair()) {
        result = eval_with_rewind(current.as_pair()->car, env);
        if (!truthy(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return result;
}

Object Interpreter::eval_or(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form;
    Object current = rest;

    while (current.is_pair()) {
        Object result = eval_with_rewind(current.as_pair()->car, env);
        if (truthy(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return m_false_object;
}

Object Interpreter::eval_set(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "set! requires variable and value");
    }

    Object name_obj = rest.as_pair()->car;
    Object value_part = rest.as_pair()->cdr;

    if (!name_obj.is_symbol()) {
        throw_eval_error(form, "set! variable must be a symbol");
    }

    if (!value_part.is_pair()) {
        throw_eval_error(form, "set! requires a value");
    }

    Object value = eval_with_rewind(value_part.as_pair()->car, env);

    if (env) {
        env->vars.set(name_obj.as_symbol(), value);
        return value;
    }

    return value;
}

Object Interpreter::eval_let(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "let requires bindings and body");
    }

    Object bindings_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr;

    if (!bindings_obj.is_list()) {
        throw_eval_error(form, "let bindings must be a list");
    }

    auto let_env = std::make_shared<EnvironmentObject>(env);

    Object current_binding = bindings_obj;
    while (current_binding.is_pair()) {
        Object binding = current_binding.as_pair()->car;

        if (!binding.is_pair()) {
            throw_eval_error(form, "let binding must be a pair (name value)");
        }

        Object name_obj = binding.as_pair()->car;
        Object value_part = binding.as_pair()->cdr;

        if (!name_obj.is_symbol()) {
            throw_eval_error(form, "let binding name must be a symbol");
        }

        if (!value_part.is_pair()) {
            throw_eval_error(form, "let binding must have a value");
        }

        Object value = eval_with_rewind(value_part.as_pair()->car, env);
        let_env->vars.set(name_obj.as_symbol(), value);

        current_binding = current_binding.as_pair()->cdr;
    }

    Object result = Object::make_empty_list();
    Object current_body = body_obj;

    while (current_body.is_pair()) {
        result = eval_with_rewind(current_body.as_pair()->car, let_env);
        current_body = current_body.as_pair()->cdr;
    }
    return result;
}

Object Interpreter::eval_while(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "while requires condition and body");
    }

    Object condition_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr;

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
            result = eval_with_rewind(current_body.as_pair()->car, env);
            current_body = current_body.as_pair()->cdr;
        }
    }

    return result;
}

Object Interpreter::eval_macro(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "macro must receive two arguments");
    }

    Object arg_list = rest.as_pair()->car;
    if (!arg_list.is_pair() && !arg_list.is_empty_list()) {
        throw_eval_error(form, "macro argument list must be a list");
    }

    Object new_macro = MacroObject::make_new();
    auto m = new_macro.as_macro();
    m->args = parse_arg_spec(form, arg_list);

    Object rrest = rest.as_pair()->cdr;
    if (!rrest.is_pair()) {
        throw_eval_error(form, "macro body must be a list");
    }

    m->body = rrest;
    m->parent_env = env;
    return new_macro;
}

Object Interpreter::eval_let_star(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "let* requires bindings and body");
    }

    Object bindings_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr;

    if (!bindings_obj.is_list()) {
        throw_eval_error(form, "let* bindings must be a list");
    }

    auto current_env = env;

    Object current_binding = bindings_obj;
    while (current_binding.is_pair()) {
        Object binding = current_binding.as_pair()->car;

        if (!binding.is_pair()) {
            throw_eval_error(form, "let* binding must be a pair (name value)");
        }

        Object name_obj = binding.as_pair()->car;
        Object value_part = binding.as_pair()->cdr;

        if (!name_obj.is_symbol()) {
            throw_eval_error(form, "let* binding name must be a symbol");
        }

        if (!value_part.is_pair()) {
            throw_eval_error(form, "let* binding must have a value");
        }

        auto new_env = std::make_shared<EnvironmentObject>(current_env);

        Object value = eval_with_rewind(value_part.as_pair()->car, current_env);
        new_env->vars.set(name_obj.as_symbol(), value);

        current_env = new_env;
        current_binding = current_binding.as_pair()->cdr;
    }

    Object result = Object::make_empty_list();
    Object current_body = body_obj;
    while (current_body.is_pair()) {
        result = eval_with_rewind(current_body.as_pair()->car, current_env);
        current_body = current_body.as_pair()->cdr;
    }
    return result;
}

Object Interpreter::eval_quasiquote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    if (rest.type != ObjectType::PAIR || rest.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
        throw_eval_error(form, "quasiquote must have one argument!");
    }
    return quasiquote_helper(rest.as_pair()->car, env);
}

Object build_list_with_spliced_tail(std::vector<Object>&& objects, const Object& tail) {
    if (objects.empty()) {
        return tail;
    }

    std::shared_ptr<PairObject> head = std::make_shared<PairObject>(objects.back(), tail);

    s64 idx = ((s64)objects.size()) - 2;
    while (idx >= 0) {
        Object next;
        next.type = ObjectType::PAIR;
        next.heap_obj = std::move(head);

        head = std::make_shared<PairObject>();
        head->car = std::move(objects[idx]);
        head->cdr = std::move(next);

        idx--;
    }

    Object result;
    result.type = ObjectType::PAIR;
    result.heap_obj = std::move(head);
    return result;
}

Object Interpreter::quasiquote_helper(const Object& form,
    const std::shared_ptr<EnvironmentObject>& env) {
    const Object* lst_iter = &form;
    std::vector<Object> result;
    for (;;) {
        if (lst_iter->type == ObjectType::PAIR) {
            const Object& item = lst_iter->as_pair()->car;
            if (item.type == ObjectType::PAIR) {
                if (item.as_pair()->car.type == ObjectType::SYMBOL &&
                    item.as_pair()->car.as_symbol() == "unquote") {
                    const Object& unquote_arg = item.as_pair()->cdr;
                    if (unquote_arg.type != ObjectType::PAIR ||
                        unquote_arg.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
                        throw_eval_error(form, "unquote must have exactly 1 arg");
                    }
                    result.push_back(eval_with_rewind(unquote_arg.as_pair()->car, env));
                    lst_iter = &lst_iter->as_pair()->cdr;
                    continue;
                }
                else if (item.as_pair()->car.type == ObjectType::SYMBOL &&
                    item.as_pair()->car.as_symbol() == "unquote-splicing") {
                    const Object& unquote_arg = item.as_pair()->cdr;
                    if (unquote_arg.type != ObjectType::PAIR ||
                        unquote_arg.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
                        throw_eval_error(form, "unquote must have exactly 1 arg");
                    }

                    // bypass normal addition:
                    lst_iter = &lst_iter->as_pair()->cdr;
                    Object splice_result = eval_with_rewind(unquote_arg.as_pair()->car, env);
                    if (lst_iter->type == ObjectType::EMPTY_LIST) {
                        // optimization!
                        return build_list_with_spliced_tail(std::move(result), splice_result);
                    }

                    const Object* to_add = &splice_result;
                    for (;;) {
                        if (to_add->type == ObjectType::PAIR) {
                            result.push_back(to_add->as_pair()->car);
                            to_add = &to_add->as_pair()->cdr;
                        }
                        else if (to_add->type == ObjectType::EMPTY_LIST) {
                            break;
                        }
                        else {
                            throw_eval_error(form, "malformed unquote-splicing result");
                        }
                    }
                    continue;
                }
                else {
                    lst_iter = &lst_iter->as_pair()->cdr;

                    if (item.is_pair()) {
                        result.push_back(quasiquote_helper(item, env));
                    }
                    else {
                        result.push_back(item);
                    }
                    continue;
                }
            }
            result.push_back(item);
            lst_iter = &lst_iter->as_pair()->cdr;
        }
        else if (lst_iter->type == ObjectType::EMPTY_LIST) {
            return build_list(std::move(result));
        }
        else {
            throw_eval_error(form, "malformed quasiquote");
        }
    }
}

// ==============================================
// Конвертирование типов
// ==============================================

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
    return 0;
}

bool Interpreter::is_number(const Object& obj) {
    return obj.is_integer() || obj.is_float();
}

// ==============================================
// Работа с аргументами
// ==============================================

/*
 * FUNCTION: vararg_check
 *
 * Validates function arguments against expected patterns for type safety.
 * This is the C++ equivalent of Common Lisp's &key and &optional argument checking.
 *
 * PARAMETERS:
 *   form    - The form being evaluated (for error reporting)
 *   args    - Actual arguments passed to the function
 *   unnamed - Pattern for unnamed (positional) arguments
 *   named   - Pattern for named (keyword) arguments
 *
 * UNNAMED ARGUMENT PATTERNS:
 *   {}                    - Any number of arguments allowed (variadic)
 *   {{}}                  - Exactly one argument, any type
 *   {{}, {}}              - Exactly two arguments, any types
 *   {ObjectType::INTEGER} - Exactly one INTEGER argument
 *   {ObjectType::STRING, {}} - Two args: STRING required, second any type
 *
 * NAMED ARGUMENT PATTERNS:
 *   {{"key", {true, ObjectType::STRING}}}   - Required STRING :key
 *   {{"key", {false, ObjectType::INTEGER}}} - Optional INTEGER :key
 *   {{"key", {false, {}}}}                  - Optional any type :key
 *
 * USAGE EXAMPLES:
 *
 *   // Mathematical operations (variadic)
 *   vararg_check(form, args, {}, {});
 *
 *   // Binary operations
 *   vararg_check(form, args, {{}, {}}, {});
 *
 *   // Type-specific functions
 *   vararg_check(form, args, {ObjectType::PAIR}, {});          // car
 *   vararg_check(form, args, {ObjectType::STRING, ObjectType::INTEGER}, {}); // string-ref
 *
 *   // With keyword arguments
 *   vararg_check(form, args, {{}}, {{"base", {false, ObjectType::INTEGER}}}); // number->string
 *
 * ERROR MESSAGES:
 *   "expected X unnamed arguments, got Y"
 *   "argument N: expected TYPE1, got TYPE2"
 *   "required named argument 'KEY' missing"
 *   "named argument 'KEY': expected TYPE1, got TYPE2"
 *   "unexpected named argument 'KEY'"
 *
 * NOTES:
 *   - Empty unnamed vector means "any number of arguments" (variadic)
 *   - Use {} for "any type" instead of explicit type specification
 *   - Check function semantics: + is variadic, / is binary, car takes one pair
 *   - Named arguments are checked after unnamed arguments
 *
 * SEE ALSO:
 *   get_args(), eval_args(), ArgumentSpec, Arguments
 */
void Interpreter::vararg_check(
    const Object& form,
    const Arguments& args,
    const std::vector<std::optional<ObjectType>>& unnamed,
    const std::unordered_map<std::string, std::pair<bool, std::optional<ObjectType>>>& named) {

    // Проверка unnamed аргументов
    if (!unnamed.empty()) {
        if (args.unnamed.size() != unnamed.size()) {
            throw_eval_error(form, fmt::format("expected {} unnamed arguments, got {}",
                unnamed.size(), args.unnamed.size()));
        }

        for (size_t i = 0; i < unnamed.size(); ++i) {
            if (unnamed[i].has_value() && args.unnamed[i].type != unnamed[i].value()) {
                std::string expected = object_type_to_string(unnamed[i].value());
                std::string got = object_type_to_string(args.unnamed[i].type);
                throw_eval_error(form, fmt::format("argument {}: expected {}, got {}", i, expected, got));
            }
        }
    }
    // ЕСЛИ unnamed пустой - ЛЮБОЕ количество аргументов разрешено (не проверяем количество)

    // Проверка named аргументов
    for (const auto& [name, spec] : named) {
        auto it = args.named.find(name);
        if (spec.first) { // required
            if (it == args.named.end()) {
                throw_eval_error(form, fmt::format("required named argument '{}' missing", name));
            }
        }

        if (it != args.named.end() && spec.second.has_value() &&
            it->second.type != spec.second.value()) {
            std::string expected = object_type_to_string(spec.second.value());
            std::string got = object_type_to_string(it->second.type);
            throw_eval_error(form, fmt::format("named argument '{}': expected {}, got {}", name, expected, got));
        }
    }

    // Проверка лишних named аргументов
    for (const auto& [name, _] : args.named) {
        if (named.find(name) == named.end()) {
            throw_eval_error(form, fmt::format("unexpected named argument '{}'", name));
        }
    }
}

Arguments Interpreter::get_args(const Object& form, const Object& rest, const ArgumentSpec& spec) {
    Arguments args;

    // loop over forms in list
    const Object* current = &rest;
    while (!current->is_empty_list()) {
        const auto& arg = current->as_pair()->car;

        // did we get a ":keyword"
        if (arg.is_symbol() && arg.as_symbol().name_ptr && arg.as_symbol().name_ptr[0] == ':') {
            auto key_name = std::string(arg.as_symbol().name_ptr + 1);
            const auto& kv = spec.named.find(key_name);

            // check for unknown key name
            if (!spec.varargs && kv == spec.named.end()) {
                throw_eval_error(form, fmt::format("Key argument {} wasn't expected", key_name));
            }

            // check for multiple definition of key
            if (args.named.find(key_name) != args.named.end()) {
                throw_eval_error(form, fmt::format("Key argument {} multiply defined", key_name));
            }

            // check for well-formed :key value expression
            current = &current->as_pair()->cdr;
            if (current->is_empty_list()) {
                throw_eval_error(form, "Key argument didn't have a value");
            }

            args.named[key_name] = current->as_pair()->car;
        }
        else {
            // not a keyword. Add to unnamed or rest, depending on what we expect
            if (spec.varargs || args.unnamed.size() < spec.unnamed.size()) {
                args.unnamed.push_back(arg);
            }
            else {
                args.rest.push_back(arg);
            }
        }
        current = &current->as_pair()->cdr;
    }

    // Check expected key args and set default values on unset ones if possible
    for (auto& kv : spec.named) {
        const auto& defined_kv = args.named.find(kv.first);
        if (defined_kv == args.named.end()) {
            // key arg not given by user, try to use a default value.
            if (kv.second.has_default) {
                args.named[kv.first] = kv.second.default_value;
            }
            else {
                throw_eval_error(form,
                    "key argument \"" + kv.first + "\" wasn't given and has no default value");
            }
        }
    }

    // Check argument size, if spec defines it
    if (!spec.varargs) {
        if (args.unnamed.size() < spec.unnamed.size()) {
            throw_eval_error(form, "didn't get enough arguments");
        }

        if (!args.rest.empty() && spec.rest.empty()) {
            throw_eval_error(form, "got too many arguments");
        }
    }

    return args;
}
ArgumentSpec Interpreter::parse_arg_spec(const Object& form, Object& rest) {
    ArgumentSpec spec;

    Object current = rest;
    while (!current.is_empty_list()) {
        auto arg = current.as_pair()->car;
        if (!arg.is_symbol()) {
            throw_eval_error(form, "args must be symbols");
        }

        std::string arg_name = arg.as_symbol().name_ptr ? arg.as_symbol().name_ptr : "";

        if (arg_name == "&rest") {
            // special case for &rest
            current = current.as_pair()->cdr;
            if (!current.is_pair()) {
                throw_eval_error(form, "rest arg must have a name");
            }
            auto rest_name = current.as_pair()->car;
            if (!rest_name.is_symbol()) {
                throw_eval_error(form, "rest name must be a symbol");
            }

            spec.rest = rest_name.as_symbol().name_ptr ? rest_name.as_symbol().name_ptr : "";

            if (!current.as_pair()->cdr.is_empty_list()) {
                throw_eval_error(form, "rest must be the last argument");
            }
            break;
        }
        else if (arg_name == "&key") {
            // special case for &key
            current = current.as_pair()->cdr;
            auto key_arg = current.as_pair()->car;
            if (key_arg.is_symbol()) {
                // form is &key name
                auto key_arg_name = key_arg.as_symbol().name_ptr ? key_arg.as_symbol().name_ptr : "";
                if (spec.named.find(key_arg_name) != spec.named.end()) {
                    throw_eval_error(form, fmt::format("key argument {} multiply defined", key_arg_name));
                }
                spec.named[key_arg_name] = NamedArg();
            }
            else if (key_arg.is_pair()) {
                // form is &key (name default-value)
                auto key_iter = key_arg;
                auto kn = key_iter.as_pair()->car;
                key_iter = key_iter.as_pair()->cdr;
                if (!kn.is_symbol()) {
                    throw_eval_error(form, "key argument must have a symbol as a name");
                }
                auto key_arg_name = kn.as_symbol().name_ptr ? kn.as_symbol().name_ptr : "";
                if (spec.named.find(key_arg_name) != spec.named.end()) {
                    throw_eval_error(form, fmt::format("key argument {} multiply defined", key_arg_name));
                }
                NamedArg na;

                if (!key_iter.is_pair()) {
                    throw_eval_error(form, "invalid keyword argument definition");
                }

                na.has_default = true;
                na.default_value = key_iter.as_pair()->car;

                if (!key_iter.as_pair()->cdr.is_empty_list()) {
                    throw_eval_error(form, "invalid keyword argument definition");
                }

                spec.named[key_arg_name] = na;
            }
            else {
                throw_eval_error(form, "invalid key argument");
            }
        }
        else {
            spec.unnamed.push_back(arg_name);
        }

        current = current.as_pair()->cdr;
    }
    return spec;
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

// ==============================================
// Добавь реализацию eval_type
// ==============================================


Object Interpreter::eval_type(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { ObjectType::SYMBOL, {} }, {});

    auto type_name = args.unnamed[0].as_symbol().name_ptr;
    auto kv = string_to_type.find(type_name);
    if (kv == string_to_type.end()) {
        throw_eval_error(form, fmt::format("invalid type name: {}", type_name));
    }

    return make_bool(args.unnamed[1].type == kv->second);
}

// ==============================================
// Системные функции(print, pprint, inspect)
// ==============================================


Object Interpreter::eval_print(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, {});

    if (!disable_printing) {
        printf("%s\n", args.unnamed.at(0).print().c_str());
    }
    return Object::make_empty_list();
}

Object Interpreter::eval_pprint(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {});

    if (!disable_printing) {
        std::cout << pretty_print::to_string(args.unnamed.at(0), 100) << std::endl;
    }
    return Object::make_empty_list();
}

Object Interpreter::eval_inspect(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {});

    if (!disable_printing) {
        printf("%s\n", args.unnamed.at(0).inspect().c_str());
    }
    return Object::make_empty_list();
}

Object Interpreter::eval_format(const Object& form,
    Arguments& args,
    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "format must get at least two arguments");
    }

    auto dest = args.unnamed.at(0);
    auto format_str = args.unnamed.at(1);
    if (!format_str.is_string()) {
        throw_eval_error(form, "format string must be a string");
    }

    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 2; i < args.unnamed.size(); i++) {
        if (args.unnamed.at(i).is_string()) {
            arg_store.push_back(args.unnamed.at(i).as_string()->data);
        }
        else {
            arg_store.push_back(args.unnamed.at(i).print());
        }
    }

    auto formatted = fmt::vformat(format_str.as_string()->data, arg_store);
    if (truthy(dest)) {
        lg::print("{}", formatted.c_str());
    }

    return Object::make_string(formatted);
}

Object Interpreter::eval_error(const Object& form,
    Arguments& args,
    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {});
    throw_eval_error(form, "Error: " + args.unnamed.at(0).as_string()->data);
    return Object::make_empty_list();
}

// ==============================================
// Математические функции с проверками
// ==============================================

Object Interpreter::eval_plus(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        return Object::make_integer(0);
    }

    // Проверяем что все аргументы - числа
    for (const auto& arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "+ requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 0;
        for (const auto& arg : args.unnamed) {
            result += number_to_integer(arg);
        }
        return Object::make_integer(result);
    }
    else {
        FloatType result = 0.0;
        for (const auto& arg : args.unnamed) {
            result += number_to_float(arg);
        }
        return Object::make_float(result);
    }
}

Object Interpreter::eval_minus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;

    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "- must receive at least one unnamed argument!");
    }

    // Проверяем что все аргументы - числа
    for (const auto& arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "- requires number arguments");
        }
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
    else {
        if (args.unnamed.size() == 1) {
            return Object::make_float(-number_to_float(args.unnamed[0]));
        }
        FloatType result = number_to_float(args.unnamed[0]);
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            result -= number_to_float(args.unnamed[i]);
        }
        return Object::make_float(result);
    }
}

Object Interpreter::eval_times(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;

    if (!args.named.empty() || args.unnamed.empty()) {
        return Object::make_integer(1);
    }

    // Проверяем что все аргументы - числа
    for (const auto& arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "* requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 1;
        for (const auto& arg : args.unnamed) {
            result *= number_to_integer(arg);
        }
        return Object::make_integer(result);
    }
    else {
        FloatType result = 1.0;
        for (const auto& arg : args.unnamed) {
            result *= number_to_float(arg);
        }
        return Object::make_float(result);
    }
}

Object Interpreter::eval_divide(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {});

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "/ requires number arguments");
    }

    FloatType numerator = number_to_float(args.unnamed[0]);
    FloatType denominator = number_to_float(args.unnamed[1]);

    if (denominator == 0.0) {
        throw_eval_error(form, "/: division by zero");
    }

    return Object::make_float(numerator / denominator);
}

Object Interpreter::eval_abs(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "abs must receive at least one unnamed argument!");
    }

    if (!is_number(args.unnamed[0])) {
        throw_eval_error(form, "abs requires a number argument");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t val = args.unnamed[0].as_integer();
        return Object::make_integer(val < 0 ? -val : val);
    }
    else {
        double val = args.unnamed[0].as_float();
        return Object::make_float(val < 0 ? -val : val);
    }
}

Object Interpreter::eval_max(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "max must receive at least one unnamed argument!");
    }


    if (args.unnamed.empty()) {
        throw_eval_error(form, "max requires at least one argument");
    }

    // Проверяем что все аргументы - числа
    for (const auto& arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "max requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        int64_t max_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            int64_t val = number_to_integer(args.unnamed[i]);
            if (val > max_val) max_val = val;
        }
        return Object::make_integer(max_val);
    }
    else {
        double max_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            double val = number_to_float(args.unnamed[i]);
            if (val > max_val) max_val = val;
        }
        return Object::make_float(max_val);
    }
}

Object Interpreter::eval_min(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "min must receive at least one unnamed argument!");
    }


    if (args.unnamed.empty()) {
        throw_eval_error(form, "min requires at least one argument");
    }

    // Проверяем что все аргументы - числа
    for (const auto& arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "min requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        int64_t min_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            int64_t val = number_to_integer(args.unnamed[i]);
            if (val < min_val) min_val = val;
        }
        return Object::make_integer(min_val);
    }
    else {
        double min_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            double val = number_to_float(args.unnamed[i]);
            if (val < min_val) min_val = val;
        }
        return Object::make_float(min_val);
    }
}

Object Interpreter::eval_expt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {});

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "expt requires number arguments");
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
    vararg_check(form, args, { {} }, {}); // Один аргумент

    if (!is_number(args.unnamed[0])) {
        throw_eval_error(form, "sqrt requires a number argument");
    }

    double val = number_to_float(args.unnamed[0]);
    if (val < 0) {
        throw_eval_error(form, "sqrt: negative argument");
    }

    return Object::make_float(std::sqrt(val));
}


Object Interpreter::eval_ash(const Object& form,
    Arguments& args,
    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {});
    auto val = number_to_integer(args.unnamed.at(0));
    auto sa = number_to_integer(args.unnamed.at(1));
    if (sa >= 0 && sa < 64) {
        return Object::make_integer(val << sa);
    }
    else if (sa > -64) {
        return Object::make_integer(val >> -sa);
    }
    else {
        throw_eval_error(form, fmt::format("Shift amount {} is out of range", sa));
        return Object::make_empty_list();
    }
}


// ==============================================
// Функции сравнения с проверками
// ==============================================


Object Interpreter::eval_numequals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return make_bool(a_val == b_val);
}

Object Interpreter::eval_lt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "< requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return make_bool(a_val < b_val);
}

Object Interpreter::eval_gt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "> requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return make_bool(a_val > b_val);
}

Object Interpreter::eval_leq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "<= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return make_bool(a_val <= b_val);
}

Object Interpreter::eval_geq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, ">= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return make_bool(a_val >= b_val);
}

// ==============================================
// Функции работы со списками с проверками
// ==============================================

Object Interpreter::eval_cons(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два любых аргумента
    return Object::make_pair(args.unnamed[0], args.unnamed[1]);
}

Object Interpreter::eval_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::PAIR }, {}); // Один pair

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    return args.unnamed[0].as_pair()->car;
}

Object Interpreter::eval_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::PAIR }, {}); // Один pair

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    return args.unnamed[0].as_pair()->cdr;
}

Object Interpreter::eval_list_func(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, {}, {}); 

    Object result = Object::make_empty_list();
    for (auto it = args.unnamed.rbegin(); it != args.unnamed.rend(); ++it) {
        result = Object::make_pair(*it, result);
    }
    return result;
}


Object Interpreter::eval_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент (список)

    Object lst = args.unnamed[0];
    int count = 0;

    while (lst.is_pair()) {
        count++;
        lst = lst.as_pair()->cdr;
    }

    if (!lst.is_empty_list()) {
        throw_eval_error(form, "length requires a proper list");
    }

    return Object::make_integer(count);
}

Object Interpreter::eval_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    if (args.unnamed.empty()) {
        return Object::make_empty_list();
    }

    Object result = args.unnamed.back();

    for (int i = args.unnamed.size() - 2; i >= 0; --i) {
        Object current = args.unnamed[i];

        Object reversed = Object::make_empty_list();
        while (current.is_pair()) {
            reversed = Object::make_pair(current.as_pair()->car, reversed);
            current = current.as_pair()->cdr;
        }

        while (reversed.is_pair()) {
            result = Object::make_pair(reversed.as_pair()->car, result);
            reversed = reversed.as_pair()->cdr;
        }
    }

    return result;
}

// ==============================================
// Предикаты типов с проверками
// ==============================================


Object Interpreter::eval_null_p(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_empty_list());
}

Object Interpreter::eval_pair_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_pair());
}

Object Interpreter::eval_symbol_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_symbol());
}

Object Interpreter::eval_number_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_integer() || args.unnamed[0].is_float());
}

Object Interpreter::eval_string_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_string());
}

Object Interpreter::eval_char_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_char());
}

Object Interpreter::eval_vector_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_array());
}

Object Interpreter::eval_procedure_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    bool is_proc = args.unnamed[0].is_lambda() ||
        args.unnamed[0].is_macro() ||
        (args.unnamed[0].is_symbol() &&
            builtin_forms.find((void*)args.unnamed[0].as_symbol().name_ptr) != builtin_forms.end());
    return make_bool(is_proc);
}

Object Interpreter::eval_boolean_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    const Object& obj = args.unnamed[0];
    bool is_bool = (obj.is_symbol() && obj.as_symbol().name_ptr &&
        (strcmp(obj.as_symbol().name_ptr, "#t") == 0 ||
            strcmp(obj.as_symbol().name_ptr, "#f") == 0));
    return make_bool(is_bool);
}

// ==============================================
// Функции сравнения
// ==============================================

Object Interpreter::eval_equals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    vararg_check(form, args, { {}, {} }, {});
    return make_bool(args.unnamed[0] == args.unnamed[1]);
}

Object Interpreter::eval_eqv(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {}, {} }, {}); // Два аргумента
    return make_bool(args.unnamed[0] == args.unnamed[1]);
}

// ==============================================
// Строковые функции с проверками
// ==============================================


Object Interpreter::eval_string_length(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка
    return Object::make_integer(args.unnamed[0].as_string()->length());
}

Object Interpreter::eval_string_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING, ObjectType::INTEGER }, {}); // Строка и индекс

    const std::string& str = args.unnamed[0].as_string()->data;
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(str.length())) {
        throw_eval_error(form, "string-ref: index out of range");
    }

    return Object::make_char(str[index]);
}

Object Interpreter::eval_string_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, {}, {});

    std::string result;
    for (const auto& arg : args.unnamed) {
        if (!arg.is_string()) {
            throw_eval_error(form, "string-append requires string arguments");
        }
        result += arg.as_string()->data;
    }
    return Object::make_string(result);
}

Object Interpreter::eval_string_substr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING, ObjectType::INTEGER, ObjectType::INTEGER }, {}); // Строка, начало, конец

    const std::string& str = args.unnamed[0].as_string()->data;
    int64_t start = args.unnamed[1].as_integer();
    int64_t end = args.unnamed[2].as_integer();

    if (start < 0 || end > static_cast<int64_t>(str.length()) || start > end) {
        throw_eval_error(form, "substring: invalid start or end index");
    }

    return Object::make_string(str.substr(start, end - start));
}

Object Interpreter::eval_string_to_symbol(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка
    return Object::make_symbol(&reader.get_symbol_table(), args.unnamed[0].as_string()->c_str());
}

Object Interpreter::eval_symbol_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::SYMBOL }, {}); // Один символ
    return Object::make_string(args.unnamed[0].as_symbol().name_ptr ?
        args.unnamed[0].as_symbol().name_ptr : "");
}

// ==============================================
// Векторные функции с проверками
// ==============================================


Object Interpreter::eval_vector(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, {}, {}); // Любое количество элементов
    return Object::make_array(args.unnamed);
}

Object Interpreter::eval_vector_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::ARRAY, ObjectType::INTEGER }, {}); // Вектор и индекс

    auto elements = args.unnamed[0].as_array();
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(elements->size())) {
        throw_eval_error(form, "vector-ref: index out of range");
    }

    return elements->at(index);
}

Object Interpreter::eval_vector_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::ARRAY, ObjectType::INTEGER, {} }, {}); // Вектор, индекс, значение

    auto vec_ptr = dynamic_cast<ArrayObject*>(args.unnamed[0].heap_obj.get());
    if (!vec_ptr) {
        throw_eval_error(form, "vector-set!: not a vector");
    }

    int64_t index = args.unnamed[1].as_integer();
    if (index < 0 || index >= static_cast<int64_t>(vec_ptr->data.size())) {
        throw_eval_error(form, "vector-set!: index out of range");
    }

    vec_ptr->data[index] = args.unnamed[2];
    return args.unnamed[2];
}

Object Interpreter::eval_vector_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::ARRAY }, {}); // Один вектор
    return Object::make_integer(args.unnamed[0].as_array()->size());
}

// ==============================================
// Хэш - таблицы с проверками
// ==============================================


Object Interpreter::eval_make_hash_table(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // Без аргументов
    return Object::make_hash_table();
}

Object Interpreter::eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING_HASH_TABLE, {}, {} }, {}); // Таблица, ключ, значение

    auto ht = args.unnamed[0].as_hash_table();
    std::string key;

    if (args.unnamed[1].is_string()) {
        key = args.unnamed[1].as_string()->data;
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
    vararg_check(form, args, { ObjectType::STRING_HASH_TABLE, {} }, {}); // Таблица и ключ

    auto ht = args.unnamed[0].as_hash_table();
    std::string key;

    if (args.unnamed[1].is_string()) {
        key = args.unnamed[1].as_string()->data;
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
// Try to look up a value by key in a hash table.The result is a pair of(success.value).

Object Interpreter::eval_hash_table_try_ref(const Object & form,
    Arguments & args,
    const std::shared_ptr<EnvironmentObject>& /*env*/) {
    vararg_check(form, args, { ObjectType::STRING_HASH_TABLE, {} }, {});
    const auto* table = args.unnamed.at(0).as_hash_table();

    const char* str = nullptr;
    if (args.unnamed.at(1).is_symbol()) {
        str = args.unnamed.at(1).as_symbol().name_ptr;
    }
    else if (args.unnamed.at(1).is_string()) {
        str = args.unnamed.at(1).as_string()->c_str();
    }
    else {
        throw_eval_error(form, "Hash table must use symbol or string as the key.");
    }
    const auto& it = table->data.find(str);
    if (it == table->data.end()) {
        // not in table
        return Object::make_pair(m_false_object, Object::make_empty_list());
    }
    else {
        return Object::make_pair(m_true_object, it->second);
    }
}

Object Interpreter::eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_hash_table());
}

// ==============================================
// Системные функции с проверками
// ==============================================

Object Interpreter::eval_read(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка

    try {
        return reader.read_from_string(args.unnamed[0].as_string()->data, "read input");
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("read error: ") + e.what());
    }
    return m_false_object;
}

Object Interpreter::eval_load_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)

    try {
        Object code = reader.read_from_file({ args.unnamed[0].as_string()->data }, true, true);
        return eval_with_rewind(code, env);
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("load-file error: ") + e.what());
    }
    return m_false_object;
}

Object Interpreter::eval_read_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    std::string content = read_entire_file(filename);
    return reader.read_from_string(content, true, filename);
}

/*!
 * Combines read-file and eval to load in a file. Return #f if it doesn't exist.
 */
Object Interpreter::eval_try_load_file(const Object& form,
    Arguments& args,
    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {});

    auto path = args.unnamed.at(0).as_string()->data;
    std::ifstream file(path);
    bool exists = file.good();
    if (!exists) {
        return m_false_object;
    }

    Object o;
    try {
        o = reader.read_from_file({ path }, true, true);
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("reader error inside of try-load-file:\n") + e.what());
    }

    try {
        return eval_with_rewind(o, global_environment.as_env_ptr());
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("eval error inside of try-load-file:\n") + e.what());
    }
    return m_true_object;
}

Object Interpreter::eval_file_exists_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    std::ifstream file(filename);
    bool exists = file.good();
    file.close();

    return make_bool(exists);
}

//  Reads list data from a file, returns the pair.Not a lot of safety here!
Object Interpreter::eval_read_data_file(const Object & form,
    Arguments & args,
    const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {});

    try {
        return reader.read_from_file({ args.unnamed.at(0).as_string()->data}, true, false).as_pair()->cdr;
    }
    catch (std::runtime_error& e) {
        throw_eval_error(form, std::string("reader error inside of read-file:\n") + e.what());
    }
    return Object::make_empty_list();
}


Object Interpreter::eval_current_directory(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, {}, {}); // Без аргументов

    try {
        std::string cwd = std::filesystem::current_path().string();
        return Object::make_string(cwd);
    }
    catch (const std::exception& e) {
        throw_eval_error(form, "cannot get current directory");
    }
    return m_false_object;
}

std::string Interpreter::read_entire_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    return std::string((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
}

// ==============================================
// Системные методы
// ==============================================

Object Interpreter::eval_get_env(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя переменной)

    std::string var_name = args.unnamed[0].as_string()->data;
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
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (команда)

    std::string command = args.unnamed[0].as_string()->data;
    int result = std::system(command.c_str());

    return Object::make_integer(result);
}


Object Interpreter::eval_exit(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)form; (void)args; (void)env;
    want_exit = true;
    return Object::make_empty_list();
}


// ==============================================
// Прочие функции с проверками
// ==============================================

Object Interpreter::eval_gensym(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)form; (void)args; (void)env;
    vararg_check(form, args, {}, {}); // Без аргументов

    std::string name = "gensym" + std::to_string(gensym_id++);
    return Object::make_symbol(&reader.get_symbol_table(), name.c_str());
}

Object Interpreter::eval_eval(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return eval_with_rewind(args.unnamed[0], env);
}

Object Interpreter::eval_set_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::PAIR, {} }, {}); // Пара и значение

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-car! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->car = args.unnamed[1];
    return args.unnamed[0];
}

Object Interpreter::eval_set_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::PAIR, {} }, {}); // Пара и значение

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-cdr! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->cdr = args.unnamed[1];
    return args.unnamed[0];
}

// ==============================================
// Функции преобразования типов с проверками
// ==============================================

Object Interpreter::eval_number_to_string(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, { {"base", {false, ObjectType::INTEGER}} });

    // Проверяем что это ЧИСЛО (любое)
    if (!is_number(args.unnamed[0])) {
        throw_eval_error(form, "number->string requires a number argument");
    }

    if (args.unnamed[0].is_integer()) {
        int64_t num = args.unnamed[0].as_integer();
        if (args.has_named("base")) {
            int64_t base = args.get_named("base").as_integer();
            if (base == 16) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%llx", (long long)num);
                return Object::make_string(buffer);
            }
        }
        return Object::make_string(std::to_string(num));
    }
    else {
        double num = args.unnamed[0].as_float();
        return Object::make_string(std::to_string(num));
    }
}

Object Interpreter::eval_string_to_number(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, { {"base", {false, ObjectType::INTEGER}} }); // Строка и опционально основание

    std::string str = args.unnamed[0].as_string()->data;
    int base = 10;

    if (args.has_named("base")) {
        base = args.get_named("base").as_integer();
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
    vararg_check(form, args, { ObjectType::CHAR }, {}); // Один символ
    return Object::make_integer(static_cast<int64_t>(args.unnamed[0].as_char()));
}

Object Interpreter::eval_integer_to_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::INTEGER }, {}); // Одно целое

    int64_t code = args.unnamed[0].as_integer();
    if (code < 0 || code > 255) {
        throw_eval_error(form, "integer->char: code out of range 0-255");
    }

    return Object::make_char(static_cast<char>(code));
}

} // namespace script