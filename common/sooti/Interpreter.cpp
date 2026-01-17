#include "common/sooti/Interpreter.hpp"
#include "common/sooti/PrettyPrinter.hpp"

#include "fmt/args.h"
#include "fmt/base.h"
#include "fmt/format.h"
#include "fmt/color.h"
#include "common/util/Log.hpp"
#include "common/util/Crc32.hpp"
#include "common/util/FileUtil.hpp"
#include "common/util/StringUtil.hpp"
#include "common/util/UnicodeUtil.hpp"
#include "common/CommonTypes.hpp"
#include "common/versions/version.h"
#include "common/versions/revision.h"
#include <sstream>
#include <filesystem>
#include <set>

namespace script 
{
    Interpreter::Interpreter(const std::string& username, bool load_libs) : reader(this) {
        // Инициализируем boolean объекты как символы
        auto& symbols = reader.get_symbol_table();
        true_object = Object::make_symbol(&symbols, "#t");
        false_object = Object::make_symbol(&symbols, "#f");

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
            {"environment", ObjectType::ENVIRONMENT},
            {"reader", ObjectType::READER},
            {"lextoken", ObjectType::LEXTOKEN},
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
            {"+",   &Interpreter::eval_plus},
            {"-",   &Interpreter::eval_minus},
            {"*",   &Interpreter::eval_times},
            {"/",   &Interpreter::eval_divide},
            {"=",   &Interpreter::eval_numequals},  // было eval_equals
            {"<",   &Interpreter::eval_lt},
            {">",   &Interpreter::eval_gt},
            {"<=",  &Interpreter::eval_leq},
            {">=",  &Interpreter::eval_geq},

            // Списки и пары
            {"cons",     &Interpreter::eval_cons},      // было eval_cons_builtin
            {"car",      &Interpreter::eval_car},        // было eval_car_builtin
            {"cdr",      &Interpreter::eval_cdr},        // было eval_cdr_builtin
            {"set-car!", &Interpreter::eval_set_car},
            {"set-cdr!", &Interpreter::eval_set_cdr},
            {"list",     &Interpreter::eval_list_func},
            {"length",   &Interpreter::eval_length},
            {"append",   &Interpreter::eval_append},
            {"apply",    &Interpreter::eval_apply},

            // Предикаты типов
            {"type-of",     &Interpreter::eval_type_of},
            {"type?",       &Interpreter::eval_type_p},
            {"null?",       &Interpreter::eval_null_p},     // было eval_null_p
            {"pair?",       &Interpreter::eval_pair_p},
            {"symbol?",     &Interpreter::eval_symbol_p},
            {"number?",     &Interpreter::eval_number_p},
            {"string?",     &Interpreter::eval_string_p},
            {"char?",       &Interpreter::eval_char_p},
            {"vector?",     &Interpreter::eval_vector_p},
            {"procedure?",  &Interpreter::eval_procedure_p},
            {"boolean?",    &Interpreter::eval_boolean_p},
            {"reader?",     &Interpreter::eval_reader_p},
            {"lextoken?",   &Interpreter::eval_lextoken_p},

            // Сравнение
            {"eq?",     &Interpreter::eval_equals},     // было eval_eq
            {"eqv?",    &Interpreter::eval_eqv},

            // Строки
            {"string-append",   &Interpreter::eval_string_append},
            {"string-length",   &Interpreter::eval_string_length},
            {"string-ref",      &Interpreter::eval_string_ref},
            {"string-substr",   &Interpreter::eval_string_substr}, // было eval_substring

            // Векторы
            {"vector",          &Interpreter::eval_vector},
            {"vector-ref",      &Interpreter::eval_vector_ref},
            {"vector-set!",     &Interpreter::eval_vector_set},
            {"vector-length",   &Interpreter::eval_vector_length},
            {"vector->list",    &Interpreter::eval_vector_to_list},

            // Хэш-таблицы
            {"make-hash-table",     &Interpreter::eval_make_hash_table},
            {"hash-table-set!",     &Interpreter::eval_hash_table_set},
            {"hash-table-ref",      &Interpreter::eval_hash_table_ref},
            {"hash-table?",         &Interpreter::eval_hash_table_p},
            {"hash-table-try-ref",  &Interpreter::eval_hash_table_try_ref},
            {"hash-table-length",   &Interpreter::eval_hash_table_length},
            {"hash-table->list",    &Interpreter::eval_hash_table_to_list},

            // Системные и ввод-вывод
            {"print",   &Interpreter::eval_print},
            {"pprint",  &Interpreter::eval_pprint},
            {"inspect", &Interpreter::eval_inspect},
            {"fmt",     &Interpreter::eval_fmt},
            {"cfmt",    &Interpreter::eval_cfmt},
            {"error",   &Interpreter::eval_error},

            // Logger
            {"log",   &Interpreter::eval_log},

            // Evaluation and parsing 
            {"read-str",  &Interpreter::eval_read_str},
            {"parse-str", &Interpreter::eval_parse_str},
            {"read-file", &Interpreter::eval_read_file},
            {"load",      &Interpreter::eval_load},

            // Files
            {"file-exists?", &Interpreter::eval_file_exists_p},
            {"get-path",     &Interpreter::eval_get_path},
            {"find-file",    &Interpreter::eval_find_file},

            // Reader
            {"set-macro-character", &Interpreter::eval_set_macro_character},
            {"remove-macro-character", &Interpreter::eval_remove_macro_character},
            {"get-macro-character", &Interpreter::eval_get_macro_character},
            {"read",                &Interpreter::eval_read},
            {"read-char",           &Interpreter::eval_read_char},
            {"peek-char",           &Interpreter::eval_peek_char},
            {"read-delimited-list", &Interpreter::eval_read_delimited_list},

            // Lexer tokens
            {"make-lextoken",  &Interpreter::eval_make_lextoken},
            {"lextoken-type",  &Interpreter::eval_lextoken_type},
            {"lextoken-value", &Interpreter::eval_lextoken_value},
            {"lextoken-info",  &Interpreter::eval_lextoken_info},

            // Macro system
            {"macroexpand", &Interpreter::eval_macroexpand},
            
            // System
            {"system", &Interpreter::eval_system},
            {"exit",   &Interpreter::eval_exit},
            {"get-environment-variable", &Interpreter::eval_get_env}, // было eval_get_environment_variable

            // Прочие
            {"gensym", &Interpreter::eval_gensym},
            {"eval",   &Interpreter::eval_eval},

            // Преобразования типов
            {"number->string", &Interpreter::eval_number_to_string},
            {"string->number", &Interpreter::eval_string_to_number},
            {"char->integer",  &Interpreter::eval_char_to_integer},
            {"integer->char",  &Interpreter::eval_integer_to_char},
            {"string->symbol", &Interpreter::eval_string_to_symbol},
            {"symbol->string", &Interpreter::eval_symbol_to_string},

            // Математические функции
            {"abs",     &Interpreter::eval_abs},
            {"max",     &Interpreter::eval_max},
            {"min",     &Interpreter::eval_min},
            {"expt",    &Interpreter::eval_expt},
            {"sqrt",    &Interpreter::eval_sqrt},
            {"ash",     &Interpreter::eval_ash},

            // Время
            {"time-seconds",        &Interpreter::eval_time_seconds},
            {"time-milliseconds",   &Interpreter::eval_time_milliseconds},
            {"time-microseconds",   &Interpreter::eval_time_microseconds},
            {"time-nanoseconds",    &Interpreter::eval_time_nanoseconds},
        } });
    // load the standard library
    if (load_libs) load_library();
}

void Interpreter::load_library() {
    auto cmd = "(load-file \"lib.sot\")";
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
    if (sym.as_symbol().name_ptr == true_object.as_symbol().name_ptr) {
        *dest = true_object;
        return true;
    }
    if (sym.as_symbol().name_ptr == false_object.as_symbol().name_ptr) {
        *dest = false_object;
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
    throw EvalException(o, err);
}

InternedSymbolPtr Interpreter::intern_ptr(const std::string& name) {
    return reader.get_symbol_table().intern(name.c_str());
}

// ==============================================
// REPL
// ==============================================

void Interpreter::execute_repl() {
    std::string input;

    //auto repl_env = std::make_shared<EnvironmentObject>();
    fmt::print(fg(fmt::color::gray), "{}i Scriptable Object-Oriented Toolkit {} Core [sha:{}]\n", SOOT_VERSION, SOOT_NAME, BUILT_SHA);
    fmt::print(fg(fmt::color::gray), "Type (exit) or 'quit' to leave\n");

    while (true) {
        std::cout << "sooti> ";
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
            stack_depth = 0;;
            Object result = eval_with_rewind(code, global_environment.as_env_ptr());
            // Print
            printf("%s\n", result.print().c_str());
        }
        catch (script::ExitException& e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());        
            exit(e.exit_code);
        }
        catch (script::EvalException& e) {
            if (e.already_printed)
                return;
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());        
        }
        catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());        
        }
    }

    std::cout << "Goodbye!\n";
}


Object Interpreter::eval_string(const std::string& expression, const std::string& filename)
{
    // read something from the user
    Object code = reader.read_from_string(expression, true, filename);
    stack_depth = 0;;
    // evaluate
    return eval_with_rewind(code, global_environment.as_env_ptr());
}


Object Interpreter::call_lambda(const Object& lambda,  const std::vector<Object>& args) {
    stack_depth = 0;;
    if (!lambda.is_lambda()) {
        throw std::runtime_error("call_lambda: object is not a lambda");
    }
    
    const auto& lam = lambda.as_lambda();
    
    // 1. Проверка аргументов
    size_t min_args = lam->args.unnamed.size();
    bool has_rest = !lam->args.rest.empty() || lam->args.varargs;
    
    if (args.size() < min_args || (!has_rest && args.size() > min_args)) {
        throw std::runtime_error(fmt::format(
            "call_lambda: wrong number of arguments (expected {}, got {})",
            has_rest ? fmt::format("at least {}", min_args) : std::to_string(min_args),
            args.size()
        ));
    }
    
    // 2. Создаем Arguments
    Arguments func_args;
    func_args.unnamed = args;
    
    // Если есть rest-аргумент (не varargs, а именно &rest)
    if (!lam->args.rest.empty() && args.size() > min_args) {
        // Помещаем лишние аргументы в rest
        for (size_t i = min_args; i < args.size(); ++i) {
            func_args.rest.push_back(args[i]);
        }
        // Обрезаем unnamed до нужного размера
        func_args.unnamed.resize(min_args);
    }
    
    // 3. Создаем окружение для выполнения
    // ВАЖНО: lam->parent_env - это уже shared_ptr<EnvironmentObject>
    auto lam_env_obj = EnvironmentObject::make_new("lambda-call", lam->parent_env);
    auto lam_env = lam_env_obj.as_env_ptr();
    
    // 4. Биндим аргументы
    Object dummy_form = Object::make_symbol(&reader.get_symbol_table(), "call-lambda");
    set_args_in_env(dummy_form, func_args, lam->args, lam_env);
    
    // 5. Выполняем тело
    return eval_list_return_last(lam->body, lam->body, lam_env);
}

// ==============================================
// Eval With Rewind (Main Recursion)
// ==============================================

Object Interpreter::eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env) {
    stack_depth++;
    try {
        auto result = eval(obj, env);
        stack_depth--; // Сбрасываем при успехе
        return result;
    }
    catch (const ExitException& e) {
        stack_depth--;
        throw; // Пробрасываем в самый верх (в main loop)
    }    
    catch (EvalException& e) {
        stack_depth--;        
        if (!disable_printing) {
            if (e.error_header_required) {
                // 1. Печатаем "Шапку"
                fmt::print(fg(fmt::color::indian_red), "\n─── ERROR ──────────────────────────────────\n");
                e.error_header_required = false;
            }

            if (e.detailed_error_required) {
                // 2. Пытаемся найти максимально точную локацию (LexToken или конкретная форма)
                std::string info = reader.get_db().get_info_for(e.form);
                
                // 3. Если не нашли, пробуем текущий уровень вызова (obj)
                if (info == "?") {
                    info = reader.get_db().get_info_for(obj);
                }
                
                // 4. Печатаем стрелочку, если нашли хоть какую-то локацию
                if (info != "?") {
                    fmt::print("{}", info);
                    e.detailed_error_required = false;
                    fmt::print(fg(fmt::color::indian_red), "Error: {}\n\n", e.message);
                }

                // 5. ПЕЧАТАЕМ САМУ ОШИБКУ (это критично!)
                e.already_printed = true; // Помечаем, что "мясо" ошибки уже на экране

            } else {
                if (obj.is_pair()) {
                    auto info_opt = reader.get_db().get_short_info_for(obj);
                    bool add_newline = stack_depth == 0;
                    // Печатаем "at ...", только если есть реальный файл и строка > 0
                    if (info_opt && info_opt->line_idx_to_display > 0) {
                        fmt::print(fg(fmt::color::dim_gray), "  [{:02d}] in {} at {}:{:d}", 
                                stack_depth + 1, obj.inspect_short(), info_opt->filename, info_opt->line_idx_to_display);
                    } else {
                        fmt::print(fg(fmt::color::dim_gray), "  [{:02d}] in {}", 
                                stack_depth + 1, obj.inspect_short());
                    }
                    if (add_newline) fmt::print(fg(fmt::color::dim_gray), "\n");
                }
            }
        }
        throw;
    }
    catch (const std::exception& e) {
        stack_depth--;
        throw; // Пробрасываем в самый верх (в main loop)
    }

}

// ==============================================
// Eval (Single Item)
// ==============================================

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
    case ObjectType::ARRAY:
    case ObjectType::STRING_HASH_TABLE:
        return obj;
    case ObjectType::LAMBDA:
        return eval_pair(obj, env);
    default:
        throw_eval_error(obj, "cannot evaluate this object");
    }
    return Object::make_empty_list();
}

// ==============================================
// Eval (Various Types)
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

// Запуск функции
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


Object Interpreter::eval_symbol(const Object& sym, const std::shared_ptr<EnvironmentObject>& env) {
    Object result;
    if (!try_symbol_lookup(sym, env, &result)) {
        throw EvalException(sym, "Unbound variable: " + std::string(std::string(sym.as_symbol().c_str())));
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
    Object result = true_object;

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

    return false_object;
}

Object Interpreter::eval_set(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    auto args = get_args(form, rest, make_varargs());
    vararg_check(form, args, {ObjectType::SYMBOL, {}}, {});
    auto to_define = args.unnamed.at(0);
    Object to_set = eval_with_rewind(args.unnamed.at(1), env);

    std::shared_ptr<EnvironmentObject> search_env = env;
    for (;;) {
        auto kv = search_env->vars.lookup(to_define.as_symbol());
        if (kv) {
        search_env->vars.set(to_define.as_symbol(), to_set);
        return to_set;
        }

        auto pe = search_env->parent_env;
        if (pe) {
        search_env = pe;
        } else {
            throw_eval_error(to_define, "symbol is not defined");
        }
    }
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
    bool parsing_keys = false; 
    Object current = rest;

    while (!current.is_empty_list()) {
        auto arg = current.as_pair()->car;

        // 1. Пытаемся понять, не встретили ли мы спец-символ (&key или &rest)
        std::string arg_name = "";
        bool is_sym = arg.is_symbol();
        if (is_sym) {
            arg_name = arg.as_symbol().name_ptr ? arg.as_symbol().name_ptr : "";
        }

        // 2. Логика переключения режимов
        if (is_sym && arg_name == "&rest") {
            current = current.as_pair()->cdr;
            if (!current.is_pair()) throw_eval_error(form, "rest arg must have a name");
            
            auto rest_name_obj = current.as_pair()->car;
            if (!rest_name_obj.is_symbol()) throw_eval_error(form, "rest name must be a symbol");
            
            spec.rest = rest_name_obj.as_symbol().name_ptr;
            if (!current.as_pair()->cdr.is_empty_list()) throw_eval_error(form, "rest must be the last argument");
            break; 
        }
        
        if (is_sym && arg_name == "&key") {
            parsing_keys = true;
            current = current.as_pair()->cdr;
            continue; // Идем к следующему элементу после "&key"
        }

        // 3. Обработка самого аргумента в зависимости от режима
        if (parsing_keys) {
            std::string key_arg_name;
            NamedArg na;

            if (arg.is_symbol()) {
                // Случай: &key b
                key_arg_name = arg.as_symbol().name_ptr;
            } 
            else if (arg.is_pair()) {
                // Случай: &key (b 1)
                auto key_iter = arg; // (b 1)
                auto kn = key_iter.as_pair()->car; // b
                if (!kn.is_symbol()) throw_eval_error(form, "key name must be a symbol");
                key_arg_name = kn.as_symbol().name_ptr;

                auto val_part = key_iter.as_pair()->cdr; // (1)
                if (val_part.is_pair()) {
                    na.has_default = true;
                    na.default_value = val_part.as_pair()->car; // 1
                }
            } 
            else {
                throw_eval_error(form, "invalid key argument");
            }

            if (spec.named.count(key_arg_name)) {
                throw_eval_error(form, fmt::format("key argument {} multiply defined", key_arg_name));
            }
            spec.named[key_arg_name] = na;
        } 
        else {
            // Обычный позиционный аргумент
            if (!is_sym) throw_eval_error(form, "positional args must be symbols");
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

Object Interpreter::eval_fmt(const Object& form,
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

// Вспомогательная функция для сопоставления символа/строки с цветом fmt
fmt::terminal_color string_to_color(const std::string& name) {
    static const std::unordered_map<std::string, fmt::terminal_color> colors = {
        {"red", fmt::terminal_color::red},
        {"green", fmt::terminal_color::green},
        {"yellow", fmt::terminal_color::yellow},
        {"blue", fmt::terminal_color::blue},
        {"magenta", fmt::terminal_color::magenta},
        {"cyan", fmt::terminal_color::cyan},
        {"white", fmt::terminal_color::white},
        {"gray", fmt::terminal_color::bright_black}
    };
    auto it = colors.find(name);
    return (it != colors.end()) ? it->second : fmt::terminal_color::white;
}

Object Interpreter::eval_cfmt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "cfmt requires at least destination and format-string");
    }

    auto dest = args.unnamed.at(0);
    auto format_str = args.unnamed.at(1);
    if (!format_str.is_string()) {
        throw_eval_error(form, "cfmt: format string must be a string");
    }

    // Обработка цвета из ключевых аргументов (например, :color "red")
    fmt::terminal_color text_color = fmt::terminal_color::white;
    auto it_color = args.named.find("color");
    if (it_color != args.named.end()) {
        if (it_color->second.is_string()) {
            text_color = string_to_color(it_color->second.as_string()->data);
        } else if (it_color->second.is_symbol()) {
            text_color = string_to_color(it_color->second.as_symbol().c_str());
        }
    }

    // Собираем аргументы (как в твоем fmt)
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 2; i < args.unnamed.size(); i++) {
        const auto& arg = args.unnamed.at(i);
        if (arg.is_string()) {
            arg_store.push_back(arg.as_string()->data);
        } else {
            arg_store.push_back(arg.print());
        }
    }

    // Форматируем строку
    auto formatted = fmt::vformat(format_str.as_string()->data, arg_store);

    // Если dest не ложь, выводим в консоль с цветом
    if (truthy(dest)) {
        fmt::print(fg(text_color), "{}\n", formatted);
    }

    return Object::make_string(formatted);
}

/**
 * (error <message-string> [object])
 * Сигнализирует об ошибке. Если передан второй аргумент, 
 * система диагностики попытается подсветить его координаты.
 */
Object Interpreter::eval_error(const Object& form,
    Arguments& args,
    const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    
    // Проверяем аргументы: 
    // 1-й обязательно STRING. 
    // 2-й опционально ЛЮБОЙ (поэтому пустые скобки {} во втором векторе)
    vararg_check(form, args, { ObjectType::STRING, {} }, {});

    std::string message = args.unnamed.at(0).as_string()->data;
    
    // Если передан второй аргумент, используем его как "место преступления"
    // Иначе используем 'form' (всю строку вызова (error ...))
    Object context_form = (args.unnamed.size() > 1) ? args.unnamed.at(1) : form;

    // Вызываем стандартный механизм исключений с учетом контекста
    throw_eval_error(context_form, message);

    return Object::make_empty_list(); // Сюда мы никогда не дойдем
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

Object Interpreter::eval_type_of(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, {});

    return reader.get_symbol_table().object_type_to_symbol(args.unnamed[0].type);
}

Object Interpreter::eval_type_p(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {}, ObjectType::SYMBOL }, {});

    auto type_name = args.unnamed[1].as_symbol().name_ptr;
    auto kv = string_to_type.find(type_name);
    if (kv == string_to_type.end()) {
        throw_eval_error(form, fmt::format("invalid type name: {}", type_name));
    }

    return make_bool(args.unnamed[0].type == kv->second);
}

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

Object Interpreter::eval_reader_p(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_reader());
}

Object Interpreter::eval_lextoken_p(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_lextoken());
}

// ==============================================
// Apply 
// ==============================================

Object Interpreter::eval_apply(const Object& obj, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.size() < 2) {
        throw_eval_error(obj, "apply: expected function and list of arguments");
    }

    Object head = args.unnamed[0]; // Это может быть символ 'vector или объект LAMBDA
    Object rest = args.unnamed[1]; // Это список (1 2 3)

    // 1. Если это символ, пытаемся найти его в Builtins (как делает твой eval_pair)
    if (head.type == ObjectType::SYMBOL) {
        const auto& head_sym = head.as_symbol();
        
        // Проверяем встроенные функции (например, vector)
        const auto& kv_b = builtin_forms.find((void*)head_sym.name_ptr);
        if (kv_b != builtin_forms.end()) {
            // Создаем Arguments напрямую из списка Lisp
            Arguments builtin_args;
            for (Object it = rest; it.is_pair(); it = it.as_pair()->cdr) {
                builtin_args.unnamed.push_back(it.as_pair()->car);
            }
            // Вызываем встроенную функцию напрямую
            return ((*this).*(kv_b->second))(obj, builtin_args, env);
        }
        
        // Если не нашли в билтинах, вычисляем символ, чтобы получить Лямбду
        head = eval_with_rewind(head, env);
    }

    // 2. Если это Лямбда (или результат вычисления символа стал Лямбдой)
    if (head.type == ObjectType::LAMBDA) {
        const auto& lam = head.as_lambda();
        
        // Перекладываем список rest в структуру Arguments для set_args_in_env
        Arguments lam_args;
        for (Object it = rest; it.is_pair(); it = it.as_pair()->cdr) {
            lam_args.unnamed.push_back(it.as_pair()->car);
        }

        // Повторяем логику твоего eval_pair для вызова лямбды:
        auto lam_env_obj = EnvironmentObject::make_new();
        auto lam_env = lam_env_obj.as_env_ptr();
        lam_env->parent_env = lam->parent_env;
        
        // Используем твою функцию привязки аргументов
        set_args_in_env(obj, lam_args, lam->args, lam_env);
        
        return eval_list_return_last(lam->body, lam->body, lam_env);
    }

    throw_eval_error(obj, "apply: head didn't evaluate to a callable function");
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

Object Interpreter::eval_vector_to_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::ARRAY }, {});

    auto array = args.unnamed[0].as_array();
    
    // Рекурсивная функция для построения списка
    std::function<Object(int)> build_list = [&](int index) -> Object {
        if (index >= array->size()) {
            return Object::make_empty_list();
        }
        return Object::make_pair(array->at(index), build_list(index + 1));
    };
    
    return build_list(0);
}

// ==============================================
// Хэш - таблицы с проверками
// ==============================================


Object Interpreter::eval_make_hash_table(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    Object table = Object::make_hash_table();
    auto table_ptr = table.as_hash_table();

    // 1. Обрабатываем именованные аргументы (те самые :a 1)
    for (auto const& [key, val] : args.named) {
        // Мы можем сохранить ключ как есть, или добавить обратно ":", 
        // чтобы в таблице ключи выглядели одинаково
        std::string key_with_colon = ":" + key; 
        table_ptr->data[key_with_colon] = val;
    }

    // 2. Обрабатываем неименованные аргументы (если пришло "a" 1 или 'a 1)
    const std::vector<Object>& elements = args.unnamed;
    if (elements.size() % 2 != 0) {
        throw_eval_error(form, "Positional arguments to make-hash-table must be in pairs");
    }

    for (size_t i = 0; i < elements.size(); i += 2) {
        std::string key_str;
        const Object& key_obj = elements[i];

        if (key_obj.is_symbol()) key_str = key_obj.as_symbol().c_str();
        else if (key_obj.is_string()) key_str = key_obj.as_string()->c_str();
        else key_str = key_obj.print();

        table_ptr->data[key_str] = elements[i + 1];
    }

    return table;
}

Object Interpreter::eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING_HASH_TABLE, {}, {} }, {}); // Таблица, ключ, значение

    auto ht = args.unnamed[0].as_hash_table();
    const char* str = nullptr;
    if (args.unnamed.at(1).is_symbol()) {
        str = args.unnamed.at(1).as_symbol().name_ptr;
    } else if (args.unnamed.at(1).is_string()) {
        str = args.unnamed.at(1).as_string()->data.c_str();
    } else {
        throw_eval_error(form, "Hash table must use symbol or string as the key.");
    }

    args.unnamed.at(0).as_hash_table()->data[str] = args.unnamed.at(2);
    return Object::make_empty_list();
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
        return Object::make_pair(false_object, Object::make_empty_list());
    }
    else {
        return Object::make_pair(true_object, it->second);
    }
}

Object Interpreter::eval_hash_table_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент

    auto ht = args.unnamed[0].as_hash_table();
   
    return Object::make_integer(ht->data.size());
}

Object Interpreter::eval_hash_table_to_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {});

    auto ht = args.unnamed[0].as_hash_table();
    Object result = Object::make_empty_list();
    
    // Итерируемся по unordered_map
    for (const auto& [key, value] : ht->data) {
        // Создаем пару (ключ значение)
        Object pair = Object::make_pair(
            Object::make_string(key), 
            value
        );
        // Добавляем в начало списка
        result = Object::make_pair(pair, result);
    }
    
    return result;
}

Object Interpreter::eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { {} }, {}); // Один аргумент
    return make_bool(args.unnamed[0].is_hash_table());
}

// ==============================================
// Системные функции с проверками
// ==============================================

// Читает весь файл как текст.
Object Interpreter::eval_read_str(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    return Object::make_string(file_util::read_text(filename));
}

// Превращает строку в список команд: (top-level ... ). Удобно для eval.
Object Interpreter::eval_parse_str(const Object & form, Arguments & args, const std::shared_ptr<EnvironmentObject>&env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка
    return reader.read_from_string(args.unnamed[0].as_string()->data, true, "read string");
}

// Читает весь файл как данные, обернутые в top-level.
Object Interpreter::eval_read_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    std::string content = file_util::read_text(filename);
    return reader.read_from_string(content, true, filename);
}

// Читает и исполняет файл. (Обычно исполняет объекты по одному, top-level не нужен).
Object Interpreter::eval_load(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (имя файла)
    Object code = reader.read_from_file({ args.unnamed[0].as_string()->data }, true, true);
    return eval_with_rewind(code, env);
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
        return Object::make_empty_list();
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
    int code = 0;
    if (!args.unnamed.empty() && args.unnamed[0].is_integer()) {
        code = args.unnamed[0].as_integer();
    }
    throw ExitException(code); // Просто бросаем, не заботясь о возврате
}

Object Interpreter::eval_get_path(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::SYMBOL }, {}); // Один символ
    std::string sym = args.unnamed[0].as_symbol().name_ptr ? args.unnamed[0].as_symbol().name_ptr : "";
    file_util::PathType select;
    if (sym == "cwd")             select = file_util::PathType::CWD;
    else  if (sym == "exe")       select = file_util::PathType::EXE;
    else  if (sym == "home")      select = file_util::PathType::HOME;    
    else  if (sym == "config")    select = file_util::PathType::CONFIG;
    else  if (sym == "cache")     select = file_util::PathType::CACHE;
    else  if (sym == "share")     select = file_util::PathType::SHARE;
    else  if (sym == "project")   select = file_util::PathType::PROJECT;
    else {
        throw_eval_error(form, "get_path requires a symbol: cwd, exe, home, config, cache, share, project");
    }
    return Object::make_string(file_util::get_path(select).string());
}

Object Interpreter::eval_find_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    vararg_check(form, args, { ObjectType::STRING }, {}); // Одна строка (команда)

    std::string path = args.unnamed[0].as_string()->data;
    auto found = file_util::find_config_file(path);
    return found.empty() ? Object::make_empty_list() : Object::make_string(found.string());
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
        return Object::make_empty_list();
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

// ==============================================
// Функции времени
// ==============================================

// time-seconds: возвращает количество секунд с эпохи Unix
Object Interpreter::eval_time_seconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // Без аргументов
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    return Object::make_integer(static_cast<int64_t>(seconds));
}

// time-milliseconds: возвращает количество миллисекунд с эпохи Unix
Object Interpreter::eval_time_milliseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // Без аргументов
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    return Object::make_integer(static_cast<int64_t>(milliseconds));
}

// time-microseconds: если нужна еще большая точность
Object Interpreter::eval_time_microseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {});
    
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    
    return Object::make_integer(static_cast<int64_t>(microseconds));
}

// time-nanoseconds: максимальная точность
Object Interpreter::eval_time_nanoseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {});
    
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    
    return Object::make_integer(static_cast<int64_t>(nanoseconds));
}

// ==============================================
// Macro Character
// ==============================================

Object Interpreter::eval_set_macro_character(const Object& form, Arguments& args,
                                           const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    
    // Проверяем аргументы
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "set-reader-macro requires 2 arguments");
    }
    
    // Получаем shortcut (первый аргумент)
    std::string shortcut;
    
    if (args.unnamed[0].is_string()) {
        shortcut = args.unnamed[0].as_string()->data;
    }
    else if (args.unnamed[0].is_symbol()) {
        const char* sym_name = args.unnamed[0].as_symbol().name_ptr;
        shortcut = sym_name ? sym_name : "";
    }
    else if (args.unnamed[0].is_char()) {
        // Символ конвертируем в строку из одного символа
        shortcut = std::string(1, args.unnamed[0].as_char());
    }
    else {
        throw_eval_error(form, "set-reader-macro: first argument must be string, symbol, or character");
    }
    
    // Получаем replacement (второй аргумент)
    std::string replacement;
    auto object = args.unnamed[1];
    if (object.is_string()) {
        replacement = object.as_string()->data;
        reader.add_reader_macro(shortcut, replacement, true);
    }
    else if (object.is_symbol()) {
        const char* sym_name = object.as_symbol().name_ptr;
        replacement = sym_name ? sym_name : "";
        reader.add_reader_macro(shortcut, replacement, true);
    }
    else if (object.is_lambda()) {
        reader.add_reader_macro(shortcut, object, false);
    }
    else {
        throw_eval_error(form, "set-reader-macro: second argument must be string or symbol");
    }

    return Object::make_empty_list();
}

Object Interpreter::eval_remove_macro_character(const Object& form, Arguments& args,
                                            const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    
    if (args.unnamed.empty()) {
        throw_eval_error(form, "remove-reader-macro requires at least 1 argument");
    }
    
    std::string shortcut;
    
    // Поддерживаем те же типы что и set-reader-macro
    if (args.unnamed[0].is_string()) {
        shortcut = args.unnamed[0].as_string()->data;
    }
    else if (args.unnamed[0].is_symbol()) {
        const char* sym_name = args.unnamed[0].as_symbol().name_ptr;
        shortcut = sym_name ? sym_name : "";
    }
    else if (args.unnamed[0].is_char()) {
        shortcut = std::string(1, args.unnamed[0].as_char());
    }
    else {
        throw_eval_error(form, "remove-reader-macro: argument must be string, symbol, or character");
    }
    reader.remove_reader_macro(shortcut); 
    
    
    return Object::make_empty_list();
}

Object Interpreter::eval_get_macro_character(const Object& form, Arguments& args,
                                            const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    
    if (args.unnamed.empty()) {
        throw_eval_error(form, "remove-reader-macro requires at least 1 argument");
    }
    
    std::string shortcut;
    
    // Поддерживаем те же типы что и set-reader-macro
    if (args.unnamed[0].is_string()) {
        shortcut = args.unnamed[0].as_string()->data;
    }
    else if (args.unnamed[0].is_symbol()) {
        const char* sym_name = args.unnamed[0].as_symbol().name_ptr;
        shortcut = sym_name ? sym_name : "";
    }
    else if (args.unnamed[0].is_char()) {
        shortcut = std::string(1, args.unnamed[0].as_char());
    }
    else {
        throw_eval_error(form,
            "get-reader-macro: argument must be string, symbol, or character");
    }
    auto macro = reader.find_reader_macro(shortcut); 
    if (macro == nullptr) 
        return Object::make_empty_list();
    else if (!macro->lambda.is_empty_list())
        return macro->lambda;
    else
        return Object::make_string(macro->replacement);
}


Object Interpreter::eval_read(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.empty() || !args.unnamed[0].is_reader()) {
        throw_eval_error(form, "read: requires a reader object");
    }

    ReaderObject* reader_obj = args.unnamed[0].as_reader();
    if (!reader_obj->ts) {
        throw_eval_error(form, "read: stream is null");
    }

    // Вызываем чтение одного объекта (чистого, без top-level)
    // Метод read_from_stream должен выполнять get_next_token + read_object + process_macros
    return reader.read_from_stream(*(reader_obj->ts)); 
}

Object Interpreter::eval_read_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.empty() || !args.unnamed[0].is_reader()) {
        throw_eval_error(form, "read-char: requires a reader object");
    }

    ReaderObject* reader_obj = args.unnamed[0].as_reader();
    auto ts = reader_obj->ts;

    if (!ts || !ts->text_remains()) {
        return Object::make_empty_list(); // EOF
    }

    // Используем метод твоего TextStream, который двигает seek и считает строки
    char c = ts->read();
    return Object::make_char(c);
}

Object Interpreter::eval_peek_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    if (args.unnamed.empty() || !args.unnamed[0].is_reader()) {
        throw_eval_error(form, "peek-char: requires a reader object");
    }

    ReaderObject* reader_obj = args.unnamed[0].as_reader();
    auto ts = reader_obj->ts;

    if (!ts || !ts->text_remains()) {
        return Object::make_empty_list(); // EOF
    }

    // Используем твой ts->peek(), который просто берет char по текущему индексу
    char c = ts->peek();
    return Object::make_char(c);
}

Object Interpreter::eval_read_delimited_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // 1. Проверяем минимальное количество аргументов (Reader обязателен)
    if (args.unnamed.size() != 2) {
        throw_eval_error(form, "read-delimited-list requires at least 1 argument (reader)");
    }

        // 3. Определяем терминатор (по умолчанию ")")
    std::string terminator = ")";
    Object term_arg = args.unnamed[0];
    if (term_arg.is_char()) {
        terminator = std::string(1, term_arg.as_char());
    } else if (term_arg.is_string()) {
        terminator = term_arg.as_string()->data;
    } else {
        throw_eval_error(form, "read-delimited-list: 1st argument must be a char or string");
    }

    // 2. Извлекаем ReaderObject
    if (!args.unnamed[1].is_reader()) {
        throw_eval_error(form, "read-delimited-list: 2d argument must be a reader object");
    }
    ReaderObject* reader_obj = args.unnamed[1].as_reader();

    // 4. Вызываем РЕАЛЬНЫЙ ридер
    // Предполагается, что у твоего Interpreter есть доступ к экземпляру Reader (напр. m_reader)
    // Мы используем разыменованный TextStream из ReaderObject
    if (!reader_obj->ts) {
        throw_eval_error(form, "read-delimited-list: reader stream is null");
    }

    // ВАЖНО: вызываем метод у объекта Reader, а не у обертки ReaderObject
    return reader.read_list(*(reader_obj->ts), false, terminator);
}


std::string Interpreter::get_all_symbols_matching(const std::string& prefix) {
    std::set<std::string> matches;

    // 1. Встроенные спец-формы (define, if, lambda...)
    for (const auto& pair : special_forms) {
        const char* name = (const char*)pair.first;
        if (std::string_view(name).starts_with(prefix)) {
            matches.insert(name);
        }
    }

    // 2. Встроенные функции (+, -, print...)
    for (const auto& [ptr, fn] : builtin_forms) {
        const char* name = (const char*)ptr;
        if (std::string_view(name).starts_with(prefix)) {
            matches.insert(name);
        }
    }

    // 3. Лямбда для глубокого обхода EnvironmentObject
    auto collect_from_env = [&](const Object& env_obj) {
        if (!env_obj.is_env()) return;
        
        // Явно указываем shared_ptr, чтобы типы совпали с parent_env
        std::shared_ptr<EnvironmentObject> current = env_obj.as_env_ptr(); 
        
        while (current) {
            const auto& entries = current->vars.get_all_entries();
            
            for (const auto& e : entries) {
                if (e.key) { 
                    std::string_view name(e.key);
                    if (name.starts_with(prefix)) {
                        matches.insert(std::string(name));
                    }
                }
            }
            // Теперь здесь не будет ошибки, так как оба типа shared_ptr
            current = current->parent_env;
        }
    };

    // 3. Собираем из обоих окружений
    collect_from_env(global_environment);
    collect_from_env(comp_env);

    // 4. Склеиваем в строку для отправки по сети
    std::string result;
    for (const auto& s : matches) {
        if (!result.empty()) result += " ";
        result += s;
    }
    return result;
}

// ==============================================
// Lex Tokens
// ==============================================

Object Interpreter::eval_make_lextoken(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // Добавим проверку: мы ожидаем либо 2 позиционных, либо именованные
    // Для простоты сейчас сделаем поддержку :type и :value как в CL
    vararg_check(form, args, {}, {
        {"type",  {false, {}}}, // Опциональные, так как можем задать дефолты
        {"value", {false, {}}}
    });

    // 1. Извлекаем параметры. Если именованных нет, пробуем взять первые два позиционных
    Object type = Object::make_symbol(&reader.get_symbol_table(), "unknown");
    if (args.named.count("type")) type = args.named.at("type");
    else if (args.unnamed.size() >= 1) type = args.unnamed[0];

    Object value = Object::make_empty_list();
    if (args.named.count("value")) value = args.named.at("value");
    else if (args.unnamed.size() >= 2) value = args.unnamed[1];

    // 2. Метаданные (работает отлично, судя по твоему логу ("repl" 0 15))
    auto info_opt = reader.get_db().get_text_ref(form);
    auto info = info_opt.value_or(TextRef::empty()); 

    return Object::make_lextoken(type, value, info);
}

Object Interpreter::eval_lextoken_type(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // Проверяем: ровно 1 аргумент, тип должен быть LEXTOKEN
    vararg_check(form, args, {{ ObjectType::LEXTOKEN }}, {});

    // Извлекаем токен (он гарантированно LEXTOKEN после проверки)
    auto token = std::static_pointer_cast<LextokenObject>(args.unnamed[0].heap_obj);
    
    return token->type;
}
Object Interpreter::eval_lextoken_value(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // Проверяем: ровно 1 аргумент типа LEXTOKEN
    vararg_check(form, args, {{ ObjectType::LEXTOKEN }}, {});

    auto token = std::static_pointer_cast<LextokenObject>(args.unnamed[0].heap_obj);
    
    return token->value;
}
/**
 * (token-info <lextoken>) -> (filename line offset)
 * * Возвращает метаданные о расположении токена в исходном коде.
 * Полезно для генерации собственных сообщений об ошибках в ассемблере.
 */
Object Interpreter::eval_lextoken_info(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // Проверка: ожидаем ровно один аргумент типа LEXTOKEN
    vararg_check(form, args, {{ ObjectType::LEXTOKEN }}, {});

    // Безопасное приведение к LextokenObject
    auto token = std::static_pointer_cast<LextokenObject>(args.unnamed[0].heap_obj);
    
    auto source_location = token->location.frag.get();
    auto offset = token->location.offset;

    // 1. Сначала получаем индекс строки (это O(log N) за счет бинарного поиска в build_offsets)
    int line_idx = source_location->get_line_idx(offset);

    // 2. Получаем абсолютный offset начала этой строки (это O(1) — просто доступ к вектору)
    int line_start_offset = source_location->get_offset_of_line(line_idx);

    // 3. Вычисляем позицию внутри строки (колонку)
    int pos_in_line = offset - line_start_offset;

    return Object::make_list({
        Object::make_string(source_location->get_description()),
        Object::make_integer(line_idx + 1), // Обычно пользователю выводят 1-based индекс
        Object::make_integer(pos_in_line)    // Смещение от начала строки
    });
}

// ==============================================
// Ьфскщучзфтв
// ==============================================

Object Interpreter::eval_macroexpand(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    // Проверяем наличие одного аргумента (формы для раскрытия)
    if (args.unnamed.empty()) {
        throw_eval_error(form, "macroexpand requires an expression to expand");
    }
    
    Object code = args.unnamed[0];

    // Макрос — это всегда список вида (имя-макроса ...)
    if (!code.is_pair()) {
        return code;
    }

    const Object& head = code.as_pair()->car;
    const Object& rest = code.as_pair()->cdr;

    Object macro_obj;
    // Используем твой механизм поиска символа
    if (head.is_symbol() && try_symbol_lookup(head, env, &macro_obj) && macro_obj.is_macro()) {
        const auto& macro = macro_obj.as_macro();
        
        // 1. Парсим аргументы согласно спецификации макроса
        Arguments mac_args = get_args(code, rest, macro->args);

        // 2. Создаем временное окружение для раскрытия (как в твоем eval_pair)
        auto mac_env_obj = EnvironmentObject::make_new();
        auto mac_env = mac_env_obj.as_env_ptr();
        mac_env->parent_env = env; 
        
        set_args_in_env(code, mac_args, macro->args, mac_env);

        // 3. Раскрываем тело макроса
        // Используем eval_list_return_last, чтобы получить результат раскрытия
        // ВАЖНО: Мы НЕ вызываем eval_with_rewind для результата!
        return eval_list_return_last(macro->body, macro->body, mac_env);
    }

    // Если это не вызов макроса, возвращаем объект как есть
    return code;
}

Object Interpreter::eval_log(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    // Минимальный вызов: (log 'level "format" ...)
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "log: (log 'level \"format-string\" ...)");
    }

    // 1. Определяем уровень лога
    std::string level_name = args.unnamed.at(0).print();
    lg::level log_level = lg::level::info; // по умолчанию

    if (level_name == "trace")      log_level = lg::level::trace;
    else if (level_name == "debug") log_level = lg::level::debug;
    else if (level_name == "info")  log_level = lg::level::info;
    else if (level_name == "warn")  log_level = lg::level::warn;
    else if (level_name == "error") log_level = lg::level::error;
    else if (level_name == "die")   log_level = lg::level::die;

    // 2. Получаем строку формата
    auto format_obj = args.unnamed.at(1);
    if (!format_obj.is_string()) {
        throw_eval_error(form, "log: format must be a string");
    }
    std::string format_str = format_obj.as_string()->data;

    // 3. Собираем аргументы через fmt (KISS)
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 2; i < args.unnamed.size(); i++) {
        const auto& arg = args.unnamed.at(i);
        if (arg.is_string()) arg_store.push_back(arg.as_string()->data);
        else arg_store.push_back(arg.print());
    }

    // 4. Форматируем и отправляем в твой lg::log
    std::string formatted = fmt::vformat(format_str, arg_store);
    
    // Используем внутренний логгер
    lg::log(log_level, "{}", formatted);

    return Object::make_string(formatted);
}

} // namespace script
