#include "common/sooti/Interpreter.hpp"
#include "common/sooti/Errors.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/PrettyPrinter.hpp"
#include "common/sooti/Printer.hpp"
#include "common/sooti/static_buffer/Export.hpp"
#include <iostream>

#include "common/type_system/Defenum.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/type_system/TypeSystem.hpp"

#include "common/util/FileUtil.hpp"
#include "common/util/Log.hpp"
#include "common/util/StringUtil.hpp"

#include "fmt/args.h"
#include "fmt/base.h"
#include "fmt/color.h"
#include "fmt/format.h"

#include "common/CommonTypes.hpp"
#include "common/versions/revision.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

namespace script {

Interpreter::Interpreter(const std::string &username, bool load_libs)
    : m_setter_map(), m_reader(this), m_top_frame(nullptr), m_symbol_table() {
    script::Object::set_symbol_table(&m_symbol_table);

    // Инициализируем boolean объекты как символы
    m_sym_null = Object::make_null();
    m_sym_true = m_symbol_table.core.sym_true;
    m_obj_false = m_symbol_table.core.sym_false;
    m_kw_undefined = m_symbol_table.core.kw_undefined;
    m_symbol_true = m_sym_true.as_symbol().name_ptr;
    m_symbol_false = m_obj_false.as_symbol().name_ptr;
    m_symbol_undefined = m_kw_undefined.as_symbol().name_ptr;

    m_undefined = Object::make_none();
    // Создаем глобальное окружение
    m_global_environment = EnvironmentObject::make_new("global");

    // create the environment which is be visible from GOAL
    m_comp_env = EnvironmentObject::make_new("goal");

    define_var_in_env(m_global_environment, m_sym_null, "null");
    define_var_in_env(m_comp_env, m_sym_null, "null");

    define_var_in_env(m_global_environment, m_global_environment, "*global-env*");
    define_var_in_env(m_global_environment, m_comp_env, "*comp-env*");
    define_var_in_env(m_comp_env, m_comp_env, "*comp-env*");
    define_var_in_env(m_comp_env, m_global_environment, "*global-env*");

    auto user = make_symbol(username.c_str());
    define_var_in_env(m_global_environment, user, "*user*");

    // Инициализация string_to_type для type?
    m_string_to_type = {
        {object_type_to_string(ObjectType::EMPTY_LIST), ObjectType::EMPTY_LIST},
        {object_type_to_string(ObjectType::INTEGER), ObjectType::INTEGER},
        {object_type_to_string(ObjectType::FLOAT), ObjectType::FLOAT},
        {object_type_to_string(ObjectType::CHAR), ObjectType::CHAR},
        {object_type_to_string(ObjectType::SYMBOL), ObjectType::SYMBOL},
        {object_type_to_string(ObjectType::STRING), ObjectType::STRING},
        {object_type_to_string(ObjectType::PAIR), ObjectType::PAIR},
        {object_type_to_string(ObjectType::ARRAY), ObjectType::ARRAY},
        {object_type_to_string(ObjectType::LAMBDA), ObjectType::LAMBDA},
        {object_type_to_string(ObjectType::MACRO), ObjectType::MACRO},
        {object_type_to_string(ObjectType::ENVIRONMENT), ObjectType::ENVIRONMENT},
        {object_type_to_string(ObjectType::READER), ObjectType::READER},
        {object_type_to_string(ObjectType::POINTER), ObjectType::POINTER},
        {object_type_to_string(ObjectType::STATIC_BUFFER), ObjectType::STATIC_BUFFER},
        {object_type_to_string(ObjectType::STATIC_WRITER), ObjectType::STATIC_WRITER},
        {object_type_to_string(ObjectType::NATIVE_REF), ObjectType::NATIVE_REF},
        {object_type_to_string(ObjectType::NONE), ObjectType::NONE},
    };

    ArgumentSpec args_with_keys(true, true);

    // === СПЕЦИАЛЬНЫЕ ФОРМЫ (не вычисляют аргументы) ===
    init_special_forms({
        {"define", &Interpreter::eval_define_special, nullptr},
        {"quote", &Interpreter::eval_quote_special, nullptr},
        {"set!", &Interpreter::eval_set_special, nullptr},
        {"let", &Interpreter::eval_let_special, nullptr},
        {"let*", &Interpreter::eval_let_star_special, nullptr},
        {"lambda", &Interpreter::eval_lambda_special, nullptr},
        {"cond", &Interpreter::eval_cond_special, nullptr},
        {"begin", &Interpreter::eval_begin_special, nullptr},
        {"or", &Interpreter::eval_or_special, nullptr},
        {"and", &Interpreter::eval_and_special, nullptr},
        {"if", &Interpreter::eval_if_special, nullptr},
        {"macro", &Interpreter::eval_macro_special, nullptr},
        {"quasiquote", &Interpreter::eval_quasiquote_special, nullptr},
        {"while", &Interpreter::eval_while_special, nullptr},
        {"top-level", &Interpreter::eval_begin_special, nullptr}, // top level evaluation
        {"->", &Interpreter::eval_deref_special, nullptr},
        {"the", &Interpreter::eval_the_special, nullptr},
        {"the-as", &Interpreter::eval_the_as_special, nullptr},
        {"offset-of", &Interpreter::eval_offset_of_special, nullptr},
        {"size-of", &Interpreter::eval_size_of_special, nullptr},
        {"method-id-of", &Interpreter::eval_method_id_of_special, nullptr},
        {"method-of", &Interpreter::eval_method_of_special, nullptr},
        {"static-new", &Interpreter::eval_static_new_special, nullptr},
    });

    // === ВСТРОЕННЫЕ ФУНКЦИИ (вычисляют аргументы) ===
    init_builtin_forms({
        // Математически
        {"+", &Interpreter::eval_plus, nullptr},
        {"-", &Interpreter::eval_minus, nullptr},
        {"*", &Interpreter::eval_times, nullptr},
        {"/", &Interpreter::eval_divide, nullptr},
        {"=", &Interpreter::eval_numequals, nullptr}, // было eval_equals
        {"<", &Interpreter::eval_lt, nullptr},
        {">", &Interpreter::eval_gt, nullptr},
        {"<=", &Interpreter::eval_leq, nullptr},
        {">=", &Interpreter::eval_geq, nullptr},

        // Списки и пары
        {"cons", &Interpreter::eval_cons, nullptr}, // было eval_cons_builtin
        {"car", &Interpreter::eval_car, nullptr},   // было eval_car_builtin
        {"cdr", &Interpreter::eval_cdr, nullptr},   // было eval_cdr_builtin
        {"set-car!", &Interpreter::eval_set_car, nullptr},
        {"set-cdr!", &Interpreter::eval_set_cdr, nullptr},
        {"list", &Interpreter::eval_list_func, nullptr},
        {"length", &Interpreter::eval_length, nullptr},
        {"append", &Interpreter::eval_append, nullptr},
        {"apply", &Interpreter::eval_apply, nullptr},

        {"bound?", &Interpreter::eval_bound_p, nullptr},

        // Работа с типом
        {"type-of", &Interpreter::eval_type_of, nullptr},
        {"type?", &Interpreter::eval_type_p, nullptr},

        // Предикаты типов
        {"null?", &Interpreter::eval_null_p, nullptr},
        {"pair?", &Interpreter::eval_pair_p, nullptr},
        {"symbol?", &Interpreter::eval_symbol_p, nullptr},
        {"number?", &Interpreter::eval_number_p, nullptr},
        {"integer?", &Interpreter::eval_integer_p, nullptr},
        {"float?", &Interpreter::eval_float_p, nullptr},
        {"string?", &Interpreter::eval_string_p, nullptr},
        {"char?", &Interpreter::eval_char_p, nullptr},
        {"vector?", &Interpreter::eval_vector_p, nullptr},
        {"hash-table?", &Interpreter::eval_hash_table_p, nullptr},
        {"procedure?", &Interpreter::eval_procedure_p, nullptr},
        {"boolean?", &Interpreter::eval_boolean_p, nullptr},
        {"reader?", &Interpreter::eval_reader_p, nullptr},
        {"cell?", &Interpreter::eval_cell_p, nullptr},
        {"primitive?", &Interpreter::eval_primitive_p, nullptr},
        {"special-form?", &Interpreter::eval_special_form_p, nullptr},

        // Сравнение
        {"eq?", &Interpreter::eval_equals, nullptr}, // было eval_eq

        // Строки
        {"string-append", &Interpreter::eval_string_append, nullptr},
        {"string-length", &Interpreter::eval_string_length, nullptr},
        {"string-ref", &Interpreter::eval_string_ref, nullptr},
        {"string-replace", &Interpreter::eval_string_replace, nullptr}, // было eval_substring
        {"string-substr", &Interpreter::eval_string_substr, nullptr},   // было eval_substring
        {"string-starts-with?", &Interpreter::eval_string_starts_with, nullptr},
        {"string-ends-with?", &Interpreter::eval_string_ends_with, nullptr},
        {"string-contains?", &Interpreter::eval_string_containsp, nullptr},
        {"string-split", &Interpreter::eval_string_split, nullptr},

        // Векторы
        {"vector", &Interpreter::eval_vector, nullptr},
        {"vector-ref", &Interpreter::eval_vector_ref, nullptr},
        {"vector-set!", &Interpreter::eval_vector_set, nullptr},
        {"vector-length", &Interpreter::eval_vector_length, nullptr},
        {"vector->list", &Interpreter::eval_vector_to_list, nullptr},

        // Хэш-таблицы
        {"make-hash-table", &Interpreter::eval_make_hash_table, &args_with_keys},
        {"hash-table-set!", &Interpreter::eval_hash_table_set, nullptr},
        {"hash-table-ref", &Interpreter::eval_hash_table_ref, nullptr},
        {"hash-table-contains?", &Interpreter::eval_hash_table_containsp, nullptr},
        {"hash-table-try-ref", &Interpreter::eval_hash_table_try_ref, nullptr},
        {"hash-table-length", &Interpreter::eval_hash_table_length, nullptr},
        {"hash-table->list", &Interpreter::eval_hash_table_to_list, nullptr},

        // Universtal table
        {"get-at", &Interpreter::eval_get_at, nullptr},
        {"set-at!", &Interpreter::eval_set_at, nullptr},

        // Итераторв
        {"string-for-each", &Interpreter::eval_string_for_each, nullptr},
        {"vector-for-each", &Interpreter::eval_vector_for_each, nullptr},
        {"hash-table-for-each", &Interpreter::eval_hash_table_for_each, nullptr},
        {"list-for-each", &Interpreter::eval_list_for_each, nullptr},

        // Системные и ввод-вывод
        {"print", &Interpreter::eval_print, nullptr},
        {"pfmt", &Interpreter::eval_pfmt, &args_with_keys},
        {"inspect", &Interpreter::eval_inspect, nullptr},
        {"fmt", &Interpreter::eval_fmt, &args_with_keys},
        {"cfmt", &Interpreter::eval_cfmt, nullptr},
        {"error", &Interpreter::eval_error, nullptr},

        // Logger
        {"log", &Interpreter::eval_log, nullptr},

        // Evaluation and parsing
        {"read-str", &Interpreter::eval_read_str, nullptr},
        {"parse-str", &Interpreter::eval_parse_str, nullptr},
        {"read-file", &Interpreter::eval_read_file, nullptr},
        {"load", &Interpreter::eval_load, nullptr},

        // Files
        {"file-exists?", &Interpreter::eval_file_exists_p, nullptr},
        {"get-path", &Interpreter::eval_get_path, nullptr},
        {"find-file", &Interpreter::eval_find_file, nullptr},
        {"read-binary-file", &Interpreter::eval_read_binary_file, nullptr},
        {"write-binary-file", &Interpreter::eval_write_binary_file, nullptr},
        {"read-text-file", &Interpreter::eval_read_text_file, nullptr},
        {"write-text-file", &Interpreter::eval_write_text_file, nullptr},
        {"export-hex", &Interpreter::eval_export_intel_hex, nullptr},
        {"crc32", &Interpreter::eval_crc32, nullptr},

        // Reader
        {"set-macro-character", &Interpreter::eval_set_macro_character, nullptr},
        {"remove-macro-character", &Interpreter::eval_remove_macro_character, nullptr},
        {"get-macro-character", &Interpreter::eval_get_macro_character, nullptr},
        {"read", &Interpreter::eval_read, nullptr},
        {"read-char", &Interpreter::eval_read_char, nullptr},
        {"peek-char", &Interpreter::eval_peek_char, nullptr},
        {"read-delimited-list", &Interpreter::eval_read_delimited_list, nullptr},

        // Macro system
        {"macroexpand", &Interpreter::eval_macroexpand, nullptr},

        // System
        {"system", &Interpreter::eval_system, nullptr},
        {"exit", &Interpreter::eval_exit, nullptr},
        {"get-environment-variable", &Interpreter::eval_get_env,
         nullptr}, // было eval_get_environment_variable

        // Прочие
        {"gensym", &Interpreter::eval_gensym, nullptr},
        {"eval", &Interpreter::eval_eval, nullptr},
        {"defsetf", &Interpreter::eval_defsetf, nullptr},
        {"get-setter", &Interpreter::eval_get_setter, nullptr},

        // Преобразования типов
        {"number->string", &Interpreter::eval_number_to_string, &args_with_keys},
        {"string->number", &Interpreter::eval_string_to_number, nullptr},
        {"char->integer", &Interpreter::eval_char_to_integer, nullptr},
        {"integer->char", &Interpreter::eval_integer_to_char, nullptr},
        {"string->symbol", &Interpreter::eval_string_to_symbol, nullptr},
        {"symbol->string", &Interpreter::eval_symbol_to_string, nullptr},

        // Математические функции
        {"abs", &Interpreter::eval_abs, nullptr},
        {"max", &Interpreter::eval_max, nullptr},
        {"min", &Interpreter::eval_min, nullptr},
        {"expt", &Interpreter::eval_expt, nullptr},
        {"sqrt", &Interpreter::eval_sqrt, nullptr},
        {"ash", &Interpreter::eval_ash, nullptr},

        // Математика: округление и остаток
        {"floor", &Interpreter::eval_floor, nullptr},
        {"ceiling", &Interpreter::eval_ceiling, nullptr},
        {"round", &Interpreter::eval_round, nullptr},
        {"mod", &Interpreter::eval_mod, nullptr},
        {"abs", &Interpreter::eval_abs, nullptr},

        // Тригонометрия
        {"sin", &Interpreter::eval_sin, nullptr},
        {"cos", &Interpreter::eval_cos, nullptr},
        {"tan", &Interpreter::eval_tan, nullptr},
        {"atan", &Interpreter::eval_atan, nullptr},

        // Константы
        {"pi", &Interpreter::eval_pi, nullptr},

        {"logand", &Interpreter::eval_logand, nullptr},
        {"logior", &Interpreter::eval_logior, nullptr},
        {"logxor", &Interpreter::eval_logxor, nullptr},
        {"lognot", &Interpreter::eval_lognot, nullptr},
        {"lshift", &Interpreter::eval_lshift, nullptr},
        {"rshift", &Interpreter::eval_rshift, nullptr},

        // Время
        {"time-seconds", &Interpreter::eval_time_seconds, nullptr},
        {"time-milliseconds", &Interpreter::eval_time_milliseconds, nullptr},
        {"time-microseconds", &Interpreter::eval_time_microseconds, nullptr},
        {"time-nanoseconds", &Interpreter::eval_time_nanoseconds, nullptr},
        // Отладка
        {"source-info", &Interpreter::eval_source_info, nullptr},
        {"get-context", &Interpreter::eval_get_context, nullptr},

        //
        {"step", &Interpreter::eval_step, nullptr},
        {"&", &Interpreter::eval_addr_of, nullptr},
        {"&+", &Interpreter::eval_addr_plus, nullptr},

        {"mem-get", &Interpreter::eval_mem_get, nullptr},
        {"mem-set!", &Interpreter::eval_mem_set, nullptr},

        // Buffer
        {"make-buffer", &Interpreter::eval_make_static_buffer, nullptr},
        {"make-buffer-writer", &Interpreter::eval_make_static_writer, nullptr},
        {"make-buffer-pointer", &Interpreter::eval_make_buffer_pointer, nullptr},
        {"buffer-dump", &Interpreter::eval_buffer_dump, nullptr},
        {"buffer-write", &Interpreter::eval_buffer_write, &args_with_keys},
        {"buffer-read", &Interpreter::eval_buffer_read, &args_with_keys},
        {"buffer-label-set!", &Interpreter::eval_buffer_label_set, &args_with_keys},
        {"buffer-label-get", &Interpreter::eval_buffer_label_get, &args_with_keys},
        {"buffer-write-reloc", &Interpreter::eval_buffer_reloc, nullptr},
        {"buffer-link", &Interpreter::eval_buffer_link, nullptr},
    });

    // Type system
    TypeSystem::instance().add_builtin_types();
    define_var_in_env(get_global_environment(), TypeSystem::instance().to_alias(), "*type-system*");

    // Special forms
    add_special_form("defenum", &Interpreter::eval_defenum_special); // does not return anything
    add_special_form("deftype", &Interpreter::eval_deftype_special); // does not return anything
    add_special_form("typespec",
                     &Interpreter::eval_typespec_special); // return s-expression of typespec
    add_builtin_form("init-types", &Interpreter::eval_init_types); // does not return anything

    // load the standard library
    if (load_libs)
        load_library();
}

void Interpreter::load_library() {
    auto cmd = "(load-file \"lib.sot\")";
    eval_form(m_reader.read_from_string(cmd), m_global_environment.as_env_ptr());
}

void Interpreter::init_builtin_forms(const std::initializer_list<BuiltinEntryConfig> forms) {
    for (const auto &entry : forms) {
        // def.method — это функция, def.spec — это ArgumentSpec
        add_builtin_form(entry.name, entry.method, entry.spec);
    }
}

void Interpreter::init_special_forms(const std::initializer_list<SpecialEntryConfig> forms) {
    for (const auto &entry : forms) {
        add_special_form(entry.name, entry.method, entry.spec);
    }
}

// --- Методы регистрации ---
void Interpreter::add_special_form(std::string name, SpecialFormMethod method,
                                   ArgumentSpec *specs) {
    // Используем обычный конструктор shared_ptr
    auto sf_obj = std::shared_ptr<SpecialFormObject>(new SpecialFormObject(method, specs, name));

    Object o = Object::make_heap_object(sf_obj, ObjectType::SPECIAL_FORM);
    define_var_in_env(m_global_environment, o, name.c_str());
}

void Interpreter::add_builtin_form(std::string name, BuiltinFormMethod method,
                                   ArgumentSpec *specs) {
    auto builtin_obj =
        std::shared_ptr<BuiltinFunctionObject>(new BuiltinFunctionObject(method, specs, name));

    Object o = Object::make_heap_object(builtin_obj, ObjectType::PRIMITIVE);
    define_var_in_env(m_global_environment, o, name.c_str());
}

// ============================================================
// Environment
// ============================================================

bool Interpreter::try_symbol_lookup(const Object                             &sym,
                                    const std::shared_ptr<EnvironmentObject> &env, Object *dest) {
    // Boolean проверка
    if (sym.as_symbol().name_ptr == get_true().as_symbol().name_ptr ||
        sym.as_symbol().name_ptr == m_obj_false.as_symbol().name_ptr) {
        *dest = sym;
        return true;
    }

    // Итеративный поиск по цепочке окружений
    EnvironmentObject *search_env = env.get();
    while (search_env != nullptr) {
        Object *obj = search_env->find(sym.as_symbol());
        if (obj) {
            *dest = *obj;
            return true;
        }
        search_env = search_env->parent_env.get();
    }

    return false;
}

void Interpreter::set_args_in_env(const Object &form, const Arguments &args,
                                  const ArgumentSpec                       &arg_spec,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    if (arg_spec.rest.empty() && args.unnamed.size() != arg_spec.unnamed.size()) {
        throw_eval_error(form, "did not get the expected number of unnamed arguments (got " +
                                   std::to_string(args.unnamed.size()) + ", expected " +
                                   std::to_string(arg_spec.unnamed.size()) + ")");
    } else if (!arg_spec.rest.empty() && args.unnamed.size() < arg_spec.unnamed.size()) {
        throw_eval_error(form, "args with rest didn't get enough arguments (got " +
                                   std::to_string(args.unnamed.size()) + " but need at least " +
                                   std::to_string(arg_spec.unnamed.size()) + ")");
    }

    // unnamed args
    for (size_t i = 0; i < arg_spec.unnamed.size(); i++) {
        env->vars.set(intern(arg_spec.unnamed.at(i).name), args.unnamed.at(i));
    }

    // named args
    for (const auto &kv : arg_spec.named) {
        env->vars.set(intern(kv.first), args.named.at(kv.first));
    }

    // rest args
    if (!arg_spec.rest.empty()) {
        // args.rest теперь сам по себе является списком Pair или Null.
        // Мы просто биндим его в окружение. Никаких копирований!
        env->vars.set(intern(arg_spec.rest), args.rest);
    } else {
        // Если rest не пустой, но спецификация его не ждет
        if (args.has_rest()) {
            throw_eval_error(form, "got too many arguments");
        }
    }
}
/*!
 * In env, set the variable named "name" to the value var.
 */
void Interpreter::define_var_in_env(const Object &env, const Object &var, const char *name) {
    env.as_env()->vars.set(InternedSymbolPtr{intern(name)}, var);
}

// ============================================================
// Tools and utilities
// ============================================================

Object Interpreter::make_symbol(const char *name) {
    return m_symbol_table.make_symbol(name);
}

Object Interpreter::make_symbol(const std::string &name) {
    return m_symbol_table.make_symbol(name.c_str());
}

InternedSymbolPtr Interpreter::intern(const std::string &name) {
    return m_symbol_table.intern(name.c_str());
}

// ============================================================
// REPL
// ============================================================
/*!
 * Display the REPL, which will run until the user executes exit.
 */
void Interpreter::execute_repl() {
    std::string input;

    // auto repl_env = std::make_shared<EnvironmentObject>();
    // fmt::print(fg(fmt::color::gray), "{}i Scriptable Object-Oriented Toolkit {} Core
    // [sha:{}]\n", SOOT_VERSION, SOOT_NAME, BUILT_SHA); fmt::print(fg(fmt::color::gray), "Type
    // (exit) or 'quit' to leave\n");

    while (true) {
        std::cout << "sooti> ";
        std::cout.flush();

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty())
            continue;
        if (input == "quit" || input == "exit")
            break;

        try {
            // read something from the user and evaluate
            Object result = eval_string(input, "repl");
            // Print
            printf("%s\n", result.print().c_str());
        } catch (script::ExitException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
            exit(e.exit_code);
        } catch (script::EvalException &e) {
            if (e.already_printed)
                return;
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
        }
    }

    std::cout << "Goodbye!\n";
}

/*!
 * Signal an evaluation error. This throws an exception which will unwind the evaluation stack
 * for debugging.
 */
void Interpreter::throw_eval_error(const Object &o, const std::string &err) {
    throw EvalException(o, err);
}

void Interpreter::throw_arity_mismatch(const Object &form, size_t expected, size_t got,
                                       const Arguments &args) {
    throw_eval_error(form, fmt::format("Arity mismatch: expected {} arguments, but got {} in: {}",
                                       expected, got, args.print_full()));
}

void Interpreter::throw_type_mismatch(const Object &form, size_t index,
                                      const std::vector<ObjectType> &expected, ObjectType got,
                                      const Arguments &args) {
    std::string expected_str;
    for (size_t i = 0; i < expected.size(); ++i) {
        expected_str += object_type_to_string(expected[i]) + (i < expected.size() - 1 ? ", " : "");
    }
    throw_eval_error(
        form, fmt::format("Type error at argument [{}]: expected one of [{}], but got {} in: {}",
                          index, expected_str, object_type_to_string(got), args.print_full()));
}

void Interpreter::throw_missing_named_arg(const Object &form, const std::string &name,
                                          const Arguments &args) {
    throw_eval_error(form, fmt::format("Required named argument ':{}' is missing in: {}", name,
                                       args.print_full()));
}

void Interpreter::throw_unexpected_named_arg(const Object &form, const std::string &name,
                                             const Arguments &args) {
    throw_eval_error(
        form, fmt::format("Unexpected named argument ':{}' in: {}", name, args.print_full()));
}

void Interpreter::throw_named_type_mismatch(const Object &form, const std::string &name,
                                            const std::vector<ObjectType> &expected,
                                            ObjectType                     got) {
    std::string expected_str;
    for (size_t i = 0; i < expected.size(); ++i) {
        expected_str += object_type_to_string(expected[i]) + (i < expected.size() - 1 ? ", " : "");
    }
    throw_eval_error(form,
                     fmt::format("Type error for named argument ':{}': expected [{}], but got {}",
                                 name, expected_str, object_type_to_string(got)));
}

Object Interpreter::eval_string(const std::string &expression, const std::string &filename) {
    auto   env = m_global_environment.as_env_ptr();
    Object last_result = Object::make_null();
    // read something from the user
    Object code = m_reader.read_from_string(
        expression, true, filename, [&](const ReaderEvent &evt) -> Object {
            // evaluate
            switch (evt.type) {
            case ReaderEvent::Type::FORM_READ:
                // fmt::print("DEBUG: Interpreter::eval_string FORM_READ {} \n",
                // evt.form.print());
                last_result = this->eval_form(evt.form, env);
                // fmt::print("DEBUG: Interpreter::eval_string FORM_READ {} -> {}\n",
                // evt.form.print(), last_result.print());
                break;
            case ReaderEvent::Type::MACRO_REQUEST:
                // fmt::print("DEBUG: Interpreter::eval_string MACRO_REQUEST {} \n",
                // evt.token.print());
                last_result = this->call_lambda_internal(evt.form, {evt.reader, evt.token});
                // fmt::print("DEBUG: Interpreter::eval_string MACRO_REQUEST {} -> {}\n",
                // evt.token.print(), last_result.print());
                break;
            default:
                throw std::runtime_error("Undexpeted");
                break;
            }

            return last_result;
        });
    return last_result;
}

Object Interpreter::eval_file_internal(const std::vector<std::string> &file_path) {
    auto   env = m_global_environment.as_env_ptr();
    Object last_result = Object::make_null();

    Object code =
        m_reader.read_from_file(file_path, true, true, [&](const ReaderEvent &evt) -> Object {
            // evaluate
            switch (evt.type) {
            case ReaderEvent::Type::FORM_READ:
                // fmt::print("DEBUG: Interpreter::eval_file_internal FORM_READ {} \n",
                // evt.form.print());
                last_result = this->eval_form(evt.form, env);
                // fmt::print("DEBUG: Interpreter::eval_file_internal FORM_READ {} -> {}\n",
                // evt.form.print(), last_result.print());
                break;
            case ReaderEvent::Type::MACRO_REQUEST:
                // fmt::print("DEBUG: Interpreter::eval_file_internal MACRO_REQUEST {} \n",
                // evt.token.print());
                last_result = this->call_lambda_internal(evt.form, {evt.reader, evt.token});
                // fmt::print("DEBUG: Interpreter::eval_file_internal MACRO_REQUEST {} -> {}\n",
                // evt.token.print(), last_result.print());
                break;
            default:
                throw std::runtime_error("Undexpeted");
                break;
            }

            return last_result;
        });
    return last_result;
}

// Same as method befor but for cases wher are no parent form
Object Interpreter::eval_form(const Object &obj, const std::shared_ptr<EnvironmentObject> &env) {
    return eval_with_rewind(obj, obj, env);
}

Object Interpreter::call_lambda_internal(const Object &lambda, const std::vector<Object> &args) {
    if (!lambda.is_lambda()) {
        throw std::runtime_error("call_lambda: object is not a lambda");
    }

    const auto &lam = lambda.as_lambda();

    // 1. Проверка аргументов
    size_t min_args = lam->args.unnamed.size();
    bool   has_rest = !lam->args.rest.empty() || lam->args.varargs;

    if (args.size() < min_args || (!has_rest && args.size() > min_args)) {
        throw_eval_error(
            lambda,
            fmt::format("call_lambda: wrong number of arguments (expected {}, got {})",
                        has_rest ? fmt::format("at least {}", min_args) : std::to_string(min_args),
                        args.size()));
    }

    // 2. Создаем Arguments
    // 2. Создаем Arguments
    Arguments func_args;

    // Резервируем место для скорости
    func_args.unnamed.reserve(min_args);

    // Заполняем только позиционные (обязательные) аргументы
    for (size_t i = 0; i < min_args; ++i) {
        func_args.unnamed.push_back(args[i]);
    }

    // 3. Обработка rest
    if (has_rest) {
        if (args.size() > min_args) {
            Object head = Object::make_null();
            for (size_t i = args.size(); i > min_args; --i) {
                head = Object::make_pair(args[i - 1], head);
            }
            func_args.rest = head;
        } else {
            func_args.rest = Object::make_null();
        }
    }
    // 3. Создаем окружение для выполнения
    // ВАЖНО: lam->parent_env - это уже shared_ptr<EnvironmentObject>
    auto lam_env_obj = EnvironmentObject::make_new("lambda-call", lam->parent_env);
    auto lam_env = lam_env_obj.as_env_ptr();

    // 4. Биндим аргументы
    Object dummy_form = m_symbol_table.make_symbol("call-lambda");
    set_args_in_env(dummy_form, func_args, lam->args, lam_env);

    // 5. Выполняем тело

    auto result = eval_list_return_last(lam->body, lam->body, lam_env);

    return result;
}

// ============================================================
// Eval With Rewind (Main Recursion)
// ============================================================
std::string truncate_obj(std::string str, size_t max_len) {
    // 1. Если строка слишком длинная — обрезаем и ставим "..."
    if (str.length() > max_len) {
        // Защита от слишком маленького max_arg_len (меньше 3)
        if (max_len <= 3) {
            return str.substr(0, max_len);
        }
        return str.substr(0, max_len - 3) + "...";
    }

    // 2. Если строка короче — дополняем пробелами справа
    // (Или слева, если использовать std::right, но для данных обычно лучше слева)
    str.append(max_len - str.length(), ' ');

    return str;
}

void Interpreter::print_form_info(const Object &form) {
    // Получаем информацию о расположении формы в исходном коде
    const std::string info = m_reader.get_db().get_info_for(form);

    // Печатаем информацию только если она доступна
    if (!info.empty() && info != "?") {
        fmt::print("{}", info);

        // Выводим стек вызовов
        fmt::print("\nCall stack (most recent call last):\n");

        const ContextFrame *current_frame = m_top_frame;
        int                 depth = 0;

        while (current_frame != nullptr) {
            auto        frame_info_opt = m_reader.get_db().get_short_info_for(current_frame->form);
            std::string frame_repr = truncate_obj(current_frame->form.print(), 60);

            if (frame_info_opt && frame_info_opt->line_idx_to_display > 0) {
                fmt::print("  [{:02d}] {} at {}:{:d}\n", depth, frame_repr,
                           frame_info_opt->filename, frame_info_opt->line_idx_to_display);
            } else {
                fmt::print("  [{:02d}] {}\n", depth, frame_repr);
            }

            current_frame = current_frame->prev;
            depth++;
        }
    }
}

/*!
 * Evaluate the given expression, with a "checkpoint" in the evaluation stack here.  If there is
 * an evaluation error, there will be a print indicating there was an error in the evaluation of
 * "obj", and if possible what file/line "obj" comes from.
 */
Object Interpreter::eval_with_rewind(const Object &parent_form, const Object &obj,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    ContextFrame frame = {.depth = m_top_frame == nullptr ? 0 : m_top_frame->depth + 1,
                          .form = obj,
                          .prev = m_top_frame};
    m_top_frame = &frame;
    // fmt::print("<Interpreter::eval_with_rewind frame={}\n", frame.print());
    try {
        // fmt::print(">>>> parent: {}\n    obj: {}\n    src: {}\n",
        //     parent_form.print().c_str(),
        //     obj.print().c_str(),
        //      m_reader.get_db().get_info_for(obj)
        //
        //);

        auto result = eval(parent_form, obj, env);
        m_top_frame = frame.prev;
        return result;
    } catch (const ExitException &e) {
        m_top_frame = frame.prev;
        throw; // Пробрасываем в самый верх (в main loop)
    } catch (EvalException &e) {
        m_top_frame = frame.prev;
        if (!m_disable_printing) {
            if (e.error_header_required) {
                // 1. Печатаем "Шапку"
                fmt::print(fg(fmt::color::indian_red),
                           "\n─── ERROR ──────────────────────────────────\n");
                e.error_header_required = false;
            }

            if (e.detailed_error_required) {
                // 2. Пытаемся найти максимально точную локацию (LexToken или конкретная форма)
                std::string info = m_reader.get_db().get_info_for(e.form);

                // 3. Если не нашли, пробуем текущий уровень вызова (obj)
                if (info == "?") {
                    info = m_reader.get_db().get_info_for(obj);
                }

                // 4. Печатаем стрелочку, если нашли хоть какую-то локацию
                if (info != "?") {
                    fmt::print("{}", info);
                    e.detailed_error_required = false;
                }
                if (!e.already_printed) {
                    // 5. ПЕЧАТАЕМ САМУ ОШИБКУ (это критично!)
                    fmt::print(fg(fmt::color::indian_red), "Error: {}\n\n", e.message);
                    e.already_printed = true; // Помечаем, что "мясо" ошибки уже на экране
                }
            } else {
                if (obj.is_pair()) {
                    auto info_opt = m_reader.get_db().get_short_info_for(obj);
                    int  current_depth = frame.depth;
                    // Печатаем "at ..", только если есть реальный файл и строка > 0

                    int  max_size = 80;
                    auto obj_string = truncate_obj(obj.print(), max_size);
                    if (info_opt && info_opt->line_idx_to_display > 0) {
                        fmt::print(fg(fmt::color::dim_gray), "  [{:02d}] in {} at {}:{:d}\n",
                                   current_depth, obj_string, info_opt->filename,
                                   info_opt->line_idx_to_display);
                    } else {
                        fmt::print(fg(fmt::color::dim_gray), "  [{:02d}] in {}\n", current_depth,
                                   obj_string);
                    }

                    if (current_depth == 0)
                        fmt::print(fg(fmt::color::dim_gray), "\n");
                }
            }
            if (!e.already_printed) {
                // 5. ПЕЧАТАЕМ САМУ ОШИБКУ (это критично!)
                fmt::print(fg(fmt::color::indian_red), "Error: {}\n\n", e.message);
                e.already_printed = true; // Помечаем, что "мясо" ошибки уже на экране
            }
        }

        throw;
    } catch (const std::exception &e) {
        m_top_frame = frame.prev;
        throw; // Пробрасываем в самый верх (в main loop)
    }
}

// ============================================================
// Eval (Single Item)
// ============================================================

Object Interpreter::eval(const Object &parent_form, const Object &obj,
                         const std::shared_ptr<EnvironmentObject> &env) {
    switch (obj.type) {
    case ObjectType::POINTER:
        return obj.as_pointer()->deref();
    case ObjectType::NATIVE_REF:
        return obj;
    case ObjectType::SYMBOL:
        if (obj.is_keyword())
            return obj;
        else
            return eval_symbol(parent_form, obj, env);
    case ObjectType::PAIR:
        return eval_pair(parent_form, obj, env);
    case ObjectType::INTEGER:
    case ObjectType::FLOAT:
    case ObjectType::STRING:
    case ObjectType::CHAR:
    case ObjectType::EMPTY_LIST:
    case ObjectType::ARRAY:
    case ObjectType::STRING_HASH_TABLE:
    case ObjectType::READER:
        return obj;
    case ObjectType::LAMBDA:
        return eval_pair(parent_form, obj, env);
    default:
        throw_eval_error(obj, "cannot evaluate this object");
    }
    return Object::make_null();
}

// ============================================================
// Eval (Various Types)
// ============================================================

std::vector<Object> Interpreter::eval_list(const Object                             &list,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    std::vector<Object> result;
    Object              current = list;

    while (current.is_pair()) {
        result.push_back(eval_with_rewind(list, current.as_pair()->car, env));
        current = current.as_pair()->cdr;
    }

    if (!current.is_null()) {
        throw_eval_error(list, "malformed argument list");
    }

    return result;
}

// Запуск функции
Object Interpreter::eval_list_return_last(const Object &form, Object rest,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    if (rest.is_null()) {
        return rest;
    }

    const Object *iter = &rest;
    while (true) {
        const Object *next = &iter->as_pair()->cdr;
        const Object *item = &iter->as_pair()->car;

        if (next->is_null()) {
            return eval_with_rewind(form, *item, env);
        } else {
            eval_with_rewind(form, *item, env);
            iter = next;
        }
    }
}

Object Interpreter::eval_symbol(const Object &parent_form, const Object &sym,
                                const std::shared_ptr<EnvironmentObject> &env) {
    Object result;
    if (!try_symbol_lookup(sym, env, &result)) {
        throw EvalException(parent_form, "Unbound variable: " +
                                             std::string(std::string(sym.as_symbol().c_str())));
    }
    return result;
}
Object Interpreter::eval_pair(const Object &parent_form, const Object &obj,
                              const std::shared_ptr<EnvironmentObject> &env) {
    const auto   &pair = obj.as_pair();
    const Object &head = pair->car;
    const Object &rest = pair->cdr;

    // 1. Вычисляем голову. Благодаря тому, что примитивы и спецформы теперь в Environment,
    // этот вызов вернет нам соответствующий HeapObject (SpecialForm, Primitive, Lambda или
    // Macro).
    Object eval_head = eval_with_rewind(obj, head, env);

    // 2. Диспетчеризация по типу вычисленного объекта

    // --- SPECIAL FORMS (if, define, quote, set! ...) ---
    if (eval_head.is_special_form()) {
        // Получаем доступ к методу через native_ref
        auto spec_form = eval_head.as_native_ref<SpecialFormObject>();
        // Передаем 'rest' как есть (без вычисления аргументов)
        return ((*this).*(spec_form->method))(obj, rest, env);
    }

    // --- PRIMITIVES (+, -, print, segment-get-abs-pc ...) ---
    if (eval_head.is_primitive()) {
        auto builtin = eval_head.as_native_ref<BuiltinFunctionObject>();

        // Сначала собираем аргументы из списка 'rest' и проверяем количество (specs)
        Arguments args = get_args(obj, rest, builtin->specs);
        // Примитивы всегда требуют вычисленных аргументов
        eval_args(obj, &args, env);

        return ((*this).*(builtin->method))(obj, args, env);
    }

    // --- MACROS ---
    if (eval_head.is_macro()) {
        const auto &macro = eval_head.as_macro();
        // Макросы получают аргументы как код (не вычисляем их здесь)
        Arguments args = get_args_with_spec(obj, rest, macro->args);

        auto mac_env = std::make_shared<EnvironmentObject>();
        mac_env->parent_env = env; // Динамическое или лексическое — на твой вкус, обычно env
        set_args_in_env(obj, args, macro->args, mac_env);

        // ШАГ 1: Запускаем программу-макрос, чтобы она создала (выпекла) код.
        Object expansion = eval_list_return_last(macro->body, macro->body, mac_env);
        // ШАГ 2: Выполняем то, что макрос нам вернул, в исходном окружении.
        return eval_with_rewind(obj, expansion, env);
    }

    // --- LAMBDAS (User defined functions) ---
    if (eval_head.is_lambda()) {
        const auto &lam = eval_head.as_lambda();

        Arguments args = get_args_with_spec(obj, rest, lam->args);
        eval_args(obj, &args, env);

        auto lam_env_obj = EnvironmentObject::make_new();
        auto lam_env = lam_env_obj.as_env_ptr();

        // Лексическое связывание: используем окружение, где лямбда была создана
        lam_env->parent_env = lam->parent_env;

        set_args_in_env(obj, args, lam->args, lam_env);
        return eval_list_return_last(lam->body, lam->body, lam_env);
    }

    // 3. Если мы дошли сюда, значит голова — не функция и не спецформа
    throw_eval_error(parent_form,
                     "Object is not callable: " + eval_head.type_name() + " " + eval_head.print());
    return m_sym_null; // unreachable
}

Object Interpreter::eval_define_special(const Object &form, const Object &rest,
                                        const std::shared_ptr<EnvironmentObject> &env) {
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

    Object value = eval_with_rewind(form, value_part.as_pair()->car, env);

    // Сохраняем в ПЕРЕДАННЫЙ environment
    env->vars.set(name_obj.as_symbol(), value);

    return value;
}

Object Interpreter::eval_set_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    auto args = get_args(form, rest, ArgumentSpec(false, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {});
    auto   to_define = args.unnamed.at(0);
    Object to_set = eval_with_rewind(form, args.unnamed.at(1), env);

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
            throw_eval_error(to_define,
                             "symbol is not defined " + std::string(to_define.as_symbol().c_str()));
        }
    }
}

Object Interpreter::eval_lambda_special(const Object &form, const Object &rest,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    // ..
    Object params_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr; // ВСЁ тело после параметров

    if (!params_obj.is_list()) {
        throw_eval_error(form, "lambda: parameter list must be a list");
    }

    ArgumentSpec args = parse_arg_spec(form, params_obj);

    if (body_obj.is_null()) {
        throw_eval_error(form, "lambda: expected body after parameter list");
    }

    Object lambda_obj = LambdaObject::make_new();
    auto   lambda = lambda_obj.as_lambda();

    lambda->args = args;
    lambda->body = body_obj; // ← ВСЁ тело, а не только .as_pair()->car!
    lambda->parent_env = env;

    return lambda_obj;
}

Object Interpreter::eval_macro_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "macro must receive two arguments");
    }

    Object arg_list = rest.as_pair()->car;
    if (!arg_list.is_pair() && !arg_list.is_null()) {
        throw_eval_error(form, "macro argument list must be a list");
    }

    Object new_macro = MacroObject::make_new();
    auto   m = new_macro.as_macro();
    m->args = parse_arg_spec(form, arg_list);

    Object rrest = rest.as_pair()->cdr;
    if (!rrest.is_pair()) {
        throw_eval_error(form, "macro body must be a list");
    }

    m->body = rrest;
    m->parent_env = env;
    return new_macro;
}

/*!
 * Quote special form: (quote x) -> x
 */
Object Interpreter::eval_quote_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto args = get_args_no_named(form, rest, ArgumentSpec(false, true));
    if (!args.unnamed.size()) {
        throw_eval_error(form, "quote requires one argument");
    }
    return args.unnamed.front();
}

/*!
 * Quasiquote (backtick) evaluation
 */
Object Interpreter::eval_quasiquote_special(const Object &form, const Object &rest,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    if (rest.type != ObjectType::PAIR || rest.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
        throw_eval_error(form, "quasiquote must have one argument!");
    }
    return quasiquote_helper(rest.as_pair()->car, env);
}

Object Interpreter::eval_begin_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;

    return eval_list_return_last(rest, rest, env);
}

Object Interpreter::eval_cond_special(const Object &form, const Object &rest,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    Object current_clause = rest;

    while (current_clause.is_pair()) {
        Object clause = current_clause.as_pair()->car;

        if (!clause.is_pair()) {
            throw_eval_error(form, "cond clause must be a pair");
        }

        Object condition = clause.as_pair()->car;
        Object body = clause.as_pair()->cdr;

        // Особый случай: (else ..)
        if (condition.is_symbol() && condition.as_symbol().name_ptr &&
            strcmp(condition.as_symbol().name_ptr, "else") == 0) {
            if (!body.is_pair()) {
                throw_eval_error(form, "cond else clause must have body");
            }
            return eval_with_rewind(form, body.as_pair()->car, env);
        }

        Object condition_result = eval_with_rewind(form, condition, env);

        if (truthy(condition_result)) {
            if (body.is_pair()) {
                return eval_with_rewind(form, body.as_pair()->car, env);
            } else {
                return condition_result;
            }
        }

        current_clause = current_clause.as_pair()->cdr;
    }

    return Object::make_null();
}

Object Interpreter::eval_if_special(const Object &form, const Object &rest,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "if requires condition and branches");
    }

    Object condition_obj = rest.as_pair()->car;
    Object then_part_obj = rest.as_pair()->cdr;

    if (!then_part_obj.is_pair()) {
        throw_eval_error(form, "if requires then branch");
    }

    Object condition_result = eval_with_rewind(form, condition_obj, env);

    if (truthy(condition_result)) {
        return eval_with_rewind(form, then_part_obj.as_pair()->car, env);
    } else {
        Object else_part = then_part_obj.as_pair()->cdr;
        if (else_part.is_pair()) {
            return eval_with_rewind(form, else_part.as_pair()->car, env);
        } else {
            return Object::make_null();
        }
    }
}

Object Interpreter::eval_or_special(const Object &form, const Object &rest,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    Object current = rest;

    while (current.is_pair()) {
        Object result = eval_with_rewind(form, current.as_pair()->car, env);
        if (truthy(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return m_obj_false;
}

Object Interpreter::eval_and_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    Object current = rest;
    Object result = get_true();

    while (current.is_pair()) {
        result = eval_with_rewind(form, current.as_pair()->car, env);
        if (!truthy(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return result;
}

Object Interpreter::eval_let_star_special(const Object &form, const Object &rest,
                                          const std::shared_ptr<EnvironmentObject> &env) {
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

        Object value = eval_with_rewind(form, value_part.as_pair()->car, current_env);
        new_env->vars.set(name_obj.as_symbol(), value);

        current_env = new_env;
        current_binding = current_binding.as_pair()->cdr;
    }

    Object result = Object::make_null();
    Object current_body = body_obj;
    while (current_body.is_pair()) {
        result = eval_with_rewind(form, current_body.as_pair()->car, current_env);
        current_body = current_body.as_pair()->cdr;
    }
    return result;
}

Object Interpreter::eval_let_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
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

        Object value = eval_with_rewind(form, value_part.as_pair()->car, env);
        let_env->vars.set(name_obj.as_symbol(), value);

        current_binding = current_binding.as_pair()->cdr;
    }

    Object result = Object::make_null();
    Object current_body = body_obj;

    while (current_body.is_pair()) {
        result = eval_with_rewind(form, current_body.as_pair()->car, let_env);
        current_body = current_body.as_pair()->cdr;
    }
    return result;
}

Object Interpreter::eval_while_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "while requires condition and body");
    }

    Object condition_obj = rest.as_pair()->car;
    Object body_obj = rest.as_pair()->cdr;

    if (!body_obj.is_pair()) {
        throw_eval_error(form, "while requires a body");
    }

    Object result = Object::make_null();

    while (true) {
        Object condition_result = eval_with_rewind(form, condition_obj, env);

        if (!truthy(condition_result)) {
            break;
        }

        Object current_body = body_obj;
        while (current_body.is_pair()) {
            result = eval_with_rewind(form, current_body.as_pair()->car, env);
            current_body = current_body.as_pair()->cdr;
        }
    }

    return result;
}

Object build_list_with_spliced_tail(std::vector<Object> &&objects, const Object &tail) {
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
/*!
 * Recursive quasi-quote evaluation
 */
Object Interpreter::quasiquote_helper(const Object                             &form,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    const Object       *lst_iter = &form;
    std::vector<Object> result;
    for (;;) {
        if (lst_iter->type == ObjectType::PAIR) {
            const Object &item = lst_iter->as_pair()->car;
            if (item.type == ObjectType::PAIR) {
                if (item.as_pair()->car.type == ObjectType::SYMBOL &&
                    item.as_pair()->car.as_symbol() == "unquote") {
                    const Object &unquote_arg = item.as_pair()->cdr;
                    if (unquote_arg.type != ObjectType::PAIR ||
                        unquote_arg.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
                        throw_eval_error(form, "unquote must have exactly 1 arg");
                    }
                    result.push_back(eval_with_rewind(form, unquote_arg.as_pair()->car, env));
                    lst_iter = &lst_iter->as_pair()->cdr;
                    continue;
                } else if (item.as_pair()->car.type == ObjectType::SYMBOL &&
                           item.as_pair()->car.as_symbol() == "unquote-splicing") {
                    const Object &unquote_arg = item.as_pair()->cdr;
                    if (unquote_arg.type != ObjectType::PAIR ||
                        unquote_arg.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
                        throw_eval_error(form, "unquote must have exactly 1 arg");
                    }

                    // bypass normal addition:
                    lst_iter = &lst_iter->as_pair()->cdr;
                    Object splice_result = eval_with_rewind(form, unquote_arg.as_pair()->car, env);
                    if (lst_iter->type == ObjectType::EMPTY_LIST) {
                        // optimization!
                        return build_list_with_spliced_tail(std::move(result), splice_result);
                    }

                    const Object *to_add = &splice_result;
                    for (;;) {
                        if (to_add->type == ObjectType::PAIR) {
                            result.push_back(to_add->as_pair()->car);
                            to_add = &to_add->as_pair()->cdr;
                        } else if (to_add->type == ObjectType::EMPTY_LIST) {
                            break;
                        } else {
                            throw_eval_error(form, "malformed unquote-splicing result");
                        }
                    }
                    continue;
                } else {
                    lst_iter = &lst_iter->as_pair()->cdr;

                    if (item.is_pair()) {
                        result.push_back(quasiquote_helper(item, env));
                    } else {
                        result.push_back(item);
                    }
                    continue;
                }
            }
            result.push_back(item);
            lst_iter = &lst_iter->as_pair()->cdr;
        } else if (lst_iter->type == ObjectType::EMPTY_LIST) {
            return build_list(std::move(result));
        } else {
            throw_eval_error(form, "malformed quasiquote");
        }
    }
}

// ============================================================
// Конвертирование типов lpres
// ============================================================

int64_t Interpreter::number_to_integer(const Object &obj) {
    if (obj.is_integer()) {
        return obj.as_integer();
    } else if (obj.is_float()) {
        return static_cast<int64_t>(obj.as_float());
    } else {
        throw_eval_error(obj, "object cannot be converted to integer");
    }
    return 0;
}

double Interpreter::number_to_float(const Object &obj) {
    if (obj.is_float()) {
        return obj.as_float();
    } else if (obj.is_integer()) {
        return static_cast<double>(obj.as_integer());
    } else {
        throw_eval_error(obj, "object cannot be converted to float");
    }
    return 0;
}

bool Interpreter::is_number(const Object &obj) {
    return obj.is_integer() || obj.is_float();
}

// ============================================================
// Работа с аргументами
// ============================================================

/*!
 * Get arguments being passed to a form. Don't evaluate them. There are two modes, "varargs" and
 * "not varargs".  With varargs enabled, any number of unnamed and named arguments can be given.
 * Without varags, the unnamed/named arguments must match the spec. By default specs are "not
 * vararg" - use make_varags() to get a varargs spec.  In general, macros/lambdas use specs, but
 * built-in forms use varargs.
 *
 * If form is "varargs", all arguments go to unnamed or named.
 *  Ex: (.. a b :key-1 c d) will put a, b, d in unnamed and d in key-1
 *
 * If form isn't "varargs", the expected number of unnamed arguments must match, unless "rest"
 * is specified, in which case the additional arguments are stored in rest.
 *
 * Also, if "varargs" isn't set, all keyword arguments must be defined. If the use doesn't
 * provide a value, the default value will be used instead.
 */
Arguments Interpreter::get_args(const Object &form, const Object &rest, const ArgumentSpec &spec) {
    Arguments args;

    // loop over forms in list
    const Object *current = &rest;
    while (!current->is_null()) {
        const auto &arg = current->as_pair()->car;

        // did we get a ":keyword"
        if (spec.keys && arg.is_keyword()) {
            auto        key_name = std::string(arg.as_symbol().name_ptr + 1);
            const auto &kv = spec.named.find(key_name);

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
            if (current->is_null()) {
                throw_eval_error(form, "Key argument didn't have a value");
            }

            args.named[key_name] = current->as_pair()->car;
        } else {
            // not a keyword. Add to unnamed or rest, depending on what we expect
            if (spec.varargs || args.unnamed.size() < spec.unnamed.size()) {
                args.unnamed.push_back(arg);
            } else {
                args.rest = *current;
                break;
            }
        }
        current = &current->as_pair()->cdr;
    }

    // Check expected key args and set default values on unset ones if possible
    for (auto &kv : spec.named) {
        const auto &defined_kv = args.named.find(kv.first);
        if (defined_kv == args.named.end()) {
            // key arg not given by user, try to use a default value.
            if (kv.second.has_default) {
                args.named[kv.first] = kv.second.default_value;
            } else {
                throw_eval_error(form, "key argument \"" + kv.first +
                                           "\" wasn't given and has no default value");
            }
        }
    }

    // Check argument size, if spec defines it
    if (!spec.varargs) {
        if (args.unnamed.size() < spec.unnamed.size()) {
            throw_eval_error(form, "didn't get enough arguments");
        }

        if (!args.rest_empty() && spec.rest.empty()) {
            throw_eval_error(form, "got too many arguments");
        }
    }

    return args;
}

/*!
 * Same as get_args, but it reffers to ArgumentSpect to find associations woth named :key
 * arguments are not parsed. It allows to pass  keywords to methods. (defun foo (a &key b &rest
 * lst) ...) can be invoked (foo 1 :b 2)  ;; named priorty but possible to do (foo :b :b 2)   ;;
 * unnamed prioprity (foo 1 :c :b 2) ;; unnamed prioprity
 *         ^
 *         +----------- to the (rest)
 */
Arguments Interpreter::get_args_with_spec(const Object &form, const Object &rest,
                                          const ArgumentSpec &spec) {
    Arguments     args;
    const Object *current = &rest;
    // fmt::print("{}\n  {}\n", form.print(),  form.print());
    //  1. Обработка всех позиционных аргументов (обязательные + опциональные)
    for (const auto &p_spec : spec.unnamed) {
        if (!current->is_null()) {
            // Если в вызове есть данные — просто забираем их.
            // Мы не проверяем на ключевые слова, так как позиция имеет приоритет.
            const auto &val = current->as_pair()->car;
            args.unnamed.push_back(val); // Сохраняем под именем из спецификации
            current = &current->as_pair()->cdr;
        } else {
            // Данные в вызове закончились. Проверяем, является ли аргумент опциональным.
            if (p_spec.is_optional) {
                // Аргумент опциональный — подставляем дефолтное значение
                args.unnamed.push_back(p_spec.default_value);
            } else {
                // Аргумент обязательный, но данных нет — это ошибка
                throw_eval_error(form, fmt::format("Not enough arguments. Required positional "
                                                   "argument '{}' is missing in {}.",
                                                   p_spec.name, spec.print()));
            }
        }
    }

    // 2. Теперь обрабатываем то, что осталось (Keyword или Rest)
    while (!current->is_null()) {
        const auto &arg = current->as_pair()->car;

        auto is_keyword = arg.is_keyword();
        // Если функция ждет именованные аргументы (&key) и мы встретили ключевое слово
        if (is_keyword && !spec.named.empty()) {
            auto key_name = std::string(arg.as_symbol().name_ptr + 1);

            // Проверка на валидность ключа
            const auto &it = spec.named.find(key_name);
            if (it == spec.named.end()) {
                // Если разрешены keys, можно игнорировать или класть в rest.
                // Но обычно неизвестный &key — это ошибка.
                throw_eval_error(form, fmt::format("Unknown key argument: {} in {}", key_name,
                                                   spec.print_full()));
            }

            if (args.named.count(key_name)) {
                throw_eval_error(form, fmt::format("Key argument: {} multiply defined in {}",
                                                   key_name, spec.print_full()));
            }

            // Переходим к значению
            current = &current->as_pair()->cdr;
            if (current->is_null()) {
                throw_eval_error(form, fmt::format("Key {} is missing a value in {}", key_name,
                                                   spec.print_full()));
            }

            args.named[key_name] = current->as_pair()->car;
        } else {
            // Если это не ключевое слово или функция не ждет &key
            if (!spec.rest.empty() || spec.varargs) {
                args.rest = *current;
                break;
            } else {
                throw_eval_error(form,
                                 fmt::format("Too many arguments (no &rest or &key expected) in {}",
                                             spec.print_full()));
            }
        }
        current = &current->as_pair()->cdr;
    }

    // 3. Заполнение дефолтных значений для пропущенных &key
    for (const auto &[name, param_spec] : spec.named) {
        if (args.named.find(name) == args.named.end()) {
            if (param_spec.has_default) {
                args.named[name] = param_spec.default_value;
            } else {
                throw_eval_error(form, fmt::format("Required key argument {} is missing in {}",
                                                   name, spec.print_full()));
            }
        }
    }

    return args;
}

/*!
 * Same as get_args, but named :key arguments are not parsed.
 */
Arguments Interpreter::get_args_no_named(const Object &form, const Object &rest,
                                         const ArgumentSpec &spec) {
    Arguments args;

    // Check expected key args, which should be none
    if (!spec.named.empty()) {
        throw_eval_error(form, "key arguments were expected in get_args_no_named");
    }

    // loop over forms in list
    Object current = rest;
    while (!current.is_null()) {
        auto arg = current.as_pair()->car;

        // not a keyword. Add to unnamed or rest, depending on what we expect
        if (spec.varargs || args.unnamed.size() < spec.unnamed.size()) {
            args.unnamed.push_back(arg);
        } else {
            args.rest = current;
            break;
        }
        current = current.as_pair()->cdr;
    }

    // Check argument size, if spec defines it
    if (!spec.varargs) {
        if (args.unnamed.size() < spec.unnamed.size()) {
            throw_eval_error(form, "didn't get enough arguments");
        }
        ASSERT(args.unnamed.size() == spec.unnamed.size());

        if (!args.rest_empty() && spec.rest.empty()) {
            throw_eval_error(form, "got too many arguments");
        }
    }

    return args;
}

/*!
 * Evaluate arguments in-place in the given environment.
 * Evaluation order is:
 *  - unnamed, in order of appearance
 *  - keyword, in alphabetical order
 *  - rest, in order of appearance
 *
 * Note that in varargs mode, all unnamed arguments are put in unnamed, not rest.
 */
void Interpreter::eval_args(const Object &parent_form, Arguments *args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    for (auto &arg : args->unnamed) {
        arg = eval_with_rewind(parent_form, arg, env);
    }

    for (auto &kv : args->named) {
        kv.second = eval_with_rewind(parent_form, kv.second, env);
    }

    // 3. REST - САМОЕ ВАЖНОЕ
    if (args->rest.is_pair()) {
        Object  old_rest = args->rest;
        Object  new_head = Object::make_null();
        Object *current_tail = &new_head;

        Object current = old_rest;
        while (current.is_pair()) {
            auto *pair = current.as_pair();

            // ВЫЧИСЛЯЕМ значение
            Object evaluated_val = eval_with_rewind(parent_form, pair->car, env);

            // СОЗДАЕМ НОВУЮ ЯЧЕЙКУ (не трогаем старую!)
            *current_tail = Object::make_pair(evaluated_val, Object::make_null());

            // Шагаем дальше
            current_tail = &((*current_tail).as_pair()->cdr);
            current = pair->cdr;
        }
        // Заменяем rest в аргументах на новый, "вычисленный" список
        args->rest = new_head;
    }
}

/*!
 * Parse argument spec found in lambda/macro definition.
 * Like (x y &key z &key (w my-default-value) &rest body)
 */
ArgumentSpec Interpreter::parse_arg_spec(const Object &form, Object &rest) {
    ArgumentSpec spec;
    bool         parsing_keys = false;
    bool         parsing_optional = false;
    Object       current = rest;

    while (!current.is_null()) {
        if (!current.is_pair()) {
            throw_eval_error(form, "argument spec must be a list");
        }
        auto arg = current.as_pair()->car;

        // 1. Пытаемся понять, не встретили ли мы спец-символ (&key или &rest)
        std::string arg_name = "";
        bool        is_sym = arg.is_symbol();
        if (is_sym) {
            arg_name = arg.as_symbol().name_ptr ? arg.as_symbol().name_ptr : "";
        }

        // 2. Логика переключения режимов
        if (is_sym && arg_name == "&rest") {
            parsing_optional = false;
            parsing_keys = false;
            current = current.as_pair()->cdr;
            if (!current.is_pair())
                throw_eval_error(form, "rest arg must have a name");

            auto rest_name_obj = current.as_pair()->car;
            if (!rest_name_obj.is_symbol())
                throw_eval_error(form, "rest name must be a symbol");

            spec.rest = rest_name_obj.as_symbol().name_ptr;
            if (!current.as_pair()->cdr.is_null())
                throw_eval_error(form, "rest must be the last argument");
            break;
        }

        if (is_sym && arg_name == "&key") {
            parsing_optional = false;
            parsing_keys = true;
            spec.keys = true;
            current = current.as_pair()->cdr;
            continue; // Идем к следующему элементу после "&key"
        }

        if (is_sym && arg_name == "&optional") {
            parsing_optional = true;
            parsing_keys = false;
            spec.keys = false;
            current = current.as_pair()->cdr;
            continue; // Идем к следующему элементу после "&key"
        }

        // 3. Обработка самого аргумента в зависимости от режима
        if (parsing_keys) {
            std::string key_arg_name;
            NamedArg    na;

            if (arg.is_symbol()) {
                // Случай: &key b
                key_arg_name = arg.as_symbol().name_ptr;
            } else if (arg.is_pair()) {
                // Случай: &key (b 1)
                auto key_iter = arg;               // (b 1)
                auto kn = key_iter.as_pair()->car; // b
                if (!kn.is_symbol())
                    throw_eval_error(form, "key name must be a symbol");
                key_arg_name = kn.as_symbol().name_ptr;

                auto val_part = key_iter.as_pair()->cdr; // (1)
                if (val_part.is_pair()) {
                    na.has_default = true;
                    na.default_value = val_part.as_pair()->car; // 1
                }
            } else {
                throw_eval_error(form, "invalid key argument");
            }

            if (spec.named.count(key_arg_name)) {
                throw_eval_error(form,
                                 fmt::format("key argument {} multiply defined", key_arg_name));
            }
            spec.named[key_arg_name] = na;
        } else if (parsing_optional) {
            PositionalArg opt_arg;

            if (arg.is_symbol()) {
                // Случай: &key b
                opt_arg.is_optional = true;
                opt_arg.name = arg.as_symbol().name_ptr;
                opt_arg.default_value = get_null();
            } else if (arg.is_pair()) {
                // Случай: &optional (b 1)
                auto kn = arg.as_pair()->car;
                if (!kn.is_symbol())
                    throw_eval_error(form, "optional name must be a symbol");

                opt_arg.name = kn.as_symbol().name_ptr;
                opt_arg.is_optional = true;

                auto val_list = arg.as_pair()->cdr;
                if (val_list.is_pair()) {
                    // Если есть второй элемент — это и есть наше default_value
                    opt_arg.default_value = val_list.as_pair()->car;
                }
            } else {
                throw_eval_error(form, "invalid optional argument " + arg.print());
            }
            spec.unnamed.push_back(opt_arg);
        } else {
            // Обычный позиционный аргумент
            if (!is_sym)
                throw_eval_error(form, "positional args must be symbols");
            spec.unnamed.push_back({.name = arg_name, .is_optional = false});
        }

        current = current.as_pair()->cdr;
    }
    // fmt::print("DEBUG: ArgSpec {}\n", spec.print_full());
    return spec;
}

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
 *   {{Type::INT, Type::FLOAT}} - One of two types
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
    const Object &form, const Arguments &args, const std::vector<std::vector<ObjectType>> &unnamed,
    const std::unordered_map<std::string, std::pair<bool, std::vector<ObjectType>>> &named) {

    // 1. Проверка unnamed аргументов
    if (!unnamed.empty()) {
        if (args.unnamed.size() != unnamed.size()) {
            throw_arity_mismatch(form, unnamed.size(), args.unnamed.size(), args);
        }

        for (size_t i = 0; i < unnamed.size(); ++i) {
            const auto &allowed_types = unnamed[i];
            if (allowed_types.empty())
                continue;

            bool type_ok = false;
            for (auto type : allowed_types) {
                if (args.unnamed[i].type == type) {
                    type_ok = true;
                    break;
                }
            }

            if (!type_ok) {
                throw_type_mismatch(form, i, allowed_types, args.unnamed[i].type, args);
            }
        }
    }

    // 2. Проверка named аргументов
    for (const auto &[name, spec] : named) {
        auto        it = args.named.find(name);
        bool        required = spec.first;
        const auto &allowed_types = spec.second;

        if (required && it == args.named.end()) {
            throw_missing_named_arg(form, name, args);
        }

        if (it != args.named.end() && !allowed_types.empty()) {
            bool type_ok = false;
            for (auto type : allowed_types) {
                if (it->second.type == type) {
                    type_ok = true;
                    break;
                }
            }
            if (!type_ok) {
                throw_named_type_mismatch(form, name, allowed_types, it->second.type);
            }
        }
    }

    // 3. Проверка лишних named
    for (const auto &[name, _] : args.named) {
        if (named.find(name) == named.end()) {
            throw_unexpected_named_arg(form, name, args);
        }
    }
}

// ============================================================
// Системные функции(print, pprint, inspect)
// ============================================================

Object Interpreter::eval_print(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});

    if (!m_disable_printing) {
        printf("%s\n", args.unnamed.at(0).printc().c_str());
        // printf("%s %s\n", args.unnamed.at(0).printc().c_str(), form.print());
    }
    return Object::make_null();
}

Object Interpreter::eval_pfmt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}},
                 {{"width", {false, {ObjectType::INTEGER}}}});
    auto width = 100;
    if (args.has_named("width"))
        width = args.named["width"].as_integer();

    if (args.unnamed[0].as_symbol() == "#t") {
        const auto &str = pretty_print::to_string(args.unnamed.at(1), width);
        fmt::print("{}\n", str);
        return get_null();
    }
    return Object::make_string(pretty_print::to_string(args.unnamed.at(1), width));
}

Object Interpreter::eval_inspect(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем, что передан хотя бы один аргумент для инспекции
    vararg_check(form, args, {{}}, {});

    // Получаем объект, который нужно проинспектировать
    const Object &target = args.unnamed.at(0);

    // Вызываем метод inspect, который теперь (благодаря нашим правкам)
    // возвращает структуру данных (List/Pair), а не строку.
    return target.inspect();
}

Object Interpreter::eval_fmt(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
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
        auto &arg = args.unnamed.at(i);
        if (arg.is_string()) {
            arg_store.push_back(arg.as_string()->data);
        } else if (arg.is_symbol()) { // Добавляем обработку целых чисел
            arg_store.push_back(arg.as_symbol().c_str());
        } else if (arg.is_integer()) { // Если есть float/double
            arg_store.push_back(arg.as_integer());
        } else if (arg.is_float()) { // Если есть float/double
            arg_store.push_back(arg.as_float());
        } else {
            arg_store.push_back(arg.print());
        }
    }

    auto formatted = fmt::vformat(format_str.as_string()->data, arg_store);
    if (truthy(dest)) {
        lg::print("{}", formatted.c_str());
        return get_null();
    }

    return Object::make_string(formatted);
}

// Вспомогательная функция для сопоставления символа/строки с цветом fmt
fmt::terminal_color string_to_color(const std::string &name) {
    static const std::unordered_map<std::string, fmt::terminal_color> colors = {
        {"red", fmt::terminal_color::red},         {"green", fmt::terminal_color::green},
        {"yellow", fmt::terminal_color::yellow},   {"blue", fmt::terminal_color::blue},
        {"magenta", fmt::terminal_color::magenta}, {"cyan", fmt::terminal_color::cyan},
        {"white", fmt::terminal_color::white},     {"gray", fmt::terminal_color::bright_black}};
    auto it = colors.find(name);
    return (it != colors.end()) ? it->second : fmt::terminal_color::white;
}

// (cfmt #t 'red "format" arguments )
Object Interpreter::eval_cfmt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (args.unnamed.size() < 3) {
        throw_eval_error(form, "cfmt requires at least destination, color and format-string");
    }

    auto dest = args.unnamed.at(0);

    // Обработка цвета из ключевых аргументов (например, :color "red")
    fmt::terminal_color text_color = fmt::terminal_color::white;
    auto                it_color = args.unnamed.at(1);
    if (it_color.is_string()) {
        text_color = string_to_color(it_color.as_string()->data);
    } else if (it_color.is_symbol()) {
        text_color = string_to_color(it_color.as_symbol().c_str());
    } else {
        throw_eval_error(form, "cfmt color as string or symbol");
    }

    auto format_str = args.unnamed.at(2);
    if (!format_str.is_string()) {
        throw_eval_error(form, "cfmt: format string must be a string");
    }

    // Собираем аргументы (как в твоем fmt)
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 3; i < args.unnamed.size(); i++) {
        auto &arg = args.unnamed.at(i);
        if (arg.is_string()) {
            arg_store.push_back(arg.as_string()->data);
        } else if (arg.is_symbol()) { // Добавляем обработку целых чисел
            arg_store.push_back(arg.as_symbol().c_str());
        } else if (arg.is_integer()) { // Если есть float/double
            arg_store.push_back(arg.as_integer());
        } else if (arg.is_float()) { // Если есть float/double
            arg_store.push_back(arg.as_float());
        } else {
            arg_store.push_back(arg.print());
        }
    }

    // Форматируем строку
    auto formatted = fmt::vformat(format_str.as_string()->data, arg_store);

    // Если dest не ложь, выводим в консоль с цветом
    if (truthy(dest)) {
        fmt::print(fg(text_color), "{}", formatted);
    }

    return Object::make_string(formatted);
}

/**
 * (error <message-string> [object])
 * Сигнализирует об ошибке. Если передан второй аргумент,
 * система диагностики попытается подсветить его координаты.
 */
Object Interpreter::eval_error(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    // Проверяем аргументы:
    // 1-й обязательно STRING.
    // 2-й опционально ЛЮБОЙ (поэтому пустые скобки {} во втором векторе)
    vararg_check(form, args, {{ObjectType::STRING}, {}}, {});

    std::string message = args.unnamed.at(0).as_string()->data;

    // Если передан второй аргумент, используем его как "место преступления"
    // Иначе используем 'form' (всю строку вызова (error ..))
    Object context_form = (args.unnamed.size() > 1) ? args.unnamed.at(1) : form;

    // Вызываем стандартный механизм исключений с учетом контекста
    throw_eval_error(context_form, message);

    return Object::make_null(); // Сюда мы никогда не дойдем
}

// ============================================================
// Математические функции с проверками
// ============================================================

Object Interpreter::eval_plus(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        return Object::make_integer(0);
    }

    // Проверяем что все аргументы - числа
    for (const auto &arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "+ requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 0;
        for (const auto &arg : args.unnamed) {
            result += number_to_integer(arg);
        }
        return Object::make_integer(result);
    } else {
        FloatType result = 0.0;
        for (const auto &arg : args.unnamed) {
            result += number_to_float(arg);
        }
        return Object::make_float(result);
    }
}

Object Interpreter::eval_minus(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "- must receive at least one unnamed argument!");
    }

    // Проверяем что все аргументы - числа
    for (const auto &arg : args.unnamed) {
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
    } else {
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

Object Interpreter::eval_times(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    if (!args.named.empty() || args.unnamed.empty()) {
        return Object::make_integer(1);
    }

    // Проверяем что все аргументы - числа
    for (const auto &arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "* requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        IntType result = 1;
        for (const auto &arg : args.unnamed) {
            result *= number_to_integer(arg);
        }
        return Object::make_integer(result);
    } else {
        FloatType result = 1.0;
        for (const auto &arg : args.unnamed) {
            result *= number_to_float(arg);
        }
        return Object::make_float(result);
    }
}

Object Interpreter::eval_divide(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});

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

Object Interpreter::eval_abs(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
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
    } else {
        double val = args.unnamed[0].as_float();
        return Object::make_float(val < 0 ? -val : val);
    }
}

Object Interpreter::eval_max(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "max must receive at least one unnamed argument!");
    }

    if (args.unnamed.empty()) {
        throw_eval_error(form, "max requires at least one argument");
    }

    // Проверяем что все аргументы - числа
    for (const auto &arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "max requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        int64_t max_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            int64_t val = number_to_integer(args.unnamed[i]);
            if (val > max_val)
                max_val = val;
        }
        return Object::make_integer(max_val);
    } else {
        double max_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            double val = number_to_float(args.unnamed[i]);
            if (val > max_val)
                max_val = val;
        }
        return Object::make_float(max_val);
    }
}

Object Interpreter::eval_min(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "min must receive at least one unnamed argument!");
    }

    if (args.unnamed.empty()) {
        throw_eval_error(form, "min requires at least one argument");
    }

    // Проверяем что все аргументы - числа
    for (const auto &arg : args.unnamed) {
        if (!is_number(arg)) {
            throw_eval_error(form, "min requires number arguments");
        }
    }

    if (args.unnamed[0].is_integer()) {
        int64_t min_val = args.unnamed[0].as_integer();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            int64_t val = number_to_integer(args.unnamed[i]);
            if (val < min_val)
                min_val = val;
        }
        return Object::make_integer(min_val);
    } else {
        double min_val = args.unnamed[0].as_float();
        for (size_t i = 1; i < args.unnamed.size(); ++i) {
            double val = number_to_float(args.unnamed[i]);
            if (val < min_val)
                min_val = val;
        }
        return Object::make_float(min_val);
    }
}

Object Interpreter::eval_expt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});

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
    } else {
        double base = number_to_float(args.unnamed[0]);
        double exponent = number_to_float(args.unnamed[1]);
        return Object::make_float(std::pow(base, exponent));
    }
}

Object Interpreter::eval_sqrt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент

    if (!is_number(args.unnamed[0])) {
        throw_eval_error(form, "sqrt requires a number argument");
    }

    double val = number_to_float(args.unnamed[0]);
    if (val < 0) {
        throw_eval_error(form, "sqrt: negative argument");
    }

    return Object::make_float(std::sqrt(val));
}

Object Interpreter::eval_ash(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});
    auto val = number_to_integer(args.unnamed.at(0));
    auto sa = number_to_integer(args.unnamed.at(1));
    if (sa >= 0 && sa < 64) {
        return Object::make_integer(val << sa);
    } else if (sa > -64) {
        return Object::make_integer(val >> -sa);
    } else {
        throw_eval_error(form, fmt::format("Shift amount {} is out of range", sa));
    }
    return Object::make_null();
}

// --- Floor / Ceiling / Round ---
Object Interpreter::eval_floor(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::floor(args.unnamed[0].as_float())));
}

Object Interpreter::eval_ceiling(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::ceil(args.unnamed[0].as_float())));
}

Object Interpreter::eval_round(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::round(args.unnamed[0].as_float())));
}

// --- Modulo (целочисленный) ---
Object Interpreter::eval_mod(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER}, {ObjectType::INTEGER}}, {});

    int64_t a = args.unnamed[0].as_integer();
    int64_t b = args.unnamed[1].as_integer();
    if (b == 0)
        throw_eval_error(form, "mod: division by zero");

    return Object::make_integer(a % b);
}

// --- Trigonometry ---
Object Interpreter::eval_sin(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
    return Object::make_float(std::sin(args.unnamed[0].as_float()));
}

Object Interpreter::eval_cos(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
    return Object::make_float(std::cos(args.unnamed[0].as_float()));
}

Object Interpreter::eval_atan(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    size_t num_args = args.unnamed.size();

    if (num_args == 1) {
        vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});
        return Object::make_float(std::atan(args.unnamed[0].as_float()));
    } else if (num_args == 2) {
        vararg_check(
            form, args,
            {{ObjectType::INTEGER, ObjectType::FLOAT}, {ObjectType::INTEGER, ObjectType::FLOAT}},
            {});
        return Object::make_float(
            std::atan2(args.unnamed[0].as_float(), args.unnamed[1].as_float()));
    } else {
        throw_eval_error(form, fmt::format("atan: expected 1 or 2 arguments, got {}", num_args));
        return get_null();
    }
}

Object Interpreter::eval_tan(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Тангенс всегда принимает ровно один аргумент (угол в радианах)
    vararg_check(form, args, {{ObjectType::INTEGER, ObjectType::FLOAT}}, {});

    return Object::make_float(std::tan(args.unnamed[0].as_float()));
}

Object Interpreter::eval_pi(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // pi вызывается как (pi)
    return Object::make_float(M_PI);
}

// ============================================================
// Bit operations
// ============================================================

// (logand n1 n2 ..)
Object Interpreter::eval_logand(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)args;
    (void)env;
    if (args.unnamed.empty())
        return Object::make_integer(-1); // Нейтральный элемент для AND

    long result = number_to_integer(args.unnamed.at(0));
    for (size_t i = 1; i < args.unnamed.size(); ++i) {
        result &= number_to_integer(args.unnamed.at(i));
    }
    return Object::make_integer(result);
}

// (logior n1 n2 ..)
Object Interpreter::eval_logior(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    long result = 0;
    for (const auto &arg : args.unnamed) {
        result |= number_to_integer(arg);
    }
    return Object::make_integer(result);
}

// (logxor n1 n2 ..)
Object Interpreter::eval_logxor(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    if (args.unnamed.empty())
        return Object::make_integer(0);

    long result = number_to_integer(args.unnamed.at(0));
    for (size_t i = 1; i < args.unnamed.size(); ++i) {
        result ^= number_to_integer(args.unnamed.at(i));
    }
    return Object::make_integer(result);
}

// (lognot n)
Object Interpreter::eval_lognot(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Ожидаем ровно один аргумент
    auto val = number_to_integer(args.unnamed.at(0));
    return Object::make_integer(~val);
}

Object Interpreter::eval_lshift(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER}, {ObjectType::INTEGER}}, {});

    auto val = args.unnamed.at(0).as_integer();
    auto sa = args.unnamed.at(1).as_integer();

    // Логический сдвиг влево на отрицательное число — это нонсенс,
    // поэтому мы просто возвращаем 0 или кидаем ошибку.
    if (sa < 0)
        return Object::make_integer(0);
    if (sa >= 64)
        return Object::make_integer(0);

    return Object::make_integer(val << sa);
}

Object Interpreter::eval_rshift(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER}, {ObjectType::INTEGER}}, {});

    auto val = args.unnamed.at(0).as_integer();
    auto sa = args.unnamed.at(1).as_integer();

    // Логический сдвиг влево на отрицательное число — это нонсенс,
    // поэтому мы просто возвращаем 0 или кидаем ошибку.
    if (sa < 0)
        return Object::make_integer(0);
    if (sa >= 64)
        return Object::make_integer(0);

    return Object::make_integer(val >> sa);
}

// ============================================================
// Функции сравнения с проверками
// ============================================================

Object Interpreter::eval_numequals(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return true_or_false(a_val == b_val);
}

Object Interpreter::eval_lt(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "< requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return true_or_false(a_val < b_val);
}

Object Interpreter::eval_gt(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "> requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return true_or_false(a_val > b_val);
}

Object Interpreter::eval_leq(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, "<= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return true_or_false(a_val <= b_val);
}

Object Interpreter::eval_geq(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два числа

    if (!is_number(args.unnamed[0]) || !is_number(args.unnamed[1])) {
        throw_eval_error(form, ">= requires number arguments");
    }

    FloatType a_val = number_to_float(args.unnamed[0]);
    FloatType b_val = number_to_float(args.unnamed[1]);

    return true_or_false(a_val >= b_val);
}

// ============================================================
// Функции работы со списками с проверками
// ============================================================

Object Interpreter::eval_cons(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {}); // Два любых аргумента
    return Object::make_pair(args.unnamed[0], args.unnamed[1]);
}

Object Interpreter::eval_car(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}}, {}); // Один pair

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "car requires a pair argument");
    }
    return args.unnamed[0].as_pair()->car;
}

Object Interpreter::eval_cdr(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}}, {}); // Один pair

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "cdr requires a pair argument");
    }
    return args.unnamed[0].as_pair()->cdr;
}

Object Interpreter::eval_list_func(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {}, {});

    Object result = Object::make_null();
    for (auto it = args.unnamed.rbegin(); it != args.unnamed.rend(); ++it) {
        result = Object::make_pair(*it, result);
    }
    return result;
}

Object Interpreter::eval_length(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент (список)

    Object lst = args.unnamed[0];
    int    count = 0;

    while (lst.is_pair()) {
        count++;
        lst = lst.as_pair()->cdr;
    }

    if (!lst.is_null()) {
        throw_eval_error(form, "length requires a proper list");
    }

    return Object::make_integer(count);
}

Object Interpreter::eval_append(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    if (args.unnamed.empty()) {
        return Object::make_null();
    }

    Object result = args.unnamed.back();

    for (int i = args.unnamed.size() - 2; i >= 0; --i) {
        Object current = args.unnamed[i];

        Object reversed = Object::make_null();
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

// ============================================================
// Предикаты типов с проверками
// ============================================================

Object Interpreter::eval_bound_p(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});
    auto   sym = args.unnamed.at(0);
    Object result;
    // Ищем символ в текущем и родительских окружениях
    if (try_symbol_lookup(sym, env, &result)) {
        return get_true(); // Твой #t / T
    }
    return m_obj_false; // Твой #f / NIL
}

Object Interpreter::eval_type_of(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});
    auto obj = args.unnamed[0];
    if (obj.is_hash_table()) {
        auto table = obj.as_hash_table();
        if (!table->type.is_none())
            return table->type;
    }
    return m_symbol_table.object_type_to_symbol(args.unnamed[0].type);
}

Object Interpreter::eval_type_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {ObjectType::SYMBOL}}, {});

    auto type_name = args.unnamed[1].as_symbol().name_ptr;
    auto kv = m_string_to_type.find(type_name);
    if (kv == m_string_to_type.end()) {
        throw_eval_error(form, fmt::format("invalid type name: {}", type_name));
    }

    return true_or_false(args.unnamed[0].type == kv->second);
}

Object Interpreter::eval_null_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_null());
}

Object Interpreter::eval_pair_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_pair());
}

Object Interpreter::eval_symbol_p(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_symbol());
}

Object Interpreter::eval_integer_p(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_integer());
}

Object Interpreter::eval_float_p(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_float());
}

Object Interpreter::eval_number_p(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_integer() || args.unnamed[0].is_float());
}

Object Interpreter::eval_string_p(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_string());
}

Object Interpreter::eval_char_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_char());
}

Object Interpreter::eval_vector_p(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_array());
}

Object Interpreter::eval_procedure_p(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка аргументов (ожидаем ровно один)
    vararg_check(form, args, {{}}, {});

    const Object &target = args.unnamed[0];

    // Теперь процедурой считается:
    // 1. Лямбда (пользовательская функция)
    // 2. Макрос
    // 3. Любой объект, наследующий CallableObject (наши новые примитивы и спецформы)
    bool is_proc = target.is_lambda() || target.is_macro() ||
                   target.is_callable(); // Проверка на SPECIAL_FORM или PRIMITIVE

    return true_or_false(is_proc);
}

Object Interpreter::eval_boolean_p(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    const Object &obj = args.unnamed[0];
    bool          is_bool = (obj.is_symbol() && obj.as_symbol().name_ptr &&
                    (strcmp(obj.as_symbol().name_ptr, "#t") == 0 ||
                     strcmp(obj.as_symbol().name_ptr, "#f") == 0));
    return true_or_false(is_bool);
}

Object Interpreter::eval_reader_p(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_reader());
}

Object Interpreter::eval_cell_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_pointer());
}

Object Interpreter::eval_special_form_p(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_pointer());
}

Object Interpreter::eval_primitive_p(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_pointer());
}

// ============================================================
// Apply
// ============================================================
Object Interpreter::eval_apply(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Базовая проверка (нам нужно минимум 2 аргумента: функция и список)
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {});

    Object callable_obj = args.unnamed[0];
    Object args_list = args.unnamed[1];

    // 2. Если первым аргументом пришел символ (например, '+), резолвим его
    if (callable_obj.is_symbol()) {
        callable_obj = eval_with_rewind(form, callable_obj, env);
    }

    // 3. Превращаем Lisp-список (1 2 3) в структуру Arguments
    // Внимание: аргументы для apply уже считаются вычисленными!
    Arguments applied_args;
    for (Object it = args_list; it.is_pair(); it = it.as_pair()->cdr) {
        applied_args.unnamed.push_back(it.as_pair()->car);
    }

    // 4. Диспетчеризация вызова

    // СЛУЧАЙ А: Это наш новый пассивный примитив (BuiltinFunctionObject)
    if (callable_obj.is_primitive()) {
        auto bf = callable_obj.as_native_ref<BuiltinFunctionObject>();
        // ВАЖНО: Мы не вызываем eval_args, так как apply подразумевает,
        // что данные в списке уже готовы к употреблению.
        return (this->*(bf->method))(form, applied_args, env);
    }

    // СЛУЧАЙ Б: Это обычная Лямбда
    if (callable_obj.is_lambda()) {
        const auto &lam = callable_obj.as_lambda();
        auto        lam_env = EnvironmentObject::make_new().as_env_ptr();
        lam_env->parent_env = lam->parent_env;

        set_args_in_env(form, applied_args, lam->args, lam_env);
        return eval_list_return_last(lam->body, lam->body, lam_env);
    }

    // СЛУЧАЙ В: Попытка применить спецформу (обычно запрещено, но на твой вкус)
    if (callable_obj.is_special_form()) {
        throw_eval_error(form, "apply: cannot apply a special form (like 'if' or 'define')");
    }

    throw_eval_error(form, "apply: first argument is not a procedure");
    return m_sym_null;
}
// ============================================================
// Функции сравнения
// ============================================================

Object Interpreter::eval_equals(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});
    return true_or_false(args.unnamed[0] == args.unnamed[1]);
}

// ============================================================
// Строковые функции с проверками
// ============================================================

Object Interpreter::eval_string_length(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка
    return Object::make_integer(args.unnamed[0].as_string()->length());
}

Object Interpreter::eval_string_ref(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::INTEGER}}, {}); // Строка и индекс

    const std::string &str = args.unnamed[0].as_string()->data;
    int64_t            index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(str.length())) {
        throw_eval_error(form, "string-ref: index out of range");
    }

    return Object::make_char(str[index]);
}

Object Interpreter::eval_string_append(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {}, {});

    std::string result;
    for (const auto &arg : args.unnamed) {
        if (!arg.is_string()) {
            throw_eval_error(form, "string-append requires string arguments got " + arg.print());
        }
        result += arg.as_string()->data;
    }
    return Object::make_string(result);
}

Object Interpreter::eval_string_substr(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::INTEGER}, {ObjectType::INTEGER}},
                 {}); // Строка, начало, конец

    const std::string &str = args.unnamed[0].as_string()->data;
    int64_t            start = args.unnamed[1].as_integer();
    int64_t            end = args.unnamed[2].as_integer();

    if (start < 0 || end > static_cast<int64_t>(str.length()) || start > end) {
        throw_eval_error(form, "substring: invalid start or end index");
    }

    return Object::make_string(str.substr(start, end - start));
}

Object Interpreter::eval_string_replace(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}, {ObjectType::STRING}},
                 {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &from = args.unnamed.at(1).as_string()->data;
    auto &to = args.unnamed.at(2).as_string()->data;
    str_util::replace(str, from, to);
    return Object::make_string(str);
}

Object Interpreter::eval_string_containsp(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &suffix = args.unnamed.at(1).as_string()->data;

    if (str_util::contains(str, suffix)) {
        return get_true();
    }
    return m_obj_false;
}

Object Interpreter::eval_string_starts_with(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &suffix = args.unnamed.at(1).as_string()->data;

    if (str_util::starts_with(str, suffix)) {
        return get_true();
    }
    return m_obj_false;
}

Object Interpreter::eval_string_ends_with(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &suffix = args.unnamed.at(1).as_string()->data;

    if (str_util::ends_with(str, suffix)) {
        return get_true();
    }
    return m_obj_false;
}

Object Interpreter::eval_string_split(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &delim = args.unnamed.at(1).as_string()->data;
    auto  list = str_util::split(str, delim.at(0));
    return pretty_print::build_list(list);
}

Object Interpreter::eval_string_to_symbol(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка
    return make_symbol(args.unnamed[0].as_string()->data);
}

Object Interpreter::eval_symbol_to_string(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {}); // Один символ
    return Object::make_string(
        args.unnamed[0].as_symbol().name_ptr ? args.unnamed[0].as_symbol().name_ptr : "");
}

// ============================================================
// Векторные функции с проверками
// ============================================================

Object Interpreter::eval_vector(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {}, {}); // Любое количество элементов
    return Object::make_array(args.unnamed);
}

Object Interpreter::eval_vector_ref(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::INTEGER}}, {}); // Вектор и индекс

    auto    elements = args.unnamed[0].as_array();
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(elements->size())) {
        throw_eval_error(form, "vector-ref: index out of range");
    }

    return elements->get(index);
}

Object Interpreter::eval_vector_set(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::INTEGER}, {}},
                 {}); // Вектор, индекс, значение

    auto array = args.unnamed[0].as_array();

    int64_t index = args.unnamed[1].as_integer();
    if (index < 0 || index >= array->size()) {
        throw_eval_error(form, "vector-set!: index out of range");
    }

    array->set(index, args.unnamed[2]);
    return args.unnamed[2];
}

Object Interpreter::eval_vector_length(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}}, {}); // Один вектор
    return Object::make_integer(args.unnamed[0].as_array()->size());
}

Object Interpreter::eval_vector_to_list(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}}, {});

    auto array = args.unnamed[0].as_array();

    // Рекурсивная функция для построения списка
    std::function<Object(int)> build_list = [&](int index) -> Object {
        if (index >= array->size()) {
            return Object::make_null();
        }
        return Object::make_pair(array->get(index), build_list(index + 1));
    };

    return build_list(0);
}

// ============================================================
// Хэш - таблицы с проверками
// ============================================================

const char *get_hash_key(Object item_pair) {
    if (item_pair.is_symbol()) {
        return item_pair.as_symbol().name_ptr;
    } else if (item_pair.is_string()) {
        return item_pair.as_string()->data.c_str();
    } else {
        return nullptr;
    }
}

/**
 * Пустая таблица: (make-hash-table)
 *
 * Только данные (размер будет 8): (make-hash-table '((:HL . 1) (:BC . 2)))
 *
 * Данные и размер: (make-hash-table '((:A . 0)) 100)
 */
Object Interpreter::eval_make_hash_table(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(
        form, args, {},
        {{"size", {false, {ObjectType::INTEGER}}}, {"type", {false, {ObjectType::SYMBOL}}}});

    // Устанавливаем значения по умолчанию
    Object initial_data = Object::make_null();
    if (args.unnamed.size() > 0) {
        if (!args.unnamed[0].is_pair() && !args.unnamed[0].is_null()) {
            throw_type_mismatch(form, 0, {ObjectType::PAIR}, args.unnamed[0].type, args);
        }
        initial_data = args.unnamed[0];
    }
    int size = 8;
    if (args.has_named("size"))
        size = args.named["size"].as_integer();

    Object type_name = Object::make_none();
    if (args.has_named("type"))
        type_name = args.named["type"];

    // Создаем таблицу
    Object table = type_name.is_none() ? Object::make_hash_table(size)
                                       : Object::make_hash_table(type_name, size);
    auto   table_ptr = table.as_hash_table();

    // Заполняем таблицу данными из списка пар
    if (initial_data.is_pair()) {
        Object current = initial_data;
        while (current.is_pair()) {
            auto   current_pair = current.as_pair();
            Object item = current_pair->car;

            if (item.is_pair()) {
                auto item_pair = item.as_pair();

                // Извлекаем ключ (символ, строку или Keyword)
                const char *str = get_hash_key(item_pair->car);

                if (str != nullptr) {
                    // Используем .cdr для точечных пар ((key . val) ...)
                    // Если планируете ((key val) ...), то нужно item_pair->cdr.as_pair()->car
                    table_ptr->data[str] = item_pair->cdr;
                } else {
                    throw_eval_error(form, "Hash table key must be a symbol, keyword or string.");
                }
            } else {
                throw_eval_error(form, "Initial data must contain only pairs.");
            }

            current = current_pair->cdr;
        }
    }

    return table;
}

Object Interpreter::eval_hash_table_set(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::STRING_HASH_TABLE}, {ObjectType::SYMBOL, ObjectType::STRING}, {}},
                 {}); // Таблица, ключ, значение

    auto ht = args.unnamed[0].as_hash_table();
    auto key = args.unnamed.at(1).to_std_string();
    ht->data[key] = args.unnamed.at(2);

    return Object::make_null();
}

Object Interpreter::eval_get_at(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // (get-at target key [default])
    if (args.unnamed.size() < 2 || args.unnamed.size() > 3) {
        throw_eval_error(form, "get-at: expected 2 or 3 arguments");
    }

    Object target = args.unnamed[0];
    Object key = args.unnamed[1];

    if (!target.is_heap_object()) {
        throw_eval_error(form, "get-at: target must be a heap object");
    }

    // Вызываем виртуальный метод объекта
    Object result = target.as_heap_object()->get_at(key);

    // Если ключ не найден (объект вернул undefined)
    if (result.is_none()) {
        // Если есть 3-й аргумент (default) — возвращаем его
        if (args.unnamed.size() == 3) {
            return args.unnamed[2];
        }
        // Иначе — ошибка
        throw_eval_error(form, "get-at: key or index not found");
    }

    return result;
}

Object Interpreter::eval_set_at(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}, {}}, {}); // [цель] [ключ] [значение]

    Object target = args.unnamed[0];
    Object key = args.unnamed[1];
    Object value = args.unnamed[2];

    if (target.is_heap_object()) {
        // Вызываем виртуальный метод.
        // Если это StaticBuffer — запишется в память.
        // Если HashTable — запишется в мапу.
        target.as_heap_object()->set_at(key, value);
    } else {
        throw_eval_error(form, "set-at!: target must be a heap object (buffer, table, etc)");
    }

    return get_null();
}

Object Interpreter::eval_hash_table_ref(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Разрешаем любое кол-во, проверку сделаем сами для гибкости
    if (args.unnamed.size() > 2) {
        vararg_check(
            form, args,
            {{ObjectType::STRING_HASH_TABLE}, {ObjectType::STRING, ObjectType::SYMBOL}, {}}, {});
    } else {
        vararg_check(form, args,
                     {{ObjectType::STRING_HASH_TABLE}, {ObjectType::STRING, ObjectType::SYMBOL}},
                     {});
    }
    auto ht = args.unnamed[0].as_hash_table();
    auto key = args.unnamed.at(1).to_std_string();

    auto it = ht->data.find(key);
    if (it != ht->data.end()) {
        return it->second;
    }

    // Ключ не найден:
    // Если передан 3-й аргумент — возвращаем его
    if (args.unnamed.size() == 3) {
        return args.unnamed[2];
    }

    // Если 3-го аргумента нет — кидаем ошибку, как и раньше
    throw_eval_error(form, "hash-table-ref: key not found: " + std::string(key));
    return get_null();
}

// Try to look up a value by key in a hash table.The result is a pair of(success.value).

Object Interpreter::eval_hash_table_try_ref(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING_HASH_TABLE}, {}}, {});

    const auto *table = args.unnamed.at(0).as_hash_table();

    const char *key = get_hash_key(args.unnamed.at(1));
    if (key != nullptr) {
        const auto &it = table->data.find(key);
        if (it == table->data.end()) {
            // not in table
            return Object::make_pair(get_false(), Object::make_null());
        } else {
            return Object::make_pair(get_true(), it->second);
        }
    } else {
        throw_eval_error(form, "Hash table must use symbol or string as the key.");
    }
    return get_null();
}

Object Interpreter::eval_hash_table_containsp(const Object &form, Arguments &args,
                                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING_HASH_TABLE}, {}}, {});

    const auto *table = args.unnamed.at(0).as_hash_table();

    const char *key = get_hash_key(args.unnamed.at(1));
    if (key != nullptr) {
        const auto &it = table->data.find(key);
        if (it == table->data.end()) {
            // not in table
            return get_false();
        } else {
            return get_true();
        }
    } else {
        throw_eval_error(form, "Hash table must use symbol or string as the key.");
    }
    return get_null();
}

Object Interpreter::eval_hash_table_length(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент

    auto ht = args.unnamed[0].as_hash_table();

    return Object::make_integer(ht->data.size());
}

Object Interpreter::eval_hash_table_to_list(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});

    auto   ht = args.unnamed[0].as_hash_table();
    Object result = Object::make_null();

    // Итерируемся по unordered_map
    for (const auto &[key, value] : ht->data) {
        // Создаем пару (ключ значение)
        Object pair = Object::make_pair(Object::make_string(key), value);
        // Добавляем в начало списка
        result = Object::make_pair(pair, result);
    }

    return result;
}

Object Interpreter::eval_hash_table_p(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_hash_table());
}

// ============================================================
// Системные функции с проверками
// ============================================================

// Читает весь файл как текст.
Object Interpreter::eval_read_str(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    return Object::make_string(file_util::read_text(filename));
}

// Превращает строку в список команд: (top-level .. ). Удобно для eval.
Object Interpreter::eval_parse_str(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка
    return m_reader.read_from_string(args.unnamed[0].as_string()->data, true, "read string");
}

// Читает весь файл как данные, обернутые в top-level.
Object Interpreter::eval_read_file(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    std::string content = file_util::read_text(filename);
    return m_reader.read_from_string(content, true, filename);
}

// Читает и исполняет файл. (Обычно исполняет объекты по одному, top-level не нужен).
Object Interpreter::eval_load(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Одна строка (имя файла)
    Object last_result = get_null();

    if (args.unnamed[0].is_string()) {
        std::vector<std::string> path;
        path.push_back(args.unnamed[0].as_string()->data);
        return eval_file_internal(path);
    } else if (args.unnamed[0].is_pair()) {
        auto strings = args.unnamed[0].as_c_vector_of_strings();
        return eval_file_internal(strings);
    } else {
        throw_eval_error(form, "load requires a string or list of strings");
    }
    return last_result;
}

Object Interpreter::eval_file_exists_p(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя файла)

    std::string   filename = args.unnamed[0].as_string()->data;
    std::ifstream file(filename);
    bool          exists = file.good();
    file.close();

    return true_or_false(exists);
}

// ============================================================
// Системные методы
// ============================================================

Object Interpreter::eval_get_env(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя переменной)

    std::string var_name = args.unnamed[0].as_string()->data;
    const char *value = std::getenv(var_name.c_str());

    if (value) {
        return Object::make_string(value);
    } else {
        return Object::make_null();
    }
}

Object Interpreter::eval_system(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (команда)

    std::string command = args.unnamed[0].as_string()->data;
    int         result = std::system(command.c_str());

    return Object::make_integer(result);
}

Object Interpreter::eval_exit(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER}, {}}, {});

    int code = args.unnamed[0].is_integer();

    throw ExitException(code); // Просто бросаем, не заботясь о возврате
}

Object Interpreter::eval_get_path(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {}); // Один символ
    std::string         sym = args.unnamed[0].as_symbol().name_ptr;
    file_util::PathType select;
    if (sym == "cwd")
        select = file_util::PathType::CWD;
    else if (sym == "exe")
        select = file_util::PathType::EXE;
    else if (sym == "home")
        select = file_util::PathType::HOME;
    else if (sym == "config")
        select = file_util::PathType::CONFIG;
    else if (sym == "cache")
        select = file_util::PathType::CACHE;
    else if (sym == "share")
        select = file_util::PathType::SHARE;
    else if (sym == "project")
        select = file_util::PathType::PROJECT;
    else {
        throw_eval_error(
            form, "get_path requires a symbol: cwd, exe, home, config, cache, share, project");
    }
    return Object::make_string(file_util::get_path(select).string());
}

Object Interpreter::eval_find_file(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (команда)

    std::string path = args.unnamed[0].as_string()->data;
    auto        found = file_util::find_config_file(path);
    return found.empty() ? Object::make_null() : Object::make_string(found.string());
}

Object Interpreter::eval_write_binary_file(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Путь — строка, Данные — любой тип (массив или список)
    vararg_check(form, args, {{ObjectType::STRING}, {}}, {});

    std::string path = args.unnamed[0].as_string()->data;
    Object      data = args.unnamed[1];

    std::ofstream file(path, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        throw_eval_error(form, "Could not open file for writing: " + path);
    }

    // Универсальная итерация
    if (data.is_array()) {
        auto *arr = data.as_array();
        for (int i = 0; i < arr->size(); ++i) {
            file.put(static_cast<char>(arr->get(i).as_integer()));
        }
    } else {
        // Если не массив, то по логике vararg_check и нашей задаче — это список (Pair/Null)
        Object current = data;
        while (current.is_pair()) {
            auto pair = current.as_pair();
            if (pair->car.is_integer())
                file.put(static_cast<char>(pair->car.as_integer()));
            else
                throw_eval_error(form, "Data list has non integer value:: " + pair->car.print());
            current = pair->cdr;
        }
    }

    file.close();
    return get_true();
}

// Gets file path andh the read mode
Object Interpreter::eval_read_binary_file(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {});

    std::string path = args.unnamed[0].as_string()->data;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw_eval_error(form, "Could not open file for writing: " + path);
        return get_false();
    }

    // Определяем размер файла
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<Object> elements;
    elements.reserve(size);

    char byte;
    while (file.get(byte)) {
        // Превращаем каждый байт в объект-число Лиспа
        elements.push_back(Object::make_integer(static_cast<unsigned char>(byte)));
    }

    file.close();
    // Создаем ArrayObject и возвращаем его как Object через Heap
    return Object::make_array(std::move(elements));
}

Object Interpreter::eval_write_text_file(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Ждем две строки: путь и контент
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});

    std::string path = args.unnamed[0].as_string()->data;
    std::string content = args.unnamed[1].as_string()->data;

    std::ofstream file(path); // Текстовый режим по умолчанию
    if (!file.is_open()) {
        throw_eval_error(form, "Could not open text file for writing: " + path);
    }

    file << content;
    file.close();

    return get_true();
}

Object Interpreter::eval_read_text_file(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Ждем один аргумент — путь к файлу
    vararg_check(form, args, {{ObjectType::STRING}}, {});

    std::string path = args.unnamed[0].as_string()->data;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw_eval_error(form, "Could not open text file for reading: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Читаем весь поток в буфер
    file.close();

    // Возвращаем новую строку Лиспа
    return Object::make_string(buffer.str());
}
// ============================================================
// Прочие функции с проверками
// ============================================================

Object Interpreter::eval_gensym(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)args;
    (void)env;
    vararg_check(form, args, {}, {}); // Без аргументов

    std::string name = "gensym" + std::to_string(m_gensym_id++);
    return make_symbol(name.c_str());
}

Object Interpreter::eval_eval(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return eval_with_rewind(form, args.unnamed[0], env);
}

Object Interpreter::eval_set_car(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}, {}}, {}); // Пара и значение

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-car! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->car = args.unnamed[1];
    return args.unnamed[0];
}

Object Interpreter::eval_set_cdr(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}, {}}, {}); // Пара и значение

    if (!args.unnamed[0].is_pair()) {
        throw_eval_error(form, "set-cdr! requires a pair as first argument");
    }
    args.unnamed[0].as_pair()->cdr = args.unnamed[1];
    return args.unnamed[0];
}

// ============================================================
// Функции преобразования типов с проверками
// ============================================================

Object Interpreter::eval_number_to_string(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {{"base", {false, {ObjectType::INTEGER}}}});

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
    } else {
        double num = args.unnamed[0].as_float();
        return Object::make_string(std::to_string(num));
    }
}

Object Interpreter::eval_string_to_number(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}},
                 {{"base", {false, {ObjectType::INTEGER}}}}); // Строка и опционально основание

    std::string str = args.unnamed[0].as_string()->data;
    int         base = 10;

    if (args.has_named("base")) {
        base = args.get_named("base").as_integer();
    }

    try {
        if (str.find('.') != std::string::npos || str.find('e') != std::string::npos) {
            double value = std::stod(str);
            return Object::make_float(value);
        } else {
            if (base == 16 && str.substr(0, 2) == "0x") {
                str = str.substr(2);
            }
            int64_t value = std::stoll(str, nullptr, base);
            return Object::make_integer(value);
        }
    } catch (const std::exception &) {
        return Object::make_null();
    }
}

Object Interpreter::eval_char_to_integer(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::CHAR}}, {}); // Один символ
    return Object::make_integer(static_cast<int64_t>(args.unnamed[0].as_char()));
}

Object Interpreter::eval_integer_to_char(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INTEGER}}, {}); // Одно целое

    int64_t code = args.unnamed[0].as_integer();
    if (code < 0 || code > 255) {
        throw_eval_error(form, "integer->char: code out of range 0-255");
    }

    return Object::make_char(static_cast<char>(code));
}

// ============================================================
// Функции времени
// ============================================================

// time-seconds: возвращает количество секунд с эпохи Unix
Object Interpreter::eval_time_seconds(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // Без аргументов

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    return Object::make_integer(static_cast<int64_t>(seconds));
}

// time-milliseconds: возвращает количество миллисекунд с эпохи Unix
Object Interpreter::eval_time_milliseconds(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {}); // Без аргументов

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    return Object::make_integer(static_cast<int64_t>(milliseconds));
}

// time-microseconds: если нужна еще большая точность
Object Interpreter::eval_time_microseconds(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {});

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

    return Object::make_integer(static_cast<int64_t>(microseconds));
}

// time-nanoseconds: максимальная точность
Object Interpreter::eval_time_nanoseconds(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    vararg_check(form, args, {}, {});

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    return Object::make_integer(static_cast<int64_t>(nanoseconds));
}

// ============================================================
// Macro Character
// ============================================================

Object Interpreter::eval_set_macro_character(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {}}, {});

    std::string shortcut = args.unnamed[0].as_string()->data;

    // Получаем replacement (второй аргумент)
    std::string replacement;
    auto        object = args.unnamed[1];
    if (object.is_string()) {
        replacement = object.as_string()->data;
        m_reader.add_reader_macro(shortcut, replacement, true);
    } else if (object.is_symbol()) {
        const char *sym_name = object.as_symbol().name_ptr;
        replacement = sym_name ? sym_name : "";
        m_reader.add_reader_macro(shortcut, replacement, true);
    } else if (object.is_lambda()) {
        m_reader.add_reader_macro(shortcut, object, false);
    } else {
        throw_eval_error(form, "set-reader-macro: second argument must be string or symbol");
    }

    return Object::make_null();
}

Object Interpreter::eval_remove_macro_character(const Object &form, Arguments &args,
                                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args, {{ObjectType::STRING}}, {});

    std::string shortcut = args.unnamed[0].as_string()->data;

    m_reader.remove_reader_macro(shortcut);

    return Object::make_null();
}

Object Interpreter::eval_get_macro_character(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args, {{ObjectType::STRING}}, {});

    std::string shortcut = args.unnamed[0].as_string()->data;

    auto macro = m_reader.find_reader_macro(shortcut);
    if (macro == nullptr)
        return Object::make_null();
    else if (!macro->lambda.is_null())
        return macro->lambda;
    else
        return Object::make_string(macro->replacement);
}

Object Interpreter::eval_read(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::READER}}, {});

    ReaderObject *reader_obj = args.unnamed[0].as_reader();
    if (!reader_obj->ts) {
        throw_eval_error(form, "read: stream is null");
    }

    // Вызываем чтение одного объекта (чистого, без top-level)
    // Метод read_from_stream должен выполнять get_next_token + read_object + process_macros
    return m_reader.read_single_form(*(reader_obj->ts));
}

Object Interpreter::eval_read_char(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::READER}}, {});

    ReaderObject *reader_obj = args.unnamed[0].as_reader();
    auto          ts = reader_obj->ts;

    if (!ts || !ts->text_remains()) {
        return Object::make_null(); // EOF
    }

    // Используем метод твоего TextStream, который двигает seek и считает строки
    char c = ts->read();
    return Object::make_char(c);
}

Object Interpreter::eval_peek_char(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::READER}}, {});

    ReaderObject *reader_obj = args.unnamed[0].as_reader();
    auto          ts = reader_obj->ts;

    if (!ts || !ts->text_remains()) {
        return Object::make_null(); // EOF
    }

    // Используем твой ts->peek(), который просто берет char по текущему индексу
    char c = ts->peek();
    return Object::make_char(c);
}

Object Interpreter::eval_read_delimited_list(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверяем минимальное количество аргументов (Reader обязателен)
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::READER}}, {});

    // 3. Определяем терминатор (по умолчанию ")")
    std::string terminator = args.unnamed[0].as_string()->data;

    // 2. Извлекаем ReaderObject
    if (!args.unnamed[1].is_reader()) {
        throw_eval_error(form, "read-delimited-list: 2d argument must be a reader object");
    }
    ReaderObject *reader_obj = args.unnamed[1].as_reader();

    // 4. Вызываем РЕАЛЬНЫЙ ридер
    // Предполагается, что у твоего Interpreter есть доступ к экземпляру Reader (напр. m_reader)
    // Мы используем разыменованный TextStream из ReaderObject
    if (!reader_obj->ts) {
        throw_eval_error(form, "read-delimited-list: reader stream is null");
    }

    // ВАЖНО: вызываем метод у объекта Reader, а не у обертки ReaderObject
    return m_reader.read_list(*(reader_obj->ts), false, terminator);
}

std::string Interpreter::get_all_symbols_matching(const std::string &prefix) {
    std::set<std::string> matches;

    // 1. Лямбда для глубокого обхода EnvironmentObject
    auto collect_from_env = [&](const Object &env_obj) {
        if (!env_obj.is_env())
            return;

        // Явно указываем shared_ptr, чтобы типы совпали с parent_env
        std::shared_ptr<EnvironmentObject> current = env_obj.as_env_ptr();

        while (current) {
            const auto &entries = current->vars.get_all_entries();

            for (const auto &e : entries) {
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

    // 2. Собираем из обоих окружений
    collect_from_env(m_global_environment);
    collect_from_env(m_comp_env);

    // 3. Склеиваем в строку для отправки по сети
    std::string result;
    for (const auto &s : matches) {
        if (!result.empty())
            result += " ";
        result += s;
    }
    return result;
}

// ============================================================
// Lex Tokens
// ============================================================

Object Interpreter::eval_source_info(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Ждем один аргумент — любую форму (символ, число или cons-пару)
    vararg_check(form, args, {{}}, {});

    std::optional<ShortInfo> result;

    // Ищем информацию по адресу объекта в памяти
    if (args.unnamed[0].is_lambda()) {
        auto lambda = args.unnamed[0].as_lambda();
        result = get_db().get_short_info_for(lambda->body);
    } else {
        result = get_db().get_short_info_for(args.unnamed[0]);
    }

    if (!result) { // Если форма вычислена динамически и её нет в БД
        return get_null();
    }

    return pretty_print::build_list({
        make_symbol(":file"), Object::make_string(result->filename), make_symbol(":line"),
        Object::make_integer(result->line_idx_to_display), make_symbol(":column"),
        Object::make_integer(result->pos_in_line), make_symbol(":text"),
        Object::make_string(result->line_text) // Полезно для вывода "стрелочки" ^
    });
}

Object Interpreter::eval_get_context(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка аргумента (ожидаем INTEGER)
    vararg_check(form, args, {{ObjectType::INTEGER}}, {});

    int64_t ctx_index = args.unnamed[0].as_integer();
    if (ctx_index < 0) {
        throw_eval_error(form, "context-ref: index cannot be negative");
    }

    // Начинаем с самого верхнего кадра
    ContextFrame *current = m_top_frame;
    int64_t       count = 0;

    // Шагаем вглубь стека
    while (current != nullptr) {
        // Если мы нашли нужный индекс
        if (count == ctx_index) {
            // Возвращаем форму, сохраненную в этом кадре
            return current->form;
        }

        current = current->prev;
        count++;
    }

    // Если индекс за пределами глубины стека, возвращаем null (или можно кинуть ошибку)
    return Object::make_null();
}

// ============================================================
// Macroexpand
// ============================================================

Object Interpreter::eval_macroexpand(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    // Проверяем наличие одного аргумента (формы для раскрытия)
    vararg_check(form, args, {{ObjectType::PAIR}}, {});

    Object code = args.unnamed[0];

    // Макрос — это всегда список вида (имя-макроса ..)
    if (!code.is_pair()) {
        return code;
    }

    const Object &head = code.as_pair()->car;
    const Object &rest = code.as_pair()->cdr;

    Object macro_obj;
    // Используем твой механизм поиска символа
    if (head.is_symbol() && try_symbol_lookup(head, env, &macro_obj) && macro_obj.is_macro()) {
        const auto &macro = macro_obj.as_macro();

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

// ============================================================
// Log
// ============================================================

Object Interpreter::eval_log(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Минимальный вызов: (log 'level "format" ..)
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::STRING}}, {});

    // 1. Определяем уровень лога
    std::string level_name = args.unnamed.at(0).print();
    lg::level   log_level = lg::level::info; // по умолчанию

    if (level_name == "trace")
        log_level = lg::level::trace;
    else if (level_name == "debug")
        log_level = lg::level::debug;
    else if (level_name == "info")
        log_level = lg::level::info;
    else if (level_name == "warn")
        log_level = lg::level::warn;
    else if (level_name == "error")
        log_level = lg::level::error;
    else if (level_name == "die")
        log_level = lg::level::die;

    // 2. Получаем строку формата
    auto format_obj = args.unnamed.at(1);
    if (!format_obj.is_string()) {
        throw_eval_error(form, "log: format must be a string");
    }
    std::string format_str = format_obj.as_string()->data;

    // 3. Собираем аргументы через fmt (KISS)
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 2; i < args.unnamed.size(); i++) {
        const auto &arg = args.unnamed.at(i);
        if (arg.is_string())
            arg_store.push_back(arg.as_string()->data);
        else
            arg_store.push_back(arg.print());
    }

    // 4. Форматируем и отправляем в твой lg::log
    std::string formatted = fmt::vformat(format_str, arg_store);

    // Используем внутренний логгер
    lg::log(log_level, "{}", formatted);

    return Object::make_string(formatted);
}

// ============================================================
// Таблица Setters для Getters
// ============================================================

Object Interpreter::eval_defsetf(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка: нам нужно ровно два аргумента, и оба должны быть символами
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}}, {});

    // Извлекаем InternedSymbolPtr напрямую из объектов
    InternedSymbolPtr getter = args.unnamed.at(0).as_symbol();
    InternedSymbolPtr setter = args.unnamed.at(1).as_symbol();

    // Записываем в нашу unordered_map
    m_setter_map[getter] = setter;

    // Возвращаем имя сеттера как результат выполнения
    return args.unnamed.at(1);
}

Object Interpreter::eval_get_setter(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка: один аргумент-символ
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});

    InternedSymbolPtr getter = args.unnamed.at(0).as_symbol();

    // Ищем в таблице
    auto it = m_setter_map.find(getter);
    if (it != m_setter_map.end()) {
        // Если нашли, создаем объект-символ из сохраненного указателя
        return make_symbol(it->second.c_str());
    }

    // Если ничего не нашли, возвращаем пустой список (nil)
    return Object::make_null();
}

// ============================================================
// Type System
// ============================================================

Object Interpreter::eval_typespec_special(const Object &, const Object &rest,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (rest.is_null())
        return get_null();

    Object spec_input = rest.as_pair()->car;
    auto   ts_ptr = parse_typespec(&TypeSystem::instance(), spec_input);
    auto   ts_shared = std::make_shared<TypeSpec>(ts_ptr);

    return Object::make_native_ref(ts_shared);
}

Object Interpreter::eval_deftype_special(const Object &form, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto env_ptr = get_global_environment().as_env();
    try {
        auto result = parse_deftype(rest, &TypeSystem::instance(), &env_ptr->vars);
        auto type_shared = std::shared_ptr<Type>(result.type_info, [](Type *) {
            /* Ничего не делаем, TypeSystem сама удалит его через unique_ptr */
        });
        return Object::make_native_ref(type_shared);
    } catch (std::runtime_error &ex) {
        throw_eval_error(form, ex.what());
    }
    return get_null();
}

Object Interpreter::eval_defenum_special(const Object &, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto enum_ptr = parse_defenum(rest, &TypeSystem::instance(), nullptr);
    auto enum_shared = std::shared_ptr<Type>(enum_ptr, [](Type *) {
        /* Ничего не делаем, TypeSystem сама удалит его через unique_ptr */
    });
    return Object::make_native_ref(enum_shared);
}

Object Interpreter::eval_types_to_lisp(const Object &, Arguments &,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    return TypeSystem::instance().get_all_type_names_as_objects();
}

Object Interpreter::eval_init_types(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    TypeSystem::instance().clear();
    // Здесь должна быть логика из твоего старого кода для init-types
    if (args.unnamed.size() > 0 && args.unnamed[0].as_symbol() == "z80") {
        TypeSystem::instance().add_builtin_types();
    } else {
        TypeSystem::instance().add_builtin_types_z80();
    }
    return get_null();
}

// ============================================================
// Работа с адресацией подобно dot sytnax в C++
// ============================================================

/**
 * @brief Примитив создания "окна" доступа (Шаг навигации).
 * * * Роль в системе:
 * Это атомарная операция перехода, на которой базируется макрос `->`.
 * Она "склеивает" логику метаданных с адресацией в памяти.
 * * * Принцип работы:
 * Превращает пару (Объект, Ключ) в новую точку доступа (Alias).
 * - Если база — NativeRef (метаданные), извлекает свойства типа.
 * - Если база — TypePointer (память), вычисляет адрес поля и возвращает дочерний TypePointer.
 * * * Использование в Lisp (неявное):
 * Используется внутри функций навигации. Например, в выражении:
 * (-> cell 'x 'y)
 * Интерпретатор дважды вызовет `make-alias`:
 * 1. (make-accessor cell 'x) -> вернет ячейку поля x
 * 2. (make-accessor cell_x 'y) -> вернет ячейку поля y внутри x
 * * @return Object (TypePointer со смещением или метаданные из NativeRef)
 */
Object Interpreter::eval_step(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});

    auto base = args.unnamed[0];
    auto key = args.unnamed[1];

    if (base.is_none() || base.is_null())
        return get_null();

    // Пытаемся сделать шаг (вернет новый Pointer или Field)
    Object next = base.step(key);

    if (next.is_none()) {
        throw_eval_error(form, "Step failed: property '" + key.print() + "' not found");
    }
    return next;
}

/**
 * @brief Специальная форма глубокой навигации (Макрос `->`).
 * * * Роль в системе:
 * Последовательно применяет цепочку ключей к объекту, "проваливаясь" внутрь
 * структур, массивов или метаданных. Реализует высокоуровневый синтаксис доступа.
 * * * Особенности реализации:
 * 1. Первый аргумент вычисляется (eval) — это корень (например, переменная с TypePointer).
 * 2. Последующие аргументы-символы трактуются как имена полей БЕЗ вычисления.
 * 3. Аргументы-списки или числа вычисляются (позволяет динамические индексы).
 * * * Синтаксис в Lisp: (-> root field1 index field2)
 * Пример:
 * (-> my-vec 'x)           ; корень my-vec, шаг к полю x
 * (-> my-buf 10 'int)      ; корень my-buf, вычислить индекс 10, шаг к типу int
 * (-> obj (get-idx) 'name) ; индекс вычисляется вызовом функции
 * * * @param rest Список аргументов навигации.
 * * @return Конечный объект (TypePointer, значение или метаданные) после прохождения всего пути.
 */
Object Interpreter::eval_deref_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    if (!rest.is_pair())
        return get_null();

    auto   pair = rest.as_pair();
    Object current = pair->car;
    Object iterator = pair->cdr;

    // --- 1. ОПРЕДЕЛЕНИЕ КОРНЯ ---
    if (current.is_pair()) {
        // Если корень - это выражение, например (-> (get-obj) field)
        current = eval_with_rewind(form, current, env);
    } else if (current.is_symbol()) {
        // Сначала ищем в переменных Лиспа (динамический объект)
        Object result;
        if (try_symbol_lookup(current, env, &result)) {
            current = result;
            if (current.is_symbol())
                current = TypeSystem::instance().make_step_accessor(current);
        } else {
            // Если переменной нет, ищем в типах (статический доступ к метаданным)
            current = TypeSystem::instance().make_step_accessor(current);
        }
    }

    // Если корень так и не разрешился
    if (current.is_none()) {
        throw_eval_error(form, fmt::format("Deref root '{}' not found in environment or TypeSystem",
                                           pair->car.print()));
        return get_null();
    }

    // --- 2. НАВИГАЦИЯ (ШАГИ) ---
    while (!iterator.is_null()) {
        Object key_form = iterator.as_pair()->car;
        // Ключи (поля) обычно символы, но могут быть вычисляемыми (-> obj (get-field-name))
        Object key = key_form.is_symbol() ? key_form : eval_with_rewind(form, key_form, env);

        // Пытаемся сделать шаг
        Object next;
        try {
            next = current.step(key);
        } catch (const std::exception &e) {
            throw_eval_error(form, fmt::format("Deref object '{}' impossible", current.print()));
        }

        if (next.is_none()) {
            throw_eval_error(
                form, fmt::format("Field '{}' not found in {}", key.print(), current.print()));
        }

        current = next;
        iterator = iterator.as_pair()->cdr;
    }

    // --- 3. РАЗЫМЕНОВАНИЕ УКАЗАТЕЛЕЙ (OpenGOAL style) ---
    // Если результат - TypePointer, мы возвращаем значение, на которое он указывает.
    // Если мы хотим получить сам объект указателя, мы используем другой оператор или (address-of
    // ...)
    if (current.is_type(ObjectType::POINTER)) {
        return current.as_native_ref<TypePointer>()->deref();
    }

    return current;
}

// ============================================================
// Работа с адресом
// ============================================================

/**
 * @brief addr-of: Возвращает адрес объекта (TypePointer или Field) в виде целого числа.
 * addr-of принимает один аргумент - целевой объект (TypePointer, Field или StaticBuffer).
 * addr-of возвращает целое число, которое является адресом объекта в памяти.
 * - Если целевой объект является TypePointer, addr-of возвращает абсолютный адрес, на который
 * ссылается указатель.
 * - Если целевой объект является Field, addr-of возвращает относительное смещение поля в структуре.
 * - Если целевой объект является StaticBuffer, addr-of возвращает базовый адрес буфера.
 * @return Object целое число, которое является адресом объекта в памяти.
 */
Object Interpreter::eval_addr_of(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});
    Object target = args.unnamed[0];

    // 1. Если это число, мы считаем, что пользователь УЖЕ оперирует адресом.
    if (target.is_integer())
        return target;

    // 2. Если это Pointer, возвращаем адрес, на который он указывает.
    if (target.is_pointer()) {
        return Object::make_integer(
            reinterpret_cast<uintptr_t>(target.as_pointer()->resolve_addr()));
    }

    // 3. Для всех NativeRef (Field, MethodInfo, StaticBuffer, StructureType...)
    // Правило одно: берем адрес самого объекта (или его данных), которые лежат в C++.
    if (target.is_native_ref()) {
        // Получаем shared_ptr на обертку
        auto hr = target.as_native_ref<NativeRef>();

        // .get() возвращает сырой указатель (NativeRef*),
        // который теперь можно легально кастить в число.
        return Object::make_integer(reinterpret_cast<uintptr_t>(hr.get()));
    }

    // 4. Fallback: если это обычный объект Лиспового типа (Pair, Symbol)
    // Мы все равно можем вернуть его адрес в памяти для полной транспарентности.
    return Object::make_integer(reinterpret_cast<uintptr_t>(&target));
}

Object Interpreter::eval_addr_plus(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    uintptr_t total = 0;

    for (const auto &arg : args.unnamed) {
        if (arg.is_integer()) {
            total += arg.as_integer();
        } else if (arg.is_pointer()) {
            total += reinterpret_cast<uintptr_t>(arg.as_pointer()->resolve_addr());
        } else if (auto f = arg.as_native_ref<Field>()) {
            total += f->offset();
        } else if (auto f = arg.as_native_ref<MethodInfo>()) {
            total += f->id;
        } else {
            throw_eval_error(form, "addr+ : argument is not a number, pointer: " + arg.print());
        }
    }

    return Object::make_integer(total);
}

// ============================================================
// Специальные операторы приведения типа
// ============================================================

Object Interpreter::eval_the_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Проверка структуры (the <type> <value>)
    if (!rest.is_pair() || !rest.as_pair()->cdr.is_pair()) {
        throw_eval_error(form, "the: expected 2 arguments: (the <type-name> <target>)");
    }

    auto   pair = rest.as_pair();
    Object type_form = pair->car;
    Object target_form = pair->cdr.as_pair()->car;

    // 2. Первый аргумент должен быть символом или списком (TypeSpec)
    // В OpenGOAL (the int x) и (the (pointer int) x) оба валидны.
    if (!type_form.is_symbol() && !type_form.is_pair()) {
        throw_eval_error(form, fmt::format("the: first argument must be a type-spec, got {}",
                                           type_form.print()));
    }

    // Создаем спецификацию того, что мы ОЖИДАЕМ
    // Предполагаю, что конструктор TypeSpec умеет принимать Object-форму
    TypeSpec expected_spec(type_form.to_std_string());

    // 3. Вычисляем объект, который ПРОВЕРЯЕМ
    Object target = eval_with_rewind(form, target_form, env);

    // 4. Определяем спецификацию того, что у нас ЕСТЬ
    TypeSpec actual_spec;
    if (target.is_pointer()) {
        // Если это Pointer, мы проверяем его внутренний тип (на что он указывает)
        actual_spec = TypeSpec(target.as_pointer()->get_type_name());
    } else {
        // Для обычных объектов (integer, string, vec) берем имя их типа
        actual_spec = TypeSpec(target.type_name());
    }

    // 5. Проверка через TypeSystem::tc
    // less_specific = expected (супертип)
    // more_specific = actual (подтип)
    if (!TypeSystem::instance().tc(expected_spec, actual_spec)) {
        // Используем твой новый метод для выброса ошибки
        // Мы можем передать информацию прямо из TypeSystem
        throw_eval_error(form, fmt::format("Type assertion failed: expected {}, but got {}",
                                           expected_spec.print(), actual_spec.print()));
    }

    // 6. Возврат
    // В GOAL 'the' возвращает тот же объект, но компилятор теперь "уверен" в его типе.
    return target;
}

Object Interpreter::eval_the_as_special(const Object &form, const Object &rest,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    // Проверка структуры (the-as <type> <value>)
    if (!rest.is_pair() || !rest.as_pair()->cdr.is_pair()) {
        throw_eval_error(form, "the-as: expected 2 arguments: (the-as <type-name> <target>)");
    }

    auto   pair = rest.as_pair();
    Object type_form = pair->car;
    Object target_form = pair->cdr.as_pair()->car;

    // 1. Первый аргумент (тип) — не вычисляем, он должен быть символом
    if (!type_form.is_symbol()) {
        throw_eval_error(form, fmt::format("the-as: first argument must be a type symbol, got {}",
                                           type_form.print()));
    }

    std::string type_name = type_form.as_symbol();

    // 2. Второй аргумент — вычисляем
    Object target = eval_with_rewind(form, target_form, env);

    // 3. Логика приведения (Cast)

    // Случай А: На входе Integer (адрес) -> создаем новый Pointer с этим типом
    if (target.is_integer()) {
        void *addr = reinterpret_cast<void *>(static_cast<uintptr_t>(target.as_integer()));
        // Передаем строку type_name в конструктор
        return Object::make_pointer(addr, type_name);
    }

    // Случай Б: На входе уже Pointer -> меняем ему тип "на лету"
    if (target.is_pointer()) {
        auto old_ptr = target.as_pointer();

        // Вместо копирования объекта целиком, создаем новый,
        // вытаскивая сырой адрес из старого через наш resolve_addr()
        void *raw_addr = old_ptr->resolve_addr();

        // Создаем новый указатель.
        // Мы передаем raw_addr и новую строку типа.
        // Interpreter подхватит это и создаст правильный TypePointer.
        return Object::make_pointer(raw_addr, type_name);
    }

    // Если пришло что-то другое — кидаем ошибку несовместимости типов
    // Используем твой новый метод throw_type_mismatch
    throw_type_mismatch(form, 1, {ObjectType::INTEGER, ObjectType::POINTER}, target.type,
                        Arguments{{target}, {}, {}});

    return get_null();
}

// ============================================================
// Получение размеров и смещений
// ============================================================

Object Interpreter::eval_offset_of_special(const Object &form, const Object &rest,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto args = get_args(form, rest, ArgumentSpec(false, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}},
                 {}); // Вторая тоже символ

    auto  type_name = args.unnamed[0].as_symbol();
    auto &ts = TypeSystem::instance();

    if (!ts.fully_defined_type_exists(type_name.name_ptr)) {
        throw_eval_error(
            form, fmt::format("Type '{}' not found or not fully defined.", type_name.c_str()));
    }

    auto type = ts.lookup_type(type_name.name_ptr);
    auto as_struct = dynamic_cast<StructureType *>(type);

    // Защита от nullptr
    if (!as_struct) {
        throw_eval_error(
            form, fmt::format("Type '{}' is not a structure/reference type.", type_name.c_str()));
    }

    auto field_name = args.unnamed[1].as_symbol();
    // Проверяем существование поля перед получением инфо
    try {
        auto info = ts.lookup_field_info(type->get_name(), field_name);

        // Логика смещения: если объект boxed, смещение в коде обычно считается от начала
        // данных, а не от тега.
        auto off = info.field.offset();

        return Object::make_integer(off);
    } catch (const std::exception &e) {
        throw_eval_error(form, fmt::format("Field '{}' not found in type '{}'.", field_name.c_str(),
                                           type_name.c_str()));
    }

    return get_null();
}

Object Interpreter::eval_size_of_special(const Object &form, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto  args = get_args(form, rest, ArgumentSpec(false, true));
    auto &ts = TypeSystem::instance();

    if (args.unnamed.size() == 1) {
        // Обычный size-of типа: (size-of vector)
        auto type_name = args.unnamed[0].as_symbol();
        return Object::make_integer(ts.lookup_type(type_name.name_ptr)->get_size_in_memory());
    } else if (args.unnamed.size() == 2) {
        // Умный size-of поля: (size-of vector x)
        auto type_name = args.unnamed[0].as_symbol();
        auto field_name = args.unnamed[1].as_symbol();

        auto type = ts.lookup_type(type_name.name_ptr);
        auto info = ts.lookup_field_info(type->get_name(), field_name.name_ptr);

        // Берем тип самого поля и узнаем его размер
        auto field_type = ts.lookup_type(info.field.type().base_type());
        return Object::make_integer(field_type->get_size_in_memory());
    }

    throw_eval_error(form, "size-of: expected 1 or 2 arguments");
    return get_null();
}

Object Interpreter::eval_method_id_of_special(const Object &form, const Object &rest,
                                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto args = get_args(form, rest, ArgumentSpec(false, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}}, {});

    auto  type_name = args.unnamed[0].as_symbol();
    auto  method_name = args.unnamed[1].as_symbol();
    auto &ts = TypeSystem::instance();

    if (!ts.fully_defined_type_exists(type_name.name_ptr)) {
        throw_eval_error(form,
                         fmt::format("Type '{}' not found for method-id.", type_name.c_str()));
    }

    auto type = ts.lookup_type(type_name.name_ptr);
    auto m_info = ts.lookup_method(type->get_name(), method_name.name_ptr);

    return Object::make_integer(m_info.id);
}

Object Interpreter::eval_method_of_special(const Object &form, const Object &rest,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto args = get_args(form, rest, ArgumentSpec(false, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL, ObjectType::INTEGER}}, {});
    auto  type_name = args.unnamed[0].as_symbol();
    auto &ts = TypeSystem::instance();
    if (!ts.fully_defined_type_exists(type_name.name_ptr)) {
        throw_eval_error(form,
                         fmt::format("Type '{}' not found for method-id.", type_name.c_str()));
    }

    auto type = ts.lookup_type(type_name.name_ptr);
    if (args.unnamed[1].is_integer()) {
        auto method_id = args.unnamed[1].as_integer();
        auto m_info = ts.lookup_method(type->get_name(), method_id);
        auto method_ptr = std::make_shared<MethodInfo>(m_info);
        return Object::make_native_ref(method_ptr);
    } else {
        auto method_name = args.unnamed[1].as_symbol();
        auto m_info = ts.lookup_method(type->get_name(), method_name.name_ptr);
        auto method_ptr = std::make_shared<MethodInfo>(m_info); // Честная копия
        return Object::make_native_ref(method_ptr);
    }
}

// ============================================================
// Чтение запись памяти
// ============================================================

/**
 * @brief Чтение значения из ячейки памяти (Dereference).
 * * * Роль: Преобразует сырые байты из буфера в объект интерпретатора.
 * Использует метаданные типа, хранящиеся в TypePointer, чтобы понять,
 * сколько байт читать и как их интерпретировать (как число, строку или энум).
 * * * Lisp Logic:
 * (mem-get cell) -> возвращает значение.
 * Обычно используется автоматически при обращении к ячейке в контексте выражения.
 * * * @param args[0] Объект TypePointer.
 * @return Object (Число, строка или другой примитив).
 */
Object Interpreter::eval_mem_get(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::POINTER}}, {});
    // Просто дергаем метод Pointer::get(), который мы обсуждали
    return args.unnamed[0].as_pointer()->get();
}
/**
 * @brief Запись значения в ячейку памяти (Assignment).
 * * * Роль: Сериализует объект интерпретатора в сырые байты буфера.
 * Это "физический" уровень записи. Функция берет TypePointer, находит
 * связанный с ней StaticBuffer и записывает данные по адресу TypePointer->ptr.
 * * * Особенности:
 * - Учитывает порядок байтов (Endianness) целевой платформы (Z80).
 * - Выполняет проверку типов (нельзя записать строку в ячейку int8).
 * * * Lisp Logic:
 * (mem-set! cell value)
 * (set! (-> my-struct 'field) 10) ; Внутри развернется в call-set!
 * * * @param args[0] Объект TypePointer (куда писать).
 * @param args[1] Значение (что писать).
 * @return undefined
 */
Object Interpreter::eval_mem_set(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::POINTER}, {}}, {});
    auto ptr = args.unnamed[0].as_pointer();
    ptr->set(args.unnamed[1]); // Пишем значение
    return get_undefined();
}

// ============================================================
// Статический буфер в памяти
// ============================================================

/**
 * @brief Создание статического буфера памяти (Холст).
 * * * Роль: Выделяет блок "сырой" памяти фиксированного размера, который
 * имитирует адресное пространство целевой платформы (например, RAM Z80).
 * * * Параметры:
 * 1. name (String) — имя буфера для логов и отладки.
 * 2. size (Integer) — физический размер в байтах.
 * 3. origin (Integer) — базовый адрес (VMA). Если origin = 0x100,
 * то запись в начало буфера будет трактоваться как запись по адресу 0x100.
 * * * Lisp Logic:
 * (make-static-buffer "main-ram" 1024 #x0000)
 * * * @return Object (HeapObject типа STATIC_BUFFER).
 */
Object Interpreter::eval_make_static_buffer(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::STRING},   // тип
                  {ObjectType::INTEGER},  // размер
                  {ObjectType::INTEGER}}, // origin
                 {});

    std::string type_name = args.unnamed[0].as_string()->data;
    int         size = args.unnamed[1].as_integer();
    uint32_t origin = static_cast<uint32_t>(args.unnamed[2].as_integer()); // исправлено имя и тип
    auto     buffer = std::make_shared<StaticBuffer>(type_name, size, origin);
    return Object::make_heap_object(buffer, ObjectType::STATIC_BUFFER);
}

/**
 * @brief Создание курсора записи (Поток/Врайтер).
 * * * Роль: Обертка над буфером, которая управляет "текущей позицией" записи.
 * Позволяет писать данные последовательно, не вычисляя каждый раз оффсет вручную.
 * Хранит ссылку на TypeSystem для автоматического выравнивания (alignment) типов.
 * * * Особенности:
 * - Связывает физический буфер с логикой типов.
 * - Позволяет выполнять автоматическую аллокацию места под структуры.
 * * * Lisp Logic:
 * (make-static-writer my-buf)
 * * * @param args[0] Существующий объект STATIC_BUFFER.
 * @return Object (HeapObject типа STATIC_WRITER).
 */
Object Interpreter::eval_make_static_writer(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STATIC_BUFFER}}, {});
    auto buffer = args.unnamed[0].as_native_ref<StaticBuffer>();
    auto writer = std::make_shared<StaticWriter>(buffer);
    return Object::make_heap_object(writer, ObjectType::STATIC_WRITER);
}

/**
 * @brief Фабрика ячеек памяти (Типизированный указатель).
 * * * Роль: Создает объект TypePointer, который связывает конкретный адрес в памяти с типом.
 * Поддерживает два режима работы:
 * 1. Через Writer: (static-cell writer 'type)
 * - Автоматически выделяет место в текущей позиции врайтера.
 * - Сдвигает курсор врайтера с учетом выравнивания (alignment).
 * 2. Через Buffer: (static-cell buffer offset 'type)
 * - Создает "окно" по фиксированному смещению без изменения состояния буфера.
 * * * Lisp Logic:
 * (define cell (static-cell wr 'test-vector)) ; Аллокация и создание ссылки
 * (define cell (static-cell buf #x10 'int8))  ; Прямой доступ по адресу
 * * * @return Object (TypePointer).
 */
Object Interpreter::eval_make_buffer_pointer(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {ObjectType::INTEGER}, {ObjectType::SYMBOL}}, {});
    // (static-cell buffer offset 'type) ИЛИ (static-cell writer 'type)
    if (args.unnamed[0].is_type(ObjectType::STATIC_WRITER)) {
        auto        writer = args.unnamed[0].as_native_ref<StaticWriter>();
        std::string type_name = args.unnamed[1].as_symbol();
        return writer->allocate(type_name); // Возвращает TypePointer через HeapObject
    }

    auto        buffer = args.unnamed[0].as_native_ref<StaticBuffer>();
    size_t      offset = static_cast<size_t>(args.unnamed[1].as_integer());
    std::string type_name = args.unnamed[2].as_symbol();

    Type *type = TypeSystem::instance().lookup_type(type_name);
    void *ptr = buffer->data() + offset;
    auto  cell = std::make_shared<TypePointer>(ptr, type, buffer);
    return Object::make_heap_object(cell, ObjectType::POINTER);
}

/**
 * (buffer-add-label buffer-or-writer name :address addr :segment seg :meta meta)
 */
Object Interpreter::eval_buffer_label_set(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверка аргументов
    vararg_check(form, args, {{}, {}},
                 {
                     {"address", {false, {ObjectType::INTEGER}}},
                     {"segment", {false, {ObjectType::STRING, ObjectType::SYMBOL}}},
                     {"meta", {false, {}}} // Любой тип
                 });

    auto first_arg = args.unnamed.at(0);
    // Унифицируем имя: 'foo или "foo" -> "foo"
    auto label_name = args.unnamed.at(1).to_std_string();

    // 2. Определяем целевой буфер и базовый офсет
    StaticBuffer *buffer = nullptr;
    size_t        offset = 0;

    if (first_arg.is_buffer_writer()) {
        auto writer = first_arg.as_native_ref<StaticWriter>();
        buffer = writer->get_buffer().get();
        offset = writer->tell();
    } else if (first_arg.is_static_buffer()) {
        buffer = first_arg.as_native_ref<StaticBuffer>().get();
        // Для прямого обращения к буферу адрес обязателен, если метка новая
        if (!args.has_named("address") && !buffer->has_label(label_name)) {
            throw_eval_error(form, "Direct buffer labeling requires :address for new labels");
        }
        if (args.has_named("address")) {
            offset = static_cast<size_t>(args.get_named("address").as_integer());
        }
    } else {
        throw_eval_error(form, "First argument must be writer or buffer, got " + first_arg.print());
    }

    // 3. Логика Upsert (Update or Insert)
    Object label_obj = buffer->get_label_obj(label_name);

    if (!label_obj.is_null()) {
        // ОБНОВЛЕНИЕ существующего HeapObject
        auto label = label_obj.as_native_ref<BufferLabel>();

        // Обновляем адрес, только если он был явно передан или мы пишем через writer
        if (args.has_named("address") || first_arg.is_buffer_writer()) {
            label->addr = offset;
        }

        if (args.has_named("segment"))
            label->segment = args.get_named("segment");
        if (args.has_named("meta"))
            label->meta = args.get_named("meta");

        return label_obj;
    } else {
        // СОЗДАНИЕ нового HeapObject
        Object segment =
            args.has_named("segment") ? args.get_named("segment") : Object::make_string("main");
        Object meta = args.has_named("meta") ? args.get_named("meta") : get_null();

        buffer->add_label(label_name, offset, segment, meta);
        return buffer->get_label_obj(label_name);
    }
}

/**
 * @brief Get buffer label by name.
 * @param form The form of function call.
 * @param args The arguments passed to the function.
 * @param env The environment object.
 * @return The buffer label object.
 *
 * This function gets the buffer label object by name. It takes two arguments:
 * the buffer writer or buffer, and the name of the label.
 *
 * If the label does not exist, the function returns null.
 */
Object Interpreter::eval_buffer_label_get(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Принимаем 2 позиционных аргумента: буфер и имя
    vararg_check(form, args, {{}, {ObjectType::STRING, ObjectType::SYMBOL}}, {});

    auto first_arg = args.unnamed.at(0);
    auto label_name = args.unnamed.at(1).to_std_string();

    StaticBuffer *buffer = nullptr;

    if (first_arg.is_buffer_writer()) {
        buffer = first_arg.as_native_ref<StaticWriter>()->get_buffer().get();
    } else if (first_arg.is_static_buffer()) {
        buffer = first_arg.as_native_ref<StaticBuffer>().get();
    } else {
        throw_eval_error(form, "Expected writer or buffer as first argument");
    }

    // Возвращаем объект из мапы (там уже лежит Object, инкапсулирующий Label*)
    Object label_obj = buffer->get_label_obj(label_name);

    // Если не нашли — возвращаем null, чтобы Lisp мог проверить (if (buffer-label-get ...))
    return label_obj;
}

/**
 * @brief Визуализация содержимого памяти (Hex Dump).
 * * * Роль: Генерирует форматированную строку, представляющую сырые байты буфера
 * в человекочитаемом виде (шестнадцатеричный код + ASCII).
 * * * Параметры:
 * 1. buffer         — целевой буфер.
 * 2. start_offset   — начальная точка чтения.
 * 3. bytes_to_dump  — количество байт для отображения.
 * 4. show_ascii     — флаг включения символьного представления (справа).
 * 5. bytes_per_line — ширина строки дампа (обычно 8 или 16).
 * * * Lisp Logic:
 * (fmt #t (static-buffer-dump buf 0 256 #t 16))
 * * * @return Object (String с отформатированным дампом).
 */
Object Interpreter::eval_buffer_dump(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args,
                 {{ObjectType::STATIC_BUFFER},
                  {ObjectType::INTEGER},
                  {ObjectType::INTEGER},
                  {ObjectType::SYMBOL},
                  {ObjectType::INTEGER}},
                 {});
    auto buffer = args.unnamed[0].as_native_ref<StaticBuffer>();
    auto start_offset = args.unnamed[1].as_integer();
    auto bytes_to_dump = args.unnamed[2].as_integer();
    bool show_ascii = args.unnamed[3].as_symbol();
    int  bytes_per_line = args.unnamed[4].as_integer();
    auto str = buffer->hex_dump(start_offset, bytes_to_dump, show_ascii, bytes_per_line);
    return Object::make_string(str);
}

/**
 * @brief Высокоуровневая команда записи в статическую память.
 * Роль: Универсальный интерфейс для записи данных (чисел, строк, структур)
 * в буфер или через врайтер. Автоматически управляет типами и смещениями.
 * Режимы работы:
 * 1. Через Writer (Stream mode): (write-to wr val :as 'type)
 * - Автоматически выделяет место (allocate).
 * - Позволяет записывать "теги" (маркеры), просто вызывая запись констант по очереди.
 * 2. Через Buffer (Random access): (write-to buf val :as 'type :at offset)
 * - Записывает данные строго по указанному адресу.
 * Особенности:
 * - Использует временную или постоянную TypePointer для выполнения физической записи.
 * - Возвращает смещение (offset), по которому были записаны данные, что удобно
 * для построения таблиц перекрестных ссылок.
 * Lisp Logic:
 * (write-to-buffer wr #xAA :as 'int)       ; Запись тега-маркера
 * (write-to-buffer wr 10 :as 'test-enum)   ; Запись значения по типу
 * @return Object (Integer — итоговый offset записи).
 */
Object Interpreter::eval_buffer_write(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}},
                 {{"as", {true, {ObjectType::SYMBOL}}}, {"at", {false, {ObjectType::INTEGER}}}});

    Object      target = args.unnamed[0];
    Object      value = args.unnamed[1];
    std::string type_name = args.named["as"].to_std_string();
    // fmt::print("{}\n", pretty_print::to_string(value, 80).c_str());
    try {
        TypePointer                 *cell_ptr = nullptr;
        std::shared_ptr<TypePointer> managed_cell;
        size_t                       write_offset = 0;

        // 1. Подготовка ячейки (Слайса)
        if (target.is_type(ObjectType::STATIC_WRITER)) {
            auto   writer = target.as_native_ref<StaticWriter>();
            Object cell_obj = writer->allocate(type_name);

            // Здесь cell_obj должен быть POINTER (TypePointer)
            managed_cell = cell_obj.as_native_ref<TypePointer>();
            cell_ptr = managed_cell.get();

            // Вычисляем оффсет относительно начала буфера писателя для возвращаемого значения
            write_offset = writer->tell() - cell_ptr->get_type()->get_size_in_memory();
        } else {
            if (!args.named.count("at")) {
                throw_eval_error(form, "Keyword :at required for buffer write");
                return get_null();
            }

            auto buffer_ptr = target.as_native_ref<StaticBuffer>();
            write_offset = static_cast<size_t>(args.named["at"].as_integer());

            Type *type = TypeSystem::instance().lookup_type(type_name);
            if (!type) {
                throw_eval_error(form, "Unknown type: " + type_name);
            }

            void *physical_ptr = buffer_ptr->data() + write_offset;

            // ОБНОВЛЕНО: Используем новый конструктор с 3 аргументами
            managed_cell = std::make_shared<TypePointer>(physical_ptr, type, buffer_ptr);
            cell_ptr = managed_cell.get();
        }

        // 2. САМА ЗАПИСЬ (Магия пакетов)
        if (value.is_pair()) {
            Object current_item = value;
            size_t internal_index = 0;

            // Итерируемся по списку (пакету данных)
            while (current_item.is_pair()) {
                Object entry = current_item.as_pair()->car;

                // Используем проверку на точечный синтаксис (field . value)
                if (entry.is_dotted_syntax()) {
                    // ВЕТКА А: Работаем как со структурой
                    Object field_name = entry.as_pair()->car;
                    Object field_val = entry.as_pair()->cdr;

                    Object sub_cell = cell_ptr->make_step_accessor(field_name);
                    if (!sub_cell.is_none()) {
                        recursive_write(sub_cell, field_val);
                    }
                } else {
                    // ВЕТКА Б: Работаем как с массивом/списком значений
                    // Применяем смещение по индексу относительно текущей ячейки
                    Object sub_cell =
                        cell_ptr->make_step_accessor(Object::make_integer(internal_index));
                    if (!sub_cell.is_none()) {
                        recursive_write(sub_cell, entry);
                    }
                    internal_index++;
                }

                // Переходим к следующему элементу входного списка
                current_item = current_item.as_pair()->cdr;
            }
        } else if (!value.is_null()) {
            // Если пришло одиночное значение — пишем как раньше
            cell_ptr->set(value);
        } else {
            throw_eval_error(form, fmt::format("Static write failed: because value is null"));
        }

        return Object::make_integer(write_offset);
    } catch (const std::exception &e) {
        throw_eval_error(form, fmt::format("Static write failed: {}", e.what()));
    }
    return get_null();
}

/**
 * @brief Recursive write to a cell (alist or array)
 *
 * This function takes a cell object and a value to write to that cell.
 * If the value is an alist, it recursively writes the elements of the alist
 * to the corresponding corresponding cell. If the value is an array, it writes the elements
 * of the array to the cell.
 *
 * @param cell_obj The cell to write to.
 * @param value The value to write to the cell. Can be an alist or an array.
 */
void Interpreter::recursive_write(Object cell_obj, Object value) {
    if (!cell_obj.is_type(ObjectType::POINTER))
        return;
    auto cell = cell_obj.as_native_ref<TypePointer>();

    // 1. Если это ПУСТОЙ список — просто выходим, это конец обхода
    if (value.is_null()) {
        return;
    }
    if (value.is_pair() && !value.as_pair()->car.is_pair()) {
        int    index = 0;
        Object current_list = value;

        while (!current_list.is_null()) {
            // 1. Создаем ячейку для i-го элемента массива
            // Мы используем наш новый make_step_accessor(index)
            Object element_cell = cell->make_step_accessor(Object::make_integer(index));

            // 2. Рекурсивно пишем значение в эту ячейку
            recursive_write(element_cell, current_list.as_pair()->car);

            // 3. Переходим к следующему элементу списка
            current_list = current_list.as_pair()->cdr;
            index++;
        }
        return; // Завершили запись массива
    }

    // --- ОБРАБОТКА СТРУКТУРЫ (alist) ---
    if (value.is_pair() && value.as_pair()->car.is_pair()) {
        Object current = value;
        while (current.is_pair()) {
            Object entry = current.as_pair()->car;
            if (entry.is_pair()) {
                Object key = entry.as_pair()->car; // Например, 'tag'
                Object val = entry.as_pair()->cdr; // Например, #x55

                try {
                    // Создаем "дочернюю" ячейку для конкретного поля
                    // make_step_accessor сам вычислит смещение поля внутри структуры
                    Object field_cell =
                        cell_obj.as_native_ref<TypePointer>()->make_step_accessor(key);

                    // Рекурсивно пишем значение в это поле
                    recursive_write(field_cell, val);
                } catch (const std::exception &e) {
                    // Если поля не существует или ошибка смещения
                    throw_eval_error(key, "StaticWrite error in field '" + key.print() +
                                              "': " + e.what());
                }
            }
            current = current.as_pair()->cdr;
        }
        return; // Завершаем обработку структуры
    }
    // --- ОБРАБОТКА ПРИМИТИВА ---
    // Если это не список, значит это конечное значение (int, float, etc.)
    if (!value.is_pair()) {
        // fmt::print("recursive_write cell: {} value: {}\n", cell->print(), value.print());
        cell->set(value); // Запись в память через mem-set
    }
}

Object Interpreter::eval_buffer_read(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}},
                 {{"as", {true, {ObjectType::SYMBOL}}}, {"at", {true, {ObjectType::INTEGER}}}});

    auto        buffer = args.unnamed[0].as_native_ref<StaticBuffer>();
    std::string type_name = args.named["as"].to_std_string();
    size_t      offset = static_cast<size_t>(args.named["at"].as_integer());

    // 2. Ищем определение типа
    Type *type = TypeSystem::instance().lookup_type(type_name);
    if (!type)
        throw std::runtime_error("Unknown type: " + type_name);

    // 3. Вычисляем физический адрес (БАЗА + СМЕЩЕНИЕ)
    void *physical_ptr = buffer->data() + offset;

    // 4. Создаем ячейку-указатель
    // Оффсет (offset) больше не передаем, он вычислится в set() через (ptr - data)
    auto cell = std::make_shared<TypePointer>(physical_ptr, type,
                                              buffer // Владелец удерживает буфер в памяти
    );

    // 5. Возвращаем результат разыменования
    // - Если тип "value" (int, float, enum) -> вернет Object с числом/символом.
    // - Если тип "structure" -> вернет Object(TypePointer), позволяя цепочку (-> ...).
    return cell->get();
}

/**
 * (define buf (make-static-buffer "ROM" 1024))
 *
 * ;; Записываем код, который прыгает на обработчик, которого еще нет
 * (buffer-write-u8 buf 0 #xC3) ;; Код инструкции JP
 * (buffer-write-reloc buf 1 "irq_handler") ;; Релокация на 1-й байт (адрес для JP)
 *
 * ;; ... где-то позже или в другом файле ...
 * (buffer-add-label buf "irq_handler" #x0100)
 */

Object Interpreter::eval_buffer_reloc(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::STATIC_BUFFER}, {ObjectType::INTEGER}, {ObjectType::STRING}}, {});

    auto        buf = args.unnamed[0].as_native_ref<StaticBuffer>();
    size_t      offset = args.unnamed[1].as_integer();
    std::string target = args.unnamed[2].to_std_string();

    // По умолчанию считаем ABS_ADDR 16-бит (Z80 style),
    // но можно расширить аргументами
    buf->write_reloc(offset, target, RelocType::ABS_ADDR);

    return Object::make_none();
}

Object Interpreter::eval_buffer_link(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STATIC_BUFFER}}, {});

    auto buf = args.unnamed[0].as_native_ref<StaticBuffer>();

    // По умолчанию считаем ABS_ADDR 16-бит (Z80 style),
    // но можно расширить аргументами
    buf->link_internal();

    return Object::make_none();
}

// ============================================================
// Работа с буфером более элегантная
// ============================================================
/**
 * ;; Лисп создает буфер в памяти хоста
 * (define my-pos (static-new vector :x 10 :y 20))
 *
 * (zasm
 *   ;; Ассемблер при генерации кода вытащит данные из my-pos
 *   (ld ix my-pos)
 *   (ld a [+ ix (offset-of vector y)])
 * )
 */
Object Interpreter::eval_static_new_special(const Object &form, const Object &rest,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto  args = get_args(form, rest, ArgumentSpec(false, true));
    auto &ts = TypeSystem::instance();

    if (args.unnamed.empty()) {
        throw_eval_error(form, "static-new: expected at least type name");
        return get_null();
    }

    // 1. Извлекаем имя типа (например, vector)
    std::string type_name = args.unnamed[0].to_std_string();
    Type       *type = ts.lookup_type(type_name);
    if (!type) {
        throw_eval_error(form, "static-new: unknown type " + type_name);
    }

    auto origin = 0x0000;
    auto buffer_name = "static-new-" + type_name;

    // 2. Создаем StaticBuffer нужного размера
    size_t size = type->get_size_in_memory();
    auto   buffer = std::make_shared<StaticBuffer>(buffer_name, size, origin);

    // 3. Создаем "корневой" указатель на начало буфера
    // TypePointer связывает физическую память буфера с метаданными типа
    auto root_cell = std::make_shared<TypePointer>(buffer->data(), type, buffer);

    // 4. Если переданы дополнительные аргументы (alist или список значений)
    // мы используем твою готовую recursive_write
    if (args.unnamed.size() > 1) {
        // Мы берем все аргументы после имени типа как "пакет данных"
        // (static-new vector ((x . 10) (y . 20)))
        Object data_package = args.unnamed[1];

        try {
            recursive_write(Object::make_native_ref(root_cell), data_package);
        } catch (const std::exception &e) {
            throw_eval_error(form, fmt::format("static-new initialization failed: {}", e.what()));
        }
    }
    // Поддержка именованных аргументов (если синтаксис :x 10)
    else if (!args.named.empty()) {
        // Превращаем named args в alist для recursive_write
        // Это позволит писать (static-new vector :x 10 :y 20)
        Object alist = get_null();
        for (auto it = args.named.rbegin(); it != args.named.rend(); ++it) {
            Object key = Object::make_symbol(it->first);
            Object val = it->second;
            alist = Object::make_pair(Object::make_pair(key, val), alist);
        }
        recursive_write(Object::make_native_ref(root_cell), alist);
    }

    // 5. Возвращаем созданный буфер
    // В зависимости от твоей архитектуры, ты можешь возвращать либо сам Buffer,
    // либо Pointer на него. Для ассемблера лучше возвращать Buffer.
    return Object::make_native_ref(buffer);
}

// ============================================================
// Итераторы
// ============================================================

Object Interpreter::eval_string_for_each(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::LAMBDA}}, {});

    const std::string &str = args.unnamed[0].as_string()->data;
    Object             lambda = args.unnamed[1];

    for (unsigned char c : str) {
        // Передаем код символа как Integer
        call_lambda_internal(lambda, {Object::make_integer(static_cast<int>(c))});
    }
    return get_null();
}

Object Interpreter::eval_vector_for_each(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем типы: первый аргумент — массив (вектор), второй — лямбда
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::LAMBDA}}, {});

    const auto &vec = args.unnamed[0].as_array()->data;
    Object      lambda = args.unnamed[1];

    for (const auto &item : vec) {
        // Вызываем лямбду для каждого элемента вектора
        call_lambda_internal(lambda, {item});
    }

    return get_null();
}

/// @brief Специальная форма для обхода хеш-таблицы с лямбдой.
/// @param form Специальная форма "for-each".
/// @param args Аргументы к форме:
///     - 1-й аргумент: хеш-таблица
///     - 2-й аргумент: лямбда, которая будет вызвана для каждого элемента хеш-таблицы
/// @return undefined
Object Interpreter::eval_hash_table_for_each(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING_HASH_TABLE}, {ObjectType::LAMBDA}}, {});

    auto       &table = args.unnamed[0].as_hash_table()->data;
    Object      lambda = args.unnamed[1];
    const auto &lam_data = lambda.as_lambda();

    for (auto const &[key, val] : table) {
        if (lam_data->args.unnamed.size() == 1) {
            // Если лямбда ждет 1 аргумент, упаковываем в пару (entry)
            call_lambda_internal(lambda, {Object::make_pair(Object::make_string(key), val)});
        } else {
            // Если ждет 2 (или больше/rest), передаем как два аргумента
            call_lambda_internal(lambda, {Object::make_string(key), val});
        }
    }
    return get_null();
}

Object Interpreter::eval_list_for_each(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}, {ObjectType::LAMBDA}}, {});

    Object current = args.unnamed[0];
    Object lambda = args.unnamed[1];

    while (current.is_pair()) {
        // Вызываем твой надежный call_lambda_internal
        call_lambda_internal(lambda, {current.as_pair()->car});
        // print_form_info(form);
        current = current.as_pair()->cdr;
    }

    return get_null();
}

// ============================================================
// CRC32
// ============================================================

Object Interpreter::eval_crc32(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});

    return Object::make_integer(args.unnamed[0].as_crc32());
}

// ============================================================
// HexFile Format
// ============================================================

// Вспомогательная функция для расчета контрольной суммы и форматирования строки
std::string format_hex_record(uint8_t length, uint16_t addr, uint8_t type, const uint8_t *data) {
    uint8_t     checksum = length + (addr >> 8) + (addr & 0xFF) + type;
    std::string hex_data;
    for (int i = 0; i < length; ++i) {
        checksum += data[i];
        hex_data += fmt::format("{:02X}", data[i]);
    }
    checksum = static_cast<uint8_t>((~checksum) + 1);
    return fmt::format(":{:02X}{:04X}{:02X}{}{:02X}\n", length, addr, type, hex_data, checksum);
}

Object Interpreter::eval_export_intel_hex(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Ожидаем: (export-hex "path/to/file.hex" buffer_or_list)
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STATIC_BUFFER, ObjectType::PAIR}},
                 {});

    std::string path = args.unnamed[0].to_std_string();
    Object      source = args.unnamed[1];
    std::string full_content;

    auto process_buffer = [&](const std::shared_ptr<StaticBuffer> &buf) {
        uint8_t *raw_data = buf->data();
        uint32_t base_origin = buf->origin();
        uint32_t start_off = buf->get_start_addr(); // Смещение относительно origin
        uint32_t end_off = buf->get_end_addr();     // Смещение относительно origin

        // Если в буфер ничего не писали, start_off будет 0xFFFFFFFF
        if (start_off > end_off)
            return;

        // Итерируемся от start_off до end_off
        for (size_t i = start_off; i <= end_off; i += 16) {
            // Вычисляем размер текущего чанка (не более 16 байт и не заходя за end_off)
            uint8_t chunk = static_cast<uint8_t>(std::min((size_t)16, (size_t)(end_off - i + 1)));

            // Реальный адрес в Intel HEX: origin + смещение внутри буфера
            uint16_t hex_addr = static_cast<uint16_t>(base_origin + i);

            // Записываем кусок данных из raw_data, начиная с индекса i
            full_content += format_hex_record(chunk, hex_addr, 0x00, &raw_data[i]);
        }
    };

    if (source.is_type(ObjectType::STATIC_BUFFER)) {
        process_buffer(source.as_native_ref<StaticBuffer>());
    } else {
        // Если это список буферов (наш гибридный лэйаут)
        auto current = source;
        if (current.is_pair()) {
            auto pair = current.as_pair();
            auto buf = pair->car.as_native_ref<StaticBuffer>();
            process_buffer(buf);
            current = pair->cdr;
        }
    }

    // Финальный маркер конца файла
    full_content += ":00000001FF";
    auto project_path = file_util::get_path(file_util::PathType::PROJECT);
    if (path[0] != '/' && path[0] != '\\')
        path = project_path.string() + "/" + path;
    file_util::write_text(path, full_content);
    return Object::make_boolean(true);
}
} // namespace script
