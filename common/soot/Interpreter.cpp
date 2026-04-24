#include "common/soot/Interpreter.hpp"

#include "archive/MemoryBuffer.hpp"

#include "common/soot/Errors.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/PrettyPrinter.hpp"
#include "common/soot/Printer.hpp"
#include <cstddef>
#include <iostream>

#include "common/type_system/Defenum.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/type_system/Register.hpp"

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
#include "repl/config.h"
#include "soot/archive/MemoryArchive.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSpec.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>

namespace soot {

Interpreter::Interpreter(const std::string &username, bool load_libs, bool init_type_system, SootPlatform platform)
    : m_setter_map(), m_reader() {

    // Инициализируем boolean объекты как символы
    m_obj_null = Object::make_null();
    m_obj_none = Object::make_none();
    m_sym_continue_error = Object::make_symbol(&symbol_table(), ":continue");
    m_sym_true = symbol_table().core.sym_true;
    m_sym_false = symbol_table().core.sym_false;
    m_symbol_true = m_sym_true.as_symbol().name_ptr;
    m_symbol_false = m_sym_false.as_symbol().name_ptr;

    // Создаем глобальное окружение
    m_global_environment = EnvironmentObject::make_new("global");
    m_global_environment.as_env()->is_global = true;

    define_var_in_env(m_global_environment, m_obj_null, "null");
    define_var_in_env(m_global_environment, m_obj_none, "none");
    define_var_in_env(m_global_environment, m_sym_true, "else");
    define_var_in_env(m_global_environment, m_global_environment, "*global-env*");

    auto user = intern(username.c_str());
    define_var_in_env(m_global_environment, user, "*user*");

    // Разрешить использование неограниченого числа ключей
    ArgumentSpec args_with_varkeys(true, true);

    // === СПЕЦИАЛЬНЫЕ ФОРМЫ (не вычисляют аргументы) ===
    init_special_forms({
        {"define", &Interpreter::eval_define_special, nullptr},
        {"quote", &Interpreter::eval_quote_special, nullptr},
        {"set!", &Interpreter::eval_set_special, nullptr},
        {"let", &Interpreter::eval_let_special, nullptr},
        {"let*", &Interpreter::eval_let_star_special, nullptr},
        {"rlet", &Interpreter::eval_rlet_special, nullptr},
        {"lambda", &Interpreter::eval_lambda_special, nullptr},
        {"cond", &Interpreter::eval_cond_special, nullptr},
        {"or", &Interpreter::eval_or_special, nullptr},
        {"and", &Interpreter::eval_and_special, nullptr},
        {"macro", &Interpreter::eval_macro_special, nullptr},
        {"quasiquote", &Interpreter::eval_quasiquote_special, nullptr},
        {"while", &Interpreter::eval_while_special, nullptr},
        // Declare new types
        {"declare", &Interpreter::eval_declare_special, nullptr},

        // Define new constrant
        {"defconstant", &Interpreter::eval_define_constant, nullptr},
        // Parse type object
        {"deftype", &Interpreter::eval_deftype_special, nullptr},
        {"defenum", &Interpreter::eval_defenum_special, nullptr},
        {"deftypespec", &Interpreter::eval_deftypespec_special, nullptr},
        // Dereference
        {"->", &Interpreter::eval_deref_special, nullptr},
        // Exception handling
        {"with-error-handler", &Interpreter::eval_with_error_handler_special, nullptr},
        {"eval-args", &Interpreter::eval_eval_args_special, nullptr},
    });

    // === ВСТРОЕННЫЕ ФУНКЦИИ (вычисляют аргументы) ===
    init_builtin_forms({

        // Moved from special
        {"top-level", &Interpreter::eval_begin, nullptr}, // top level evaluation
        {"begin", &Interpreter::eval_begin, nullptr},

        // Работа с окружением
        {"env", &Interpreter::eval_env, nullptr},
        {"defined?", &Interpreter::eval_defined_p, nullptr},
        {"lookup", &Interpreter::eval_lookup, nullptr},
        {"lookup-type", &Interpreter::eval_lookup_type, nullptr},
        {"current-function", &Interpreter::eval_current_function, nullptr},
        {"gensym", &Interpreter::eval_gensym, nullptr},
        {"env-name", &Interpreter::eval_env_name_get, nullptr},
        {"env-name!", &Interpreter::eval_env_name_set, nullptr},
        {"current-env", &Interpreter::eval_current_env, nullptr},

        // Сравнение
        {"eq?", &Interpreter::eval_equals, nullptr}, // было eval_eq

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

        // Математические функции
        {"abs", &Interpreter::eval_abs, nullptr},
        {"max", &Interpreter::eval_max, nullptr},
        {"min", &Interpreter::eval_min, nullptr},
        {"expt", &Interpreter::eval_expt, nullptr},
        {"sqrt", &Interpreter::eval_sqrt, nullptr},
        {"ash", &Interpreter::eval_ash, nullptr},
        {"crc32", &Interpreter::eval_crc32, nullptr},

        // Тригонометрия
        {"sin", &Interpreter::eval_sin, nullptr},
        {"cos", &Interpreter::eval_cos, nullptr},
        {"tan", &Interpreter::eval_tan, nullptr},
        {"atan", &Interpreter::eval_atan, nullptr},

        // Математика: округление и остаток
        {"floor", &Interpreter::eval_floor, nullptr},
        {"ceiling", &Interpreter::eval_ceiling, nullptr},
        {"round", &Interpreter::eval_round, nullptr},
        {"mod", &Interpreter::eval_mod, nullptr},
        {"abs", &Interpreter::eval_abs, nullptr},

        // Логические операции
        {"logand", &Interpreter::eval_logand, nullptr},
        {"logior", &Interpreter::eval_logior, nullptr},
        {"logxor", &Interpreter::eval_logxor, nullptr},
        {"lognot", &Interpreter::eval_lognot, nullptr},
        {"lshift", &Interpreter::eval_lshift, nullptr},
        {"rshift", &Interpreter::eval_rshift, nullptr},

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
        {"list-for-each", &Interpreter::eval_list_for_each, nullptr},
        {"list-for-each-pair", &Interpreter::eval_list_for_each_pair, nullptr},
        {"getf", &Interpreter::eval_getf, nullptr},
        {"assoc", &Interpreter::eval_assoc, nullptr},

        // Работа с типом
        {"type-of", &Interpreter::eval_type_of, nullptr},
        {"type?", &Interpreter::eval_type_p, nullptr},
        {"null?", &Interpreter::eval_null_p, nullptr},
        {"keyword?", &Interpreter::eval_keyword_p, nullptr},

        // Преобразования типов
        {"number->string", &Interpreter::eval_number_to_string, &args_with_varkeys},
        {"string->number", &Interpreter::eval_string_to_number, nullptr},
        {"char->integer", &Interpreter::eval_char_to_integer, nullptr},
        {"integer->char", &Interpreter::eval_integer_to_char, nullptr},
        {"string->symbol", &Interpreter::eval_string_to_symbol, nullptr},
        {"symbol->string", &Interpreter::eval_symbol_to_string, nullptr},

        // Строки
        {"string-append", &Interpreter::eval_string_append, nullptr},
        {"string-length", &Interpreter::eval_string_length, nullptr},
        {"string-ref", &Interpreter::eval_string_ref, nullptr},
        {"string-replace", &Interpreter::eval_string_replace, nullptr}, // было eval_substring
        {"string-substr", &Interpreter::eval_string_substr, nullptr},   // было eval_substring
        {"string-prefix?", &Interpreter::eval_string_starts_with, nullptr},
        {"string-suffix?", &Interpreter::eval_string_ends_with, nullptr},
        {"string-contains?", &Interpreter::eval_string_containsp, nullptr},
        {"string-split", &Interpreter::eval_string_split, nullptr},
        {"string-join", &Interpreter::eval_string_join, nullptr},
        {"string-to-lower", &Interpreter::eval_string_to_lower, nullptr},
        {"string-to-upper", &Interpreter::eval_string_to_upper, nullptr},
        {"string-titelize", &Interpreter::eval_string_titlize, nullptr},
        {"string-trim", &Interpreter::eval_string_trim, nullptr},
        {"string-rtrim", &Interpreter::eval_string_rtrim, nullptr},
        {"string-ltrim", &Interpreter::eval_string_ltrim, nullptr},
        {"string-trim-idents", &Interpreter::eval_string_trim_indents, nullptr},
        {"string-for-each", &Interpreter::eval_string_for_each, nullptr},

        // Векторы
        {"make-array", &Interpreter::eval_make_array, nullptr},
        {"array-ref", &Interpreter::eval_array_ref, nullptr},
        {"array-set!", &Interpreter::eval_array_set, nullptr},
        {"array-length", &Interpreter::eval_array_length, nullptr},
        {"array->list", &Interpreter::eval_array_to_list, nullptr},
        {"array-for-each", &Interpreter::eval_array_for_each, nullptr},

        // Хэш-таблицы
        {"make-hash-table", &Interpreter::eval_make_hash_table, &args_with_varkeys},
        {"hash-table-set!", &Interpreter::eval_hash_table_set, nullptr},
        {"hash-table-ref", &Interpreter::eval_hash_table_ref, nullptr},
        {"hash-table-contains?", &Interpreter::eval_hash_table_containsp, nullptr},
        {"hash-table-try-ref", &Interpreter::eval_hash_table_try_ref, nullptr},
        {"hash-table-length", &Interpreter::eval_hash_table_length, nullptr},
        {"hash-table->list", &Interpreter::eval_hash_table_to_list, nullptr},
        {"hash-table-for-each", &Interpreter::eval_hash_table_for_each, nullptr},

        // Universtal table для объектов подобных hash-table но не hash-table
        {"get-at", &Interpreter::eval_get_at, nullptr},
        {"set-at!", &Interpreter::eval_set_at, nullptr},

        // Системные и ввод-вывод
        {"print", &Interpreter::eval_print, nullptr},
        {"fmt", &Interpreter::eval_fmt, &args_with_varkeys},
        {"log", &Interpreter::eval_log, nullptr},

        // Получение информации для вывода
        {"pfmt", &Interpreter::eval_pfmt, &args_with_varkeys},
        {"inspect", &Interpreter::eval_inspect, nullptr},

        // Сообщение об ошибке
        {"error", &Interpreter::eval_error, &args_with_varkeys},

        // Отладка
        {"source-info", &Interpreter::eval_source_info, nullptr},
        {"get-context", &Interpreter::eval_get_context, nullptr},

        // Отладка macro system
        {"macroexpand", &Interpreter::eval_macroexpand, nullptr},

        // Итераторв
        {"type-for-each-field", &Interpreter::eval_type_for_each_field, nullptr},
        {"type-for-each-method", &Interpreter::eval_type_for_each_method, nullptr},

        // Evaluation and parsing
        {"eval", &Interpreter::eval_eval, &args_with_varkeys},

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

        // Reader
        {"set-macro-character", &Interpreter::eval_set_macro_character, nullptr},
        {"remove-macro-character", &Interpreter::eval_remove_macro_character, nullptr},
        {"get-macro-character", &Interpreter::eval_get_macro_character, nullptr},
        {"read", &Interpreter::eval_read, nullptr},
        {"read-char", &Interpreter::eval_read_char, nullptr},
        {"peek-char", &Interpreter::eval_peek_char, nullptr},
        {"read-delimited-list", &Interpreter::eval_read_delimited_list, nullptr},

        // System
        {"system", &Interpreter::eval_system, nullptr},
        {"exit", &Interpreter::eval_exit, nullptr},
        {"get-environment-variable", &Interpreter::eval_get_sys_env,
         nullptr}, // было eval_get_environment_variable

        // System & Time
        {"time-seconds", &Interpreter::eval_time_seconds, nullptr},
        {"time-milliseconds", &Interpreter::eval_time_milliseconds, nullptr},
        {"time-microseconds", &Interpreter::eval_time_microseconds, nullptr},
        {"time-nanoseconds", &Interpreter::eval_time_nanoseconds, nullptr},

        // Система геттеров и сеттеров
        {"defsetf", &Interpreter::eval_defsetf, nullptr},
        {"get-setter", &Interpreter::eval_get_setter, nullptr},

        // Low-level pointer operations
        {"&", &Interpreter::eval_addr_of, nullptr},
        {"&+", &Interpreter::eval_addr_plus, nullptr},

        // Memory access
        {"mem-ref", &Interpreter::eval_mem_get, nullptr},
        {"mem-set!", &Interpreter::eval_mem_set, nullptr},

        // Type system
        {"init-types", &Interpreter::eval_init_types, nullptr},

        // Type system -- Type navigation
        {"~>", &Interpreter::eval_deref, nullptr},
        {"typespec", &Interpreter::eval_typespec, nullptr},

        // Type system
        {"declarations", &Interpreter::eval_declarations, &args_with_varkeys},
        {"define-function", &Interpreter::eval_define_function, &args_with_varkeys},
        {"function-type-make", &Interpreter::eval_function_type_make, &args_with_varkeys},
        {"function-type", &Interpreter::eval_function_type_get, &args_with_varkeys},
        {"function-type!", &Interpreter::eval_function_type_set, &args_with_varkeys},
        {"function-name", &Interpreter::eval_function_name_get, &args_with_varkeys},
        {"function-name!", &Interpreter::eval_function_name_set, &args_with_varkeys},
        {"types-match?", &Interpreter::eval_types_match_p, &args_with_varkeys},
        {"declare-type", &Interpreter::eval_declare_type, nullptr},
        {"declare-external", &Interpreter::eval_declare_external, nullptr},
        // Methods and types
        {"define-method", &Interpreter::eval_define_method, &args_with_varkeys},
        {"method-id-of", &Interpreter::eval_method_id_of, nullptr},
        {"method-of", &Interpreter::eval_method_of, nullptr},
        {"offset-of", &Interpreter::eval_offset_of, nullptr},
        {"size-of", &Interpreter::eval_size_of, nullptr},

        // Register aliases
        {"reg", &Interpreter::eval_reg_alias, nullptr},

        // Type casting
        {"the", &Interpreter::eval_the, nullptr},
        {"the-as", &Interpreter::eval_the_as, nullptr},

        // Archive
        {"make-memory-archive", &Interpreter::eval_make_memory_archive, &args_with_varkeys},
        {"make-memory-region", &Interpreter::eval_make_memory_region, &args_with_varkeys},
        {"make-memory-buffer", &Interpreter::eval_make_memory_buffer, &args_with_varkeys},
        {"memory-region-export-hex", &Interpreter::eval_memory_region_export_hex,
         &args_with_varkeys},
        {"memory-region-dump", &Interpreter::eval_memory_region_dump, &args_with_varkeys},

        {"memory-buffer-label-ref", &Interpreter::eval_memory_buffer_label_ref, &args_with_varkeys},
        {"memory-buffer-label-set!", &Interpreter::eval_memory_buffer_label_set,
         &args_with_varkeys},
        {"memory-buffer-symbol-ref", &Interpreter::eval_memory_buffer_symbol_ref,
         &args_with_varkeys},
        {"memory-buffer-symbol-set!", &Interpreter::eval_memory_buffer_symbol_set, nullptr},
        {"memory-buffer-reloc-ref", &Interpreter::eval_memory_buffer_reloc_ref, &args_with_varkeys},
        {"memory-buffer-reloc-set!", &Interpreter::eval_memory_buffer_reloc_set, nullptr},
        {"memory-buffer-reloc-list", &Interpreter::eval_memory_buffer_reloc_list, nullptr},

        {"archive-serialize", &Interpreter::eval_archive_serialize, &args_with_varkeys},

        {"archive-tell", &Interpreter::eval_archive_tell, nullptr},
        {"archive-seek!", &Interpreter::eval_archive_seek, nullptr},
        {"archive-at-end?", &Interpreter::eval_archive_at_end, nullptr},
        {"archive-close!", &Interpreter::eval_archive_close, nullptr},
        {"memory-region-read-byte", &Interpreter::eval_region_read_byte, nullptr},
        {"memory-region-write-byte!", &Interpreter::eval_region_write_byte, nullptr},

    });

    // Type system
    if (init_type_system)
        init_types(platform);

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

    Object o = Object::make_heap_obj(sf_obj, ObjectType::SPECIAL_FORM);
    define_var_in_env(m_global_environment, o, name.c_str());
}

void Interpreter::add_builtin_form(std::string name, BuiltinFormMethod method,
                                   ArgumentSpec *specs) {
    auto builtin_obj =
        std::shared_ptr<BuiltinFunctionObject>(new BuiltinFunctionObject(method, specs, name));

    Object o = Object::make_heap_obj(builtin_obj, ObjectType::PRIMITIVE);
    define_var_in_env(m_global_environment, o, name.c_str());
}

void Interpreter::add_custom_form(std::string name, CustomFormMethod method,
                                   ArgumentSpec *specs) {
    auto builtin_obj =
        std::shared_ptr<CustomFunctionObject>(new CustomFunctionObject(method, specs, name));

    Object o = Object::make_heap_obj(builtin_obj, ObjectType::PRIMITIVE);
    define_var_in_env(m_global_environment, o, name.c_str());
}

/*!
 * Iterate through elements of a soot list and apply the given function. Throw compiler error if the
 * list is invalid.
 */
void Interpreter::for_each_in_list(const Object                              &list,
                                   const std::function<void(const Object &)> &f) {
    const Object *iter = &list;
    while (iter->is_pair()) {
        auto lap = iter->as_pair();
        f(lap->car);
        iter = &lap->cdr;
    }

    if (!iter->is_null()) {
        throw_eval_error(list, "Invalid list: {}", list.print());
    }
}

// ============================================================
// Environment
// ============================================================

bool Interpreter::try_symbol_lookup(const Object                             &sym,
                                    const std::shared_ptr<EnvironmentObject> &env, Object *dest) {
    // Boolean проверка
    if (sym.as_symbol().name_ptr == get_true().as_symbol().name_ptr ||
        sym.as_symbol().name_ptr == m_sym_false.as_symbol().name_ptr) {
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
    {
        Object *obj = m_global_constants.lookup(sym.as_symbol());
        if (obj) {
            *dest = *obj;
            return true;
        }
    }
    {
        auto type_ptr = m_symbol_types.lookup(sym.as_symbol()); // Получаем указатель из мапы
        if (type_ptr) {
            // 1. Разыменовываем указатель, получаем std::shared_ptr<TypeSpec>
            // 2. static_pointer_cast приводит его к std::shared_ptr<HeapObject>
            auto heap_ptr = std::static_pointer_cast<soot::HeapObject>(*type_ptr);

            // Теперь вызываем создание объекта
            *dest = Object::make_heap_obj(heap_ptr, ObjectType::NATIVE_OBJECT);
            return true;
        }
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
        env->vars.set(intern_ptr(arg_spec.unnamed.at(i).name), args.unnamed.at(i));
    }

    // named args
    for (const auto &kv : arg_spec.named) {
        env->vars.set(intern_ptr(kv.first), args.named.at(kv.first));
    }

    // rest args
    if (!arg_spec.rest.empty()) {
        // args.rest теперь сам по себе является списком Pair или Null.
        // Мы просто биндим его в окружение. Никаких копирований!
        if (args.rest.is_none())
            env->vars.set(intern_ptr(arg_spec.rest), get_null());
        else
            env->vars.set(intern_ptr(arg_spec.rest), args.rest);
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
    env.as_env()->vars.set(InternedSymbolPtr{intern_ptr(name)}, var);
}

// ============================================================
// Tools and utilities
// ============================================================

Object Interpreter::intern(const char *name) {
    return symbol_table().make_symbol(name);
}

/*!
 * Get a symbol with the given name, creating one if none exist.
 */
Object Interpreter::intern(const std::string &name) {
    return symbol_table().make_symbol(name.c_str());
}

InternedSymbolPtr Interpreter::intern_ptr(const std::string &name) {
    return symbol_table().intern(name.c_str());
}

// ============================================================
// REPL
// ============================================================

/*!
 * Display the REPL, which will run until the user executes exit.
 */
void Interpreter::execute_repl(REPL::Wrapper& repl) {
    std::string input;

    while (true) {
        std::cout.flush();
        try {
            auto obj = m_reader.read_from_stdin("soot> ", repl);
            if (!obj) {
                continue;
            }
            // evaluate
            Object evald = eval_with_rewind(*obj, m_global_environment.as_env_ptr());
            // print
            printf("%s\n", evald.print().c_str());
        } catch (soot::ExitException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
            exit(e.exit_code);
        } catch (soot::EvalException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError:");
            fmt::print("Error: {}", e.full_report(m_reader));
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
        }
    }
}

/*!
 * Signal an evaluation error. This throws an exception which will unwind the evaluation stack
 * for debugging.
 */
void Interpreter::throw_eval_error(const Object &o, const std::string &err) {
    throw EvalException(o, err);
}

void Interpreter::throw_arity_mismatch(const Object &form, const Arguments &args, uint expected_min,
                                       uint expected_max, size_t got) {
    if (expected_min == expected_max) {
        throw_eval_error(form,
                         fmt::format("arity mismatch: expected {} arguments, but got {} in: {}",
                                     expected_min, got, args.print_full()));
    } else {
        throw_eval_error(form,
                         fmt::format("arity mismatch: expected {}-{} arguments, but got {} in: {}",
                                     expected_min, expected_max, got, args.print_full()));
    }
}

void Interpreter::throw_type_mismatch(const Object &form, const Arguments &args, uint index,
                                      const std::vector<ObjectType> &expected, ObjectType got) {
    std::string expected_str;
    for (size_t i = 0; i < expected.size(); ++i) {
        expected_str += object_type_to_string(expected[i]) + (i < expected.size() - 1 ? ", " : "");
    }
    throw_eval_error(
        form, fmt::format("type error at argument [{}]: expected one of [{}], but got [{}] in: {}",
                          index, expected_str, object_type_to_string(got), args.print_full()));
}

void Interpreter::throw_type_mismatch(const Object &form, const Arguments &args, uint index,
                                      std::initializer_list<const char *> expected,
                                      std::string                         got) {
    std::string expected_str;
    bool        first = true;
    for (const char *name : expected) {
        if (!first)
            expected_str += ", ";
        expected_str += name;
        first = false;
    }

    throw_eval_error(
        form, fmt::format("type error at argument [{}]: expected one of [{}], but got [{}] in: {}",
                          index, expected_str, got, args.print_full()));
}

void Interpreter::throw_missing_named_arg(const Object &form, const Arguments &args,
                                          const std::string &name) {
    throw_eval_error(form, fmt::format("Required named argument ':{}' is missing in: {}", name,
                                       args.print_full()));
}

void Interpreter::throw_unexpected_named_arg(const Object &form, const Arguments &args,
                                             const std::string &name) {
    throw_eval_error(
        form, fmt::format("unexpected named argument ':{}' in: {}", name, args.print_full()));
}

void Interpreter::throw_named_type_mismatch(const Object &form, const std::string &name,
                                            const std::vector<ObjectType> &expected,
                                            const Object                  &got) {
    std::string expected_str;
    for (size_t i = 0; i < expected.size(); ++i) {
        expected_str += object_type_to_string(expected[i]) + (i < expected.size() - 1 ? ", " : "");
    }
    throw_eval_error(
        form, fmt::format("type error for named argument ':{}': expected [{}], but got {} {}", name,
                          expected_str, object_type_to_string(got.type), got.print()));
}

void Interpreter::throw_named_type_mismatch(const Object &form, const std::string &name,
                                            std::initializer_list<const char *> expected,
                                            std::string                         got) {
    std::string expected_str;
    bool        first = true;
    for (const char *name : expected) {
        if (!first)
            expected_str += ", ";
        expected_str += name;
        first = false;
    }
    throw_eval_error(form,
                     fmt::format("type error for named argument ':{}': expected [{}], but got {}",
                                 name, expected_str, got));
}

/*!
 * If o isn't an environment object, throws an evaluation error on form.
 */
void Interpreter::expect_env(const Object &form, const Object &o) {
    if (!o.is_env()) {
        throw_eval_error(form, "object " + o.print() + " is a " + object_type_to_string(o.type) +
                                   " but was expected to be an environment");
    }
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
                // fmt::print("DEBUG: Interpreter::eval_string FORM_READ {} -> {}\n",>
                // evt.form.print(), last_result.print());
                break;
            case ReaderEvent::Type::MACRO_REQUEST:
                // fmt::print("DEBUG: Interpreter::eval_string MACRO_REQUEST {} \n",
                // evt.token.print());
                last_result = this->call_lambda_internal(
                    evt.form, evt.form, {evt.reader, evt.token},
                    m_global_environment.as_heap_obj<EnvironmentObject>());
                // fmt::print("DEBUG: Interpreter::eval_string MACRO_REQUEST {} -> {}\n",
                // evt.token.print(), last_result.print());
                break;
            default:
                throw std::runtime_error("undexpeted");
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
                last_result = this->call_lambda_internal(
                    evt.form, evt.form, {evt.reader, evt.token},
                    m_global_environment.as_heap_obj<EnvironmentObject>());
                // fmt::print("DEBUG: Interpreter::eval_file_internal MACRO_REQUEST {} -> {}\n",
                // evt.token.print(), last_result.print());
                break;
            default:
                throw std::runtime_error("undexpeted");
                break;
            }

            return last_result;
        });
    return last_result;
}

// Same as method befor but for cases wher are no parent form
Object Interpreter::eval_form(const Object &obj, const std::shared_ptr<EnvironmentObject> &env) {
    return eval_with_rewind(obj, env);
}
Object Interpreter::eval_form(const Object &obj) {
    return eval_with_rewind(obj, m_global_environment.as_env_ptr());
}

Object Interpreter::call_lambda_internal(const Object &form, const Object &lambda,
                                         const std::vector<Object>                &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!lambda.is_function()) {
        throw std::runtime_error("call_lambda: object is not a lambda");
    }

    const auto &lam = lambda.as_function();

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
    auto lam_env_obj = EnvironmentObject::make_new("lambda-call", nullptr);
    auto lam_env = lam_env_obj.as_env_ptr();
    lam_env->parent_env = lam->parent_env;
    lam_env->is_function = true;
    lam_env->owner_function = lambda;
    lam_env->ctx = form;
    ASSERT(lam_env->owner_function.is_function());

    //  4. Биндим аргументы
    Object dummy_form = symbol_table().make_symbol("call-lambda");
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
void Interpreter::render_complex_error(EvalException &e) {
    fmt::print(fg(fmt::color::indian_red), "\n─── ERROR ──────────────────────────────────\n");
    fmt::print(fg(fmt::color::indian_red), "Error: {}\n", e.message);

    // 1. Печатаем первичный контекст (где именно рвануло)
    std::string primary_info = m_reader.get_db().get_info_for(e.form);
    if (primary_info != "?") {
        fmt::print("\n{}\n", primary_info);
    }

    // 2. Печатаем накопленный стек контекстов
    fmt::print(fg(fmt::color::dim_gray), "Traceback (most recent call last):\n");

    int depth = 0;
    for (const auto &frame : e.trace) {
        // Will get index of line and column
        auto        info = m_reader.get_db().get_short_info_for(frame.form);
        std::string obj_str = truncate_obj(frame.form.print(), 60);

        fmt::print(fg(fmt::color::dim_gray), "  [{:02d}] ", depth++);
        if (!frame.message.empty())
            fmt::print("{} ", frame.message);

        fmt::print("in {}", obj_str);
        if (info && info->line_number > 0) {
            fmt::print(" at {}:{:d}", info->filename, info->line_number);
        }
        fmt::print("\n");
    }
}
void Interpreter::print_form_info(const Object                             &form,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    // Получаем информацию о расположении формы в исходном коде
    const std::string info = m_reader.get_db().get_info_for(form);

    // Печатаем информацию только если она доступна
    if (!info.empty() && info != "?") {
        fmt::print("{}", info);

        // Выводим стек вызовов
        fmt::print("\nCall stack (most recent call last):\n");

        auto current_frame = env;
        int  depth = 0;

        while (current_frame != nullptr) {
            auto        frame_info_opt = m_reader.get_db().get_short_info_for(current_frame->ctx);
            std::string frame_repr = truncate_obj(current_frame->ctx.print(), 60);

            if (frame_info_opt && frame_info_opt->line_number > 0) {
                fmt::print("  [{:02d}] {} at {}:{:d}\n", depth, frame_repr,
                           frame_info_opt->filename, frame_info_opt->line_number);
            } else {
                fmt::print("  [{:02d}] {}\n", depth, frame_repr);
            }

            current_frame = current_frame->parent_env;
            depth++;
        }
    }
}

/*!
 * Evaluate the given expression, with a "checkpoint" in the evaluation stack here.  If there is
 * an evaluation error, there will be a print indicating there was an error in the evaluation of
 * "obj", and if possible what file/line "obj" comes from.
 */
Object Interpreter::eval_with_rewind(const Object                             &obj,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    try {
        m_dynamic_stack.push_back(env);
        auto res = eval(obj, env);
        m_dynamic_stack.pop_back();
        return res;
    } catch (EvalException &e) {
        if (!m_dynamic_stack.empty()) {
            m_dynamic_stack.pop_back();
        }
        if (e.env.get() == nullptr)
            e.env = env;

        // Добавляем текущий контекст в трассировку
        // Note может быть пустым или содержать имя функции из env
        if (e.stack_counter > 0)
            e.add_context(obj, env->name, env);
        e.stack_counter++;
        // ПРОВЕРКА ЛОВУШКИ (The Trap)
        // 2. Проверяем наличие ловушки
        if (env->error_handler.is_function()) {
            // Вызываем лямбду-обработчик
            // Передаем ей (msg err-form trace)
            Object response = call_error_handler(obj, env->error_handler, e, env);

            // СЛУЧАЙ 1: #f (null) - Ошибка погашена
            if (response.is_null()) {
                return response; // Просто возвращаем null, выполнение продолжается
            }

            // СЛУЧАЙ 2: Список (контекст сообщение)
            if (response.is_pair()) {
                Object new_ctx = response.as_pair()->car;
                Object next = response.as_pair()->cdr;

                std::string new_msg = "Additional context"; // default
                if (next.is_pair() && next.as_pair()->car.is_string()) {
                    new_msg = next.as_pair()->car.to_std_string();
                }

                // Модифицируем объект исключения:
                // Добавляем новый "этаж" информации, который мы получили из скрипта
                e.add_context(new_ctx, new_msg, env, true);

                // Если мы обновили контекст, мы почти всегда хотим лететь дальше вверх,
                // чтобы увидеть весь стек в итоге.
                throw;
            }

            // СЛУЧАЙ 3: #t (true) - Проброс без изменений
            if (is_true(response)) {
                throw; // Летим выше к следующему catch
            }

            // По умолчанию (если вернулось что-то иное) — гасим ошибку и возвращаем это значение
            return response;
        }

        // Если ловушки нет или она пропустила ошибку — летим дальше вверх
        throw;
    }
}

Object Interpreter::call_error_handler(const Object &form, const Object handler, EvalException &e,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Подготавливаем Traceback как список для Лиспа:
    // ((form1 . note1) (form2 . note2) ...)
    Object trace_list = Object::make_null();
    // Идем с конца в начало, чтобы в Лиспе список был от корня к ошибке (или наоборот, как тебе
    // удобнее)
    for (auto it = e.trace.rbegin(); it != e.trace.rend(); ++it) {
        Object frame_cell = Object::make_pair(it->form, Object::make_string(it->message));
        trace_list = Object::make_pair(frame_cell, trace_list);
    }

    // 2. Аргументы для лямбды:
    // Арг 0: Сообщение (string)
    // Арг 1: Форма ошибки (object)
    // Арг 2: Весь накопленный стек (list)
    std::vector<Object> args = {Object::make_string(e.message), e.form, trace_list};

    // Используем твой существующий метод
    // В качестве 'form' передаем саму лямбду или пустой символ
    return call_lambda_internal(form, handler, args, env);
}

void Interpreter::print_stack_frame(EvalException &e, const Object &obj) {
    int         max_size = 80;
    std::string obj_str = truncate_obj(obj.print(), max_size);
    auto        info_opt = m_reader.get_db().get_short_info_for(obj);

    // Цвет для мета-информации (путь к файлу)
    auto dim_color = fg(fmt::color::dim_gray);

    if (info_opt && info_opt->line_number > 0) {
        fmt::print(dim_color, "  [{:02d}] in {} ", e.stack_counter, obj_str);
        fmt::print(dim_color, "at {}:{:d}\n", info_opt->filename, info_opt->line_number);
    } else {
        fmt::print(dim_color, "  [{:02d}] in {}\n", e.stack_counter, obj_str);
    }
}
// ============================================================
// Eval (Single Item)
// ============================================================

Object Interpreter::eval(const Object &obj, const std::shared_ptr<EnvironmentObject> &env) {
    switch (obj.type) {
    case ObjectType::SYMBOL:
        if (obj.is_keyword())
            return obj;
        else
            return eval_symbol(obj, env);
    case ObjectType::PAIR:
        return eval_pair(obj, env);
    case ObjectType::POINTER:
        return obj.as_pointer()->deref();
    case ObjectType::NATIVE_OBJECT:
        return obj;
    case ObjectType::INT:
    case ObjectType::FLOAT:
    case ObjectType::STRING:
    case ObjectType::CHAR:
    case ObjectType::EMPTY_LIST:
    case ObjectType::ARRAY:
    case ObjectType::HASH_TABLE:
    case ObjectType::READER:
    case ObjectType::FUNCTION:
        return obj;
    default:
        throw_eval_error(obj, fmt::format("cannot evaluate this object '{}'", obj.print()));
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
        result.push_back(eval_with_rewind(current.as_pair()->car, env));
        current = current.as_pair()->cdr;
    }

    if (!current.is_null()) {
        throw_eval_error(list, "malformed argument list");
    }

    return result;
}

/*!
 * Evaluate a list and return the result of the last evaluation.
 */
Object Interpreter::eval_list_return_last(const Object &form, Object rest,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    if (rest.is_null()) {
        return rest;
    }
    const Object *iter = &rest;
    while (true) {
        const Object *next = &iter->as_pair()->cdr;
        const Object *item = &iter->as_pair()->car;

        if (next->is_null()) {
            return eval_with_rewind(*item, env);
        } else {
            eval_with_rewind(*item, env);
            iter = next;
        }
    }
}

Object Interpreter::eval_symbol(const Object &sym, const std::shared_ptr<EnvironmentObject> &env) {
    Object result;
    if (!try_symbol_lookup(sym, env, &result)) {
        throw EvalException(sym, "Unbound variable: " +
                                     std::string(std::string(sym.as_symbol().c_str())));
    }
    return result;
}

Object Interpreter::eval_pair(const Object &obj, const std::shared_ptr<EnvironmentObject> &env) {
    const auto   &pair = obj.as_pair();
    const Object &head = pair->car;
    const Object &rest = pair->cdr;

    // 1. Вычисляем голову. Благодаря тому, что примитивы и спецформы теперь в Environment,
    // этот вызов вернет нам соответствующий HeapObject (SpecialForm, Primitive, Lambda или
    // Macro).
    Object eval_head = eval_with_rewind(head, env);

    // 2. Диспетчеризация по типу вычисленного объекта

    // --- SPECIAL FORMS (if, define, quote, set! ...) ---
    if (eval_head.is_special_form()) {
        // Получаем доступ к методу через native_ref
        auto spec_form = eval_head.as_heap_obj<SpecialFormObject>();
        // Передаем 'rest' как есть (без вычисления аргументов)
        return ((*this).*(spec_form->method))(obj, rest, env);
    }

    // --- PRIMITIVES (+, -, print, segment-get-abs-pc ...) ---
    if (eval_head.is_primitive()) {
        auto builtin = eval_head.as_heap_obj<BuiltinFunctionObject>();
        if (builtin) {
            // Сначала собираем аргументы из списка 'rest' и проверяем количество (specs)
            Arguments args = get_args(obj, rest, builtin->specs);
            // Примитивы всегда требуют вычисленных аргументов
            eval_args(obj, &args, env);
            m_profiler.begin(eval_head.print().c_str());
            auto result = ((*this).*(builtin->method))(obj, args, env);
            m_profiler.end();
            return result;
        } 

        auto custom = eval_head.as_heap_obj<CustomFunctionObject>();
        if (custom) {
            // Сначала собираем аргументы из списка 'rest' и проверяем количество (specs)
            Arguments args = get_args(obj, rest, custom->specs);
            // Примитивы всегда требуют вычисленных аргументов
            eval_args(obj, &args, env);
            m_profiler.begin(eval_head.print().c_str());
            auto result = custom->method(obj, args, env);
            m_profiler.end();
            return result;
        }

        throw_eval_error(obj,
                     "The primitive is not builting or custom function: " + eval_head.class_name() + " " + eval_head.print());
    }

    // --- MACROS ---
    if (eval_head.is_macro()) {
        const auto &macro = eval_head.as_macro();
        // Макросы получают аргументы как код (не вычисляем их здесь)
        Arguments args = get_args_with_spec(obj, rest, macro->args);

        auto mac_env = std::make_shared<EnvironmentObject>();
        mac_env->ctx = obj;
        mac_env->is_function = false; // MACRO is not function
        mac_env->parent_env = env;    // Динамическое или лексическое — на твой вкус, обычно env
        set_args_in_env(obj, args, macro->args, mac_env);

        // ШАГ 1: Запускаем программу-макрос, чтобы она создала (выпекла) код.
        Object expansion = eval_list_return_last(macro->body, macro->body, mac_env);
        // ШАГ 2: Выполняем то, что макрос нам вернул, в исходном окружении.
        m_profiler.begin(eval_head.print().c_str());
        auto result = eval_with_rewind(expansion, env);
        m_profiler.end();
        return result;
    }

    // --- LAMBDAS (User defined functions) ---
    if (eval_head.is_function()) {
        const auto &lam = eval_head.as_function();

        Arguments args = get_args_with_spec(obj, rest, lam->args);
        eval_args(obj, &args, env);

        auto lam_env_obj = EnvironmentObject::make_new("lambda-call", nullptr);
        auto lam_env = lam_env_obj.as_env_ptr();
        lam_env->ctx = obj;
        lam_env->is_function = true;
        lam_env->owner_function = eval_head;
        ASSERT(lam_env->owner_function.is_function());
        // Лексическое связывание: используем окружение, где лямбда была создана
        lam_env->parent_env = lam->parent_env;

        set_args_in_env(obj, args, lam->args, lam_env);
        return eval_list_return_last(lam->body, lam->body, lam_env);
    }

    // 3. Если мы дошли сюда, значит голова — не функция и не спецформа
    throw_eval_error(obj,
                     "Object is not callable: " + eval_head.class_name() + " " + eval_head.print());
    return m_obj_null; // unreachable
}

// ============================================================
// The environment access
// ============================================================

/*!
 * Get current environment
 */
Object Interpreter::eval_env(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {}, {});
    return Object::make_heap_obj(env, ObjectType::ENVIRONMENT); // Твой #f / NIL
}

/*!
 * Get name of environment
 */
Object Interpreter::eval_env_name_get(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {}, {});
    return Object::make_symbol(&symbol_table(), env->name); // Твой #f / NIL
}

/*!
 * Mark environment with name
 */
Object Interpreter::eval_env_name_set(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});
    env->name = args.unnamed[0].to_std_string();
    return get_none();
}

/*!
 * Mark environment with name
 */
Object Interpreter::eval_current_env(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)args;
    return Object::make_heap_obj(env, ObjectType::ENVIRONMENT);
}

/*!
 * Check if symbol is defined
 */
Object Interpreter::eval_defined_p(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});
    auto   sym = args.unnamed.at(0);
    Object result;

    // Ищем символ в текущем и родительских окружениях
    if (try_symbol_lookup(sym, env, &result)) {
        return get_true(); // Твой #t / T
    }
    return m_sym_false; // Твой #f / NIL
}

/*!
 * Check if symbol is defined
 */
Object Interpreter::eval_lookup(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {

    // 1. ВЫЧИСЛЯЕМ аргумент.
    // Если в Лиспе написано (defined? x), мы должны вычислить 'x',
    // чтобы понять, проверяем ли мы символ 'foo или что-то другое.
    auto evaluated_name = eval_with_rewind(args.unnamed[0], env);

    // 2. Проверяем, что после вычисления мы получили символ
    if (!evaluated_name.is_symbol()) {
        // Если это не символ (например, число или строка),
        // технически оно "определено" как само себя, но обычно возвращают false
        return get_false();
    }

    auto name_sym = evaluated_name.as_symbol();

    // ПРОВЕРКА: Глобальные константы
    auto aconstant = m_global_constants.lookup(name_sym);
    if (aconstant) {
        return *aconstant;
    }

    auto define_env = env;
    if (args.has_named("env")) {
        auto result = eval_with_rewind(args.get_named("env"), env);
        expect_env(form, result);
        define_env = result.as_env_ptr();
    }

    Object result;
    // Ищем уже вычисленный символ в окружении
    if (try_symbol_lookup(evaluated_name, define_env, &result)) {
        return result;
    }
    return get_none();
}

/*!
 * Check if symbol is defined
 */
Object Interpreter::eval_lookup_type(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(
        form, args, {{ObjectType::SYMBOL}},
        {{"types", {false, {ObjectType::SYMBOL}}}, {"extern", {false, {ObjectType::SYMBOL}}}});

    auto type_name = args.unnamed[0].as_symbol();

    bool lookup_types = true;
    bool lookup_extern = true;
    if (args.has_named("types"))
        lookup_types = is_true(args.named["types"]);
    if (args.has_named("extern"))
        lookup_extern = is_true(args.named["extern"]);

    if (lookup_extern) {
        // 1. Сначала ищем в m_symbol_types
        auto type_ptr_ptr = m_symbol_types.lookup(type_name);
        if (type_ptr_ptr) {
            // *type_ptr_ptr - это shared_ptr<TypeSpec>
            auto heap_ptr = std::static_pointer_cast<soot::HeapObject>(*type_ptr_ptr);
            return Object::make_heap_obj(heap_ptr, ObjectType::NATIVE_OBJECT);
        }
    }

    if (lookup_types) {
        // 2. Если не нашли, ищем в TypeSystem
        // ПРЕДПОЛОЖИМ: lookup_type возвращает std::shared_ptr<Type>
        Type *type = TypeSystem::instance().lookup_type(type_name.as_string());
        if (type) {
            // Создаем shared_ptr с no-op deleter, потому что TypeSystem управляет памятью
            auto heap_ptr =
                std::shared_ptr<HeapObject>(static_cast<HeapObject *>(type), [](HeapObject *) {});
            return Object::make_heap_obj(heap_ptr, ObjectType::NATIVE_OBJECT);
        }
    }
    return get_none();
}

Object Interpreter::eval_current_function(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверка аргументов: (define-function ИМЯ ЛЯМБДА)
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});

    auto                               desc = args.unnamed[0].as_symbol();
    std::shared_ptr<EnvironmentObject> func_env;
    if (desc == "function") {
        // Find any function
        func_env = env->get_function_env();
        if (!func_env.get()) {
            throw_eval_error(form, "Environment does not have a function");
        }
    } else if (desc == "asm") {
        // Find any function
        func_env = env->get_declared_function_env();
        if (!func_env.get()) {
            throw_eval_error(form, "Environment does not have any asm-function");
        }
    }
    if (func_env->owner_function.is_none())
        throw_eval_error(form, "Environment does not have owner function");

    return func_env->owner_function;
}

// ============================================================
// The environment modifiers
// ============================================================

/*!
 * Define object
 */
Object Interpreter::eval_define_special(const Object &form, const Object &rest,
                                        const std::shared_ptr<EnvironmentObject> &env) {

    auto args = get_args(form, rest, ArgumentSpec(true, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {{"env", {false, {}}}});

    auto name = args.unnamed[0];
    // ПРОВЕРКА: Если мы в глобальном окружении, нельзя переопределять константу
    // (Или вообще запрещаем, если константы у нас только глобальные)
    if (m_global_constants.lookup(name.as_symbol())) {
        throw_eval_error(name, "Cannot define variable: symbol '" + name.to_std_string() +
                                   "' is already a constant");
    }

    auto define_env = env;
    if (args.has_named("env")) {
        auto result = eval_with_rewind(args.get_named("env"), env);
        expect_env(form, result);
        define_env = result.as_env_ptr();
    }

    Object value = eval_with_rewind(args.unnamed[1], env);
    define_env->vars.set(name.as_symbol(), value);
    return value;
}

/*!
 * Define constant
 */
Object Interpreter::eval_define_constant(const Object &form, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    auto args = get_args(form, rest, ArgumentSpec(true, true));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {{"env", {false, {}}}});
    auto name = args.unnamed[0].as_symbol();

    // Проверка: нельзя объявлять константу, если уже есть переменная с таким именем
    if (m_symbol_types.lookup(name))
        throw_eval_error(form, "Cannot define constant: symbol already has a type definition");

    Object value = eval_with_rewind(args.unnamed[1], env);

    // Определяем тип константы автоматически
    // Например, если это число < 256, тип может быть uint8 и т.д.
    TypeSpec ts = deduct_type_for_constant_helper(value);

    m_global_constants.set(name, value);
    // Оборачиваем TypeSpec в shared_ptr для таблицы типов
    m_symbol_types.set(name, std::make_shared<TypeSpec>(ts));

    return get_none();
}

/*!
 * Convert the lisp object to the type systems type
 */
TypeSpec Interpreter::deduct_type_for_constant_helper(const Object &val) {
    if (val.is_integer()) {
        int64_t v = val.as_integer();

        // Маленькое положительное число -> uint8
        if (v >= 0 && v <= 255) {
            return TypeSystem::instance().make_typespec("uint8");
        }
        // Маленькое отрицательное число -> int8
        if (v >= -128 && v < 0) {
            return TypeSystem::instance().make_typespec("int8");
        }
        // Всё остальное, что влезает в 16 бит
        return TypeSystem::instance().make_typespec("int16");
    }

    if (val.is_string()) {
        return TypeSystem::instance().make_typespec("string");
    }

    if (val.is_symbol()) {
        return TypeSystem::instance().make_typespec("symbol");
    }

    // По умолчанию, если не знаем что это
    return TypeSystem::instance().make_typespec("object");
}

/*!
 * Set object
 */
Object Interpreter::eval_set_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    auto args = get_args(form, rest, ArgumentSpec(true, false));
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {});
    auto   to_define = args.unnamed.at(0);
    Object to_set = eval_with_rewind(args.unnamed.at(1), env);

    std::shared_ptr<EnvironmentObject> search_env = env;
    for (;;) {
        if (search_env->is_global) {
            // ПРОВЕРКА: Нельзя менять константу через set!
            if (m_global_constants.lookup(to_define.as_symbol())) {
                throw_eval_error(to_define, "Cannot set! constant: '" + to_define.to_std_string() +
                                                "' is immutable");
            }
        }

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

    Object lambda_obj = FunctionObject::make_new();
    auto   lambda = lambda_obj.as_function();

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
    auto args = get_args_no_named(form, rest, ArgumentSpec(true, false));
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

/*!
 * Begin form
 */
Object Interpreter::eval_begin(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty()) {
        throw_eval_error(form, "begin form cannot have keyword arguments");
    }

    if (args.unnamed.empty()) {
        return Object::make_null();
    } else {
        return args.unnamed.back();
    }
}

Object Interpreter::eval_cond_special(const Object &form, const Object &rest,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    if (rest.type != ObjectType::PAIR)
        throw_eval_error(form, "cond must have at least one clause, which must be a form");
    Object result;

    Object lst = rest;
    for (;;) {
        if (lst.type == ObjectType::PAIR) {
            Object current_case = lst.as_pair()->car;
            if (current_case.type != ObjectType::PAIR)
                throw_eval_error(lst, "bogus cond case");

            // check condition:
            Object condition_result = eval_with_rewind(current_case.as_pair()->car, env);
            if (is_true(condition_result)) {
                if (current_case.as_pair()->cdr.type == ObjectType::EMPTY_LIST) {
                    return condition_result;
                }
                // got a match!
                return eval_list_return_last(current_case, current_case.as_pair()->cdr, env);
            } else {
                // no match, continue.
                lst = lst.as_pair()->cdr;
            }
        } else if (lst.type == ObjectType::EMPTY_LIST) {
            return m_sym_false;
        } else {
            throw_eval_error(form, "malformed cond");
        }
    }
}

Object Interpreter::eval_or_special(const Object &form, const Object &rest,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    Object current = rest;

    while (current.is_pair()) {
        Object result = eval_with_rewind(current.as_pair()->car, env);
        if (is_true(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return m_sym_false;
}

Object Interpreter::eval_and_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    Object current = rest;
    Object result = get_true();

    while (current.is_pair()) {
        result = eval_with_rewind(current.as_pair()->car, env);
        if (!is_true(result)) {
            return result;
        }
        current = current.as_pair()->cdr;
    }

    return result;
}

// ============================================================
// LET & RLET
// ============================================================

Object Interpreter::eval_let_special(const Object &form, const Object &rest,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    return eval_let_common_special(form, rest, env, false);
}

Object Interpreter::eval_let_star_special(const Object &form, const Object &rest,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    return eval_let_common_special(form, rest, env, true);
}

Object Interpreter::eval_let_common_special(const Object &form, const Object &rest,
                                            const std::shared_ptr<EnvironmentObject> &env,
                                            bool                                      is_star) {
    if (!rest.is_pair()) {
        throw_eval_error(form, "first argument to let must be bindings");
    }

    // Проверяем, есть ли имя у let-блока
    std::string block_name = "";
    Object      current_rest = rest;

    // Первый аргумент может быть именем (символ, строка или quoted символ)
    auto first = current_rest.as_pair()->car;

    // Распаковываем quote если есть
    Object name_obj = first;
    if (first.is_pair() && first.as_pair()->car.is_symbol("quote")) {
        // Это (quote name) - берем внутренний аргумент
        if (first.as_pair()->cdr.is_pair()) {
            name_obj = first.as_pair()->cdr.as_pair()->car;
        }
    }

    // Проверяем, является ли распакованный объект именем
    if (name_obj.is_symbol() || name_obj.is_string()) {
        block_name = name_obj.to_std_string();
        current_rest = current_rest.as_pair()->cdr;

        if (!current_rest.is_pair()) {
            throw_eval_error(form, "let requires bindings after name");
        }
    }

    // Далее проверяем биндинги
    const auto *bindings_iter = &current_rest.as_pair()->car;
    const auto *body_iter = &current_rest.as_pair()->cdr;

    // Проверка, что биндинги - это список
    if (!bindings_iter->is_pair() && !bindings_iter->is_null()) {
        throw_eval_error(form, "let bindings must be a list");
    }

    std::shared_ptr<EnvironmentObject> new_env = std::make_shared<EnvironmentObject>();
    new_env->ctx = form;
    new_env->parent_env = env;
    new_env->name = block_name;

    m_dynamic_stack.push_back(new_env);
    try {
        // Обрабатываем биндинги
        while (bindings_iter->is_pair()) {
            const auto *binding = &bindings_iter->as_pair()->car;

            // Каждый биндинг должен быть парой (variable value)
            if (!binding->is_pair()) {
                throw_eval_error(form,
                                 fmt::format("Invalid binding: expected (variable value), got {}",
                                             binding->print()));
            }

            const auto &name = binding->as_pair()->car;
            if (!name.is_symbol()) {
                throw_eval_error(
                    form, fmt::format("Binding name must be a symbol, got {}", name.print()));
            }

            const auto &value_part = binding->as_pair()->cdr;
            if (!value_part.is_pair() || !value_part.as_pair()->cdr.is_null()) {
                throw_eval_error(
                    form, fmt::format("Invalid binding format for {}: expected ({} value), got {}",
                                      name.print(), name.print(), binding->print()));
            }

            const auto &value = value_part.as_pair()->car;

            // Вычисляем значение в правильном окружении
            Object evaluated_value;
            if (is_star) {
                // Для let* каждое следующее выражение видит предыдущие
                evaluated_value = eval(value, new_env);
            } else {
                // Для let все выражения вычисляются в исходном окружении
                evaluated_value = eval(value, env);
            }

            new_env->vars.set(name.as_symbol(), evaluated_value);

            bindings_iter = &bindings_iter->as_pair()->cdr;
        }

        // Проверяем, что после биндингов нет мусора
        if (!bindings_iter->is_null()) {
            throw_eval_error(form, "Malformed let: unexpected elements after bindings");
        }

        auto res = eval_list_return_last(form, *body_iter, new_env);
        if (!m_dynamic_stack.empty())
            m_dynamic_stack.pop_back();

        return res;
    } catch (EvalException &e) {
        if (!m_dynamic_stack.empty())
            m_dynamic_stack.pop_back();
        throw;
    }
}

/*!
 * Same as the `let` but with extended synatax each slot will be
 * created as `reg` container. (rlet ((name type reg) ...) )
 * where is:
 *   @name - is the name in environment
 *   @type - a type of the value (a type in the type system)
 *   @reg  - the register alias: af, hl, ix etc
 */
Object Interpreter::eval_rlet_special(const Object &form, const Object &rest,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;

    if (!rest.is_pair())
        throw_eval_error(form, "rlet requires name or bindings");

    Object      current_rest = rest;
    std::string env_name = "";
    auto        first = current_rest.as_pair()->car;
    // 1. Проверяем, не является ли первый аргумент именем (строкой)
    if (first.is_symbol() || first.is_string()) {
        env_name = first.to_std_string();
        current_rest = current_rest.as_pair()->cdr;
    }

    if (!current_rest.is_pair())
        throw_eval_error(form, "rlet requires bindings");
    Object bindings = current_rest.as_pair()->car;
    Object body = current_rest.as_pair()->cdr;

    // Создаем окружение
    auto new_env = std::make_shared<EnvironmentObject>(env);
    new_env->name = env_name;
    new_env->is_reg_let = true;
    new_env->parent_env = env;
    m_dynamic_stack.push_back(new_env);
    try {
        int current_idx = 0; // Инициализируем счетчик
        // Используем helper для обхода списка
        for_each_in_list(bindings, [&](const Object &binding) {
            if (!binding.is_pair())
                throw_eval_error(binding, "Invalid rlet binding");
            auto name_obj = binding.as_pair()->car;
            auto rest_obj = binding.as_pair()->cdr;

            if (!name_obj.is_symbol())
                throw_eval_error(
                    form, fmt::format("Binding name must be a symbol `{}`", name_obj.print()));
            if (!rest_obj.is_pair())
                throw_eval_error(form,
                                 fmt::format("Binding name must be a pair `{}`", rest_obj.print()));
            // Создаем shared_ptr на Register
            auto alias = std::make_shared<Register>();
            alias->name = name_obj;
            alias->type_name = rest_obj.as_pair()->car;

            // --- ДОБАВЛЯЕМ РАСЧЕТ РАЗМЕРА ---
            auto type_ptr = TypeSystem::instance().lookup_type(alias->type_name.to_std_string());
            if (type_ptr) {
                if (type_ptr->get_name() == "_type_") {
                    alias->bit_size = TypeConfig::pointer_size * 8;
                    // Явно проверяем на "none" или "void"
                } else if (type_ptr->get_name() == "none") {
                    alias->bit_size = 0;
                } else if (type_ptr->get_name() == "void") {
                    alias->bit_size = 0;
                } else {
                    alias->bit_size = type_ptr->get_size_in_memory() * 8;
                }
            } else {
                // Если тип не найден, можно либо кинуть ошибку,
                // либо поставить 0 (но тогда ассемблер упадет позже)
                alias->bit_size = 0;
            }

            Object current = rest_obj.as_pair()->cdr;

            // ПРОВЕРКА: Если следующий элемент НЕ ключевое слово, считаем его регистром
            if (current.is_pair()) {
                Object next = current.as_pair()->car;
                if (!next.is_keyword()) {
                    alias->reg = next;
                    current = current.as_pair()->cdr; // Потребляем этот элемент
                }
            }

            while (current.is_pair()) {
                Object key = current.as_pair()->car;
                current = current.as_pair()->cdr;

                if (!current.is_pair())
                    throw_eval_error(key, "Missing value for keyword");
                Object val = current.as_pair()->car;
                current = current.as_pair()->cdr;

                if (key.is_symbol()) {
                    std::string k = key.print();
                    if (k == ":reg") {
                        alias->reg = val;
                    } else if (k == ":offset") {
                        alias->offset = (int)val.as_integer();
                    } else {
                        throw_eval_error(binding,
                                         "Expected :type, :reg, :offset, :source, but got " +
                                             val.print());
                    }
                }
            }

            // Сохраняем алиас (alias уже является shared_ptr, так что все правильно)
            // new_env->set_at(name_sym, Object::make_native_ref(alias));
            alias->arg_index = current_idx++;
            // Регистрируем в обычном окружении, чтобы символ 'pp возвращал 'r13
            auto name_sym = name_obj.as_symbol();
            new_env->vars.set(name_sym, Object::make_heap_obj(alias));
        });
        // Регистрируем окружение в родительском
        if (!env_name.empty())
            env->vars.set(Object::intern(&symbol_table(), env_name.c_str()),
                          Object::make_heap_obj(new_env, ObjectType::ENVIRONMENT));

        auto res = eval_list_return_last(body, body, new_env);
        if (!m_dynamic_stack.empty())
            m_dynamic_stack.pop_back();
        return res;
    } catch (EvalException &e) {
        if (!m_dynamic_stack.empty())
            m_dynamic_stack.pop_back();
        throw;
    }
}

// ============================================================
// Whitle
// ============================================================

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
        Object condition_result = eval_with_rewind(condition_obj, env);

        if (!is_true(condition_result)) {
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

Object Interpreter::build_list_with_links(std::vector<QuasiquoteEntry> &&entries, Object tail) {
    Object current_tail = tail;

    for (i64 i = (i64)entries.size() - 1; i >= 0; --i) {
        Object new_pair;
        new_pair.type = ObjectType::PAIR;
        new_pair.heap_obj = std::make_shared<PairObject>(entries[i].value, current_tail);

        // Если у нас есть сохраненная оригинальная ячейка - копируем линк
        if (entries[i].origin_cons.is_pair()) {
            m_reader.get_db().copy_link(entries[i].origin_cons, new_pair);
        }

        current_tail = new_pair;
    }
    return current_tail;
}

Object Interpreter::eval_unquote_arg(const Object                             &item,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    // item — это список вида (unquote <expression>)
    const Object &cdr = item.as_pair()->cdr;

    // Проверка: (unquote) — ошибка
    if (cdr.type == ObjectType::EMPTY_LIST) {
        throw_eval_error(item, "unquote must have exactly 1 argument, got 0");
    }

    // Проверка: (unquote a b) — ошибка
    if (cdr.as_pair()->cdr.type != ObjectType::EMPTY_LIST) {
        throw_eval_error(item, "unquote must have exactly 1 argument, got more");
    }

    // Вычисляем аргумент (car от cdr)
    return eval_with_rewind(cdr.as_pair()->car, env);
}

/*!
 * Recursive quasi-quote evaluation
 */
Object Interpreter::quasiquote_helper(const Object                             &form,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    const Object                *lst_iter = &form;
    std::vector<QuasiquoteEntry> entries;

    for (;;) {
        if (lst_iter->type == ObjectType::PAIR) {
            const Object &current_cons = *lst_iter; // Сохраняем текущую ячейку шаблона
            const Object &item = current_cons.as_pair()->car;

            if (item.type == ObjectType::PAIR) {
                const Object &head = item.as_pair()->car;

                // --- UNQUOTE ---
                if (head.is_symbol("unquote")) {
                    Object val = eval_unquote_arg(item, env); // Вспомогательная проверка аргументов
                    entries.push_back({val, current_cons});
                    lst_iter = &lst_iter->as_pair()->cdr;
                    continue;
                }

                // --- UNQUOTE-SPLICING ---
                else if (head.is_symbol("unquote-splicing")) {
                    Object splice_list = eval_unquote_arg(item, env);

                    if (splice_list.is_pair()) {
                        const Object *splice_iter = &splice_list;
                        bool          first = true;
                        while (splice_iter->is_pair()) {
                            // Копируем линк только для первого элемента сплайсинга
                            // Остальные элементы "плывут" следом
                            entries.push_back(
                                {splice_iter->as_pair()->car, first ? current_cons : Object()});
                            splice_iter = &splice_iter->as_pair()->cdr;
                            first = false;
                        }
                    } else if (!splice_list.is_null()) {
                        throw_eval_error(form, "unquote-splicing must return a list");
                    }

                    lst_iter = &lst_iter->as_pair()->cdr;
                    continue;
                }
            }

            // --- ОБЫЧНЫЙ ЭЛЕМЕНТ ---
            Object processed;
            if (item.is_pair()) {
                processed = quasiquote_helper(item, env);
            } else {
                processed = item;
            }

            entries.push_back({processed, current_cons});
            lst_iter = &lst_iter->as_pair()->cdr;

        } else if (lst_iter->type == ObjectType::EMPTY_LIST) {
            return build_list_with_links(std::move(entries), Object::make_null());
        } else {
            // Это случай (a . b), где lst_iter указывает на b (не Pair и не Nil)
            Object last_val;
            // Если хвост — это тоже пара (например, вложенный список), обрабатываем рекурсивно
            if (lst_iter->is_pair()) {
                last_val = quasiquote_helper(*lst_iter, env);
            } else {
                last_val = *lst_iter;
            }
            return build_list_with_links(std::move(entries), last_val);
        }
    }
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
        if (spec.varkeys && arg.is_keyword()) {
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
        if (is_keyword && spec.varkeys) {
            // неограниченое число ключей
            auto key_name = std::string(arg.as_symbol().name_ptr + 1);
            // Переходим к значению
            current = &current->as_pair()->cdr;
            if (current->is_null()) {
                throw_eval_error(form, fmt::format("Key {} is missing a value in {}", key_name,
                                                   spec.print_full()));
            }

            args.named[key_name] = current->as_pair()->car;
        } else if (is_keyword && !spec.named.empty()) {
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
    (void)parent_form;
    for (auto &arg : args->unnamed) {
        arg = eval_with_rewind(arg, env);
    }

    for (auto &kv : args->named) {
        kv.second = eval_with_rewind(kv.second, env);
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
            Object evaluated_val = eval_with_rewind(pair->car, env);

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
            spec.varkeys = true;
            current = current.as_pair()->cdr;
            continue; // Идем к следующему элементу после "&key"
        }

        if (is_sym && arg_name == "&optional") {
            parsing_optional = true;
            parsing_keys = false;
            spec.varkeys = false;
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
            throw_arity_mismatch(form, args, unnamed.size(), unnamed.size(), args.unnamed.size());
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
                throw_type_mismatch(form, args, i, allowed_types, args.unnamed[i].type);
            }
        }
    }

    // 2. Проверка named аргументов
    for (const auto &[name, spec] : named) {
        auto        it = args.named.find(name);
        bool        required = spec.first;
        const auto &allowed_types = spec.second;

        if (required && it == args.named.end()) {
            throw_missing_named_arg(form, args, name);
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
                throw_named_type_mismatch(form, name, allowed_types, it->second);
            }
        }
    }

    // 3. Проверка лишних named
    for (const auto &[name, _] : args.named) {
        if (named.find(name) == named.end()) {
            throw_unexpected_named_arg(form, args, name);
        }
    }
}

// ============================================================
// Системные функции(print, pprint, inspect)
// ============================================================

/*!
 * Print the inspection of a form to stdout, including a newline.
 * Returns ()
 */
Object Interpreter::eval_inspect(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем, что передан хотя бы один аргумент для инспекции
    vararg_check(form, args, {{}}, {});

    // Получаем объект, который нужно проинспектировать
    const Object &target = args.unnamed.at(0);

    // Вызываем метод inspect, который теперь (благодаря нашим правкам)
    // возвращает структуру данных (List/Pair), а не строку.
    return target.inspect(&symbol_table());
}

/*!
 * Print the form to stdout, including a newline.
 * Returns ()
 */
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

/*!
 * Return preformated string from the s-expression
 * Returns string
 */
Object Interpreter::eval_pfmt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка аргументов
    vararg_check(form, args, {{}}, {{"width", {false, {ObjectType::INT}}}});

    auto width = 100;
    if (args.has_named("width"))
        width = (int)args.named["width"].as_integer();

    // 2. Получаем строковое представление объекта
    std::string object_repr = pretty_print::to_string(args.unnamed.at(0), width);

    return Object::make_string(object_repr);
}

Object Interpreter::eval_fmt(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "fmt must get at least destination and format-string");
    }

    auto dest = args.unnamed.at(0);
    auto format_str = args.unnamed.at(1);
    if (!format_str.is_string()) {
        throw_eval_error(form, "format string must be a string");
    }

    // 1. Собираем аргументы (теперь начинаем с индекса 2)
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    for (size_t i = 2; i < args.unnamed.size(); i++) {
        auto &arg = args.unnamed.at(i);
        if (arg.is_string())
            arg_store.push_back(arg.as_string()->data);
        else if (arg.is_symbol())
            arg_store.push_back(arg.as_symbol().c_str());
        else if (arg.is_integer())
            arg_store.push_back(arg.as_integer());
        else if (arg.is_float())
            arg_store.push_back(arg.as_float());
        else
            arg_store.push_back(arg.print());
    }

    try {
        // 2. Форматируем финальную строку
        auto formatted = fmt::vformat(format_str.as_string()->data, arg_store);

        // 3. Вывод или возврат строки
        if (is_true(dest)) {
            // Проверяем наличие именованного аргумента :color
            if (args.has_named("color")) {
                auto                color_val = args.named["color"];
                fmt::terminal_color text_color = fmt::terminal_color::white;

                if (color_val.is_string())
                    text_color = string_to_color(color_val.as_string()->data);
                else if (color_val.is_symbol())
                    text_color = string_to_color(color_val.as_symbol().c_str());

                fmt::print(fg(text_color), "{}", formatted); // Вывод с цветом
            } else {
                lg::print("{}", formatted); // Обычный вывод
            }
            return get_null();
        }
        return Object::make_string(formatted);
    } catch (std::runtime_error &e) {
        throw_eval_error(form, e.what());
    }
    return Object::make_none();
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

// ============================================================
// Errors from script and error handling
// ============================================================

/*!
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
    vararg_check(form, args, {{ObjectType::STRING}},
                 {{"ctx", {false, {ObjectType::PAIR, ObjectType::NONE, ObjectType::EMPTY_LIST}}}});

    std::string message = args.unnamed.at(0).as_string()->data;
    Object      context_form = form;
    // Если передан второй аргумент, используем его как "место преступления"
    // Иначе используем 'form' (всю строку вызова (error ..))
    if (args.has_named("ctx") && !args.named["ctx"].is_null() && !args.named["ctx"].is_none())
        context_form = args.named["ctx"];

    // Вызываем стандартный механизм исключений с учетом контекста
    throw_eval_error(context_form, message);

    return Object::make_null(); // Сюда мы никогда не дойдем
}

Object Interpreter::eval_with_error_handler_special(const Object &form, const Object &rest,
                                                    const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Проверка структуры (with-error-handler handler-fn . body)
    if (!rest.is_pair())
        throw_eval_error(form, "with-error-handler requires a handler function and body");

    Object handler_code = rest.as_pair()->car;
    Object body = rest.as_pair()->cdr;

    // 2. Вычисляем сам обработчик (лямбду) в текущем окружении
    Object handler_fn = eval(handler_code, env);

    // Проверка, что это действительно то, что можно вызвать
    if (!handler_fn.is_function()) { // или иная проверка на callable
        throw_eval_error(handler_code, "error-handler must be a lambda");
    }

    // 3. Создаем новое окружение (аналог rlet)
    auto new_env = std::make_shared<EnvironmentObject>(env);
    new_env->name = "error-handler-scope";
    new_env->parent_env = env;

    // ВАЖНО: Устанавливаем ловушку именно в это НОВОЕ окружение
    new_env->error_handler = handler_fn;

    // 4. Выполняем тело внутри этого окружения
    // Если внутри случится Exception, eval_with_rewind найдет new_env->error_handler
    return eval_list_return_last(body, body, new_env);
}

// ============================================================
// Математические функции с проверками
// ============================================================

/*!
 * Convert a number to an integer
 */
int64_t Interpreter::number_to_integer(const Object &obj) {
    switch (obj.type) {
    case ObjectType::INT:
        return obj.integer_obj.value;
    case ObjectType::FLOAT:
        return (int64_t)obj.float_obj.value;
    case ObjectType::CHAR:
        return (int8_t)obj.char_obj.value;
    default:
        throw_eval_error(obj, "object cannot be interpreted as a number!");
    }
    return 0;
}

/*!
 * Convert a number to floating point
 */
double Interpreter::number_to_float(const Object &obj) {
    switch (obj.type) {
    case ObjectType::INT:
        return obj.integer_obj.value;
    case ObjectType::FLOAT:
        return obj.float_obj.value;
    default:
        throw_eval_error(obj, "object cannot be interpreted as a number!");
    }
    return 0;
}

/*!
 * Convert number to template type.
 */
template <> FloatType Interpreter::number(const Object &obj) {
    return number_to_float(obj);
}

/*!
 * Convert number to template type.
 */
template <> IntType Interpreter::number(const Object &obj) {
    return number_to_integer(obj);
}

/*!
 * Template implementation of addition.
 */
template <typename T>
Object Interpreter::num_plus(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    T result = 0;
    for (const auto &arg : args.unnamed) {
        result += number<T>(arg);
    }
    return Object::make_number<T>(result);
}

/*!
 * Addition
 */
Object Interpreter::eval_plus(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    if (!args.named.empty() || args.unnamed.empty()) {
        // DYNAMIC
        return Object::make_integer(0);
        // PURITAN
        throw_eval_error(form, "+ must receive at least one unnamed argument!");
    }

    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_plus<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_plus<double>(form, args, env);

    default:
        throw_eval_error(form, "+ must have a numeric argument");
        return Object::make_null();
    }
}

/*!
 * Template implementation of multiplication.
 */
template <typename T>
Object Interpreter::num_times(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    T result = 1;
    for (const auto &arg : args.unnamed) {
        result *= number<T>(arg);
    }
    return Object::make_number<T>(result);
}

/*!
 * Multiplication
 */
Object Interpreter::eval_times(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    if (!args.named.empty() || args.unnamed.empty()) {
        // DYNAMIC
        return Object::make_integer(1);
        // PURITAN
        throw_eval_error(form, "* must receive at least one unnamed argument!");
    }

    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_times<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_times<double>(form, args, env);

    default:
        throw_eval_error(form, "* must have a numeric argument");
        return Object::make_null();
    }
}

/*!
 * Template implementation of subtraction.
 */
template <typename T>
Object Interpreter::num_minus(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    T result;
    if (args.unnamed.size() > 1) {
        result = number<T>(args.unnamed[0]);
        for (uint32_t i = 1; i < args.unnamed.size(); i++) {
            result -= number<T>(args.unnamed[i]);
        }
    } else {
        result = -number<T>(args.unnamed[0]);
    }
    return Object::make_number<T>(result);
}

/*!
 * Subtraction
 */
Object Interpreter::eval_minus(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    if (!args.named.empty() || args.unnamed.empty()) {
        // DYNAMIC
        return Object::make_integer(0);
        // PURITAN
        throw_eval_error(form, "- must receive at least one unnamed argument!");
    }

    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_minus<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_minus<double>(form, args, env);

    default:
        throw_eval_error(form, "- must have a numeric argument");
        return Object::make_null();
    }
}

/*!
 * Template implementation of division.
 */
template <typename T>
Object Interpreter::num_divide(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    (void)form;
    T result = number<T>(args.unnamed[0]) / number<T>(args.unnamed[1]);
    return Object::make_number<T>(result);
}

/*!
 * Division
 */
Object Interpreter::eval_divide(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}, {}}, {});
    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_divide<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_divide<double>(form, args, env);

    default:
        throw_eval_error(form, "/ must have a numeric argument");
        return Object::make_null();
    }
}

/*!
 * Compare numbers for equality
 */
Object Interpreter::eval_numequals(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.size() < 2) {
        throw_eval_error(form, "= must receive at least two unnamed arguments!");
    }

    bool result = true;
    switch (args.unnamed.front().type) {
    case ObjectType::INT: {
        int64_t ref = number_to_integer(args.unnamed.front());
        for (uint32_t i = 1; i < args.unnamed.size(); i++) {
            if (ref != number_to_integer(args.unnamed[i])) {
                result = false;
                break;
            }
        }
    } break;

    case ObjectType::FLOAT: {
        double ref = number_to_float(args.unnamed.front());
        for (uint32_t i = 1; i < args.unnamed.size(); i++) {
            if (ref != number_to_float(args.unnamed[i])) {
                result = false;
                break;
            }
        }
    } break;

    default:
        throw_eval_error(form, "= must have a numeric argument");
        return Object::make_null();
    }

    return true_or_false(result);
}

template <typename T>
Object Interpreter::num_lt(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    T a = number<T>(args.unnamed[0]);
    T b = number<T>(args.unnamed[1]);
    return true_or_false(a < b);
}

Object Interpreter::eval_lt(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}, {}}, {});
    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_lt<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_lt<double>(form, args, env);

    default:
        throw_eval_error(form, "< must have a numeric argument");
        return Object::make_null();
    }
}

template <typename T>
Object Interpreter::num_gt(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    T a = number<T>(args.unnamed[0]);
    T b = number<T>(args.unnamed[1]);
    return true_or_false(a > b);
}

Object Interpreter::eval_gt(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}, {}}, {});
    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_gt<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_gt<double>(form, args, env);

    default:
        throw_eval_error(form, "> must have a numeric argument");
        return Object::make_null();
    }
}

template <typename T>
Object Interpreter::num_leq(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    T a = number<T>(args.unnamed[0]);
    T b = number<T>(args.unnamed[1]);
    return true_or_false(a <= b);
}

Object Interpreter::eval_leq(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}, {}}, {});
    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_leq<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_leq<double>(form, args, env);

    default:
        throw_eval_error(form, "<= must have a numeric argument");
        return Object::make_null();
    }
}

template <typename T>
Object Interpreter::num_geq(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    T a = number<T>(args.unnamed[0]);
    T b = number<T>(args.unnamed[1]);
    return true_or_false(a >= b);
}

Object Interpreter::eval_geq(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{}, {}}, {});
    switch (args.unnamed.front().type) {
    case ObjectType::INT:
        return num_geq<int64_t>(form, args, env);

    case ObjectType::FLOAT:
        return num_geq<double>(form, args, env);

    default:
        throw_eval_error(form, ">= must have a numeric argument");
        return Object::make_null();
    }
}

/*!
 * implementation of absolute value.
 */
Object Interpreter::eval_abs(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (!args.named.empty() || args.unnamed.empty()) {
        throw_eval_error(form, "abs must receive at least one unnamed argument!");
    }

    if (!args.unnamed[0].is_number()) {
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

/*!
 * implementation of max.
 */
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
        if (!arg.is_number()) {
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

/*!
 * implementation of min.
 */
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
        if (!arg.is_number()) {
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

/*!
 * implementation of exponent.
 */
Object Interpreter::eval_expt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}, {}}, {});

    if (!args.unnamed[0].is_number() || !args.unnamed[1].is_number()) {
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

/*!
 * implementation of square root.
 */
Object Interpreter::eval_sqrt(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент

    if (!args.unnamed[0].is_number()) {
        throw_eval_error(form, "sqrt requires a number argument");
    }

    double val = number_to_float(args.unnamed[0]);
    if (val < 0) {
        throw_eval_error(form, "sqrt: negative argument");
    }

    return Object::make_float(std::sqrt(val));
}

/*!
 * implementation of ariphmetic shift.
 */
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

/*!
 * implementation of flore.
 */
Object Interpreter::eval_floor(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::floor(args.unnamed[0].as_float())));
}

/*!
 * implementation of ceil.
 */
Object Interpreter::eval_ceiling(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::ceil(args.unnamed[0].as_float())));
}

/*!
 * implementation of round.
 */
Object Interpreter::eval_round(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
    return Object::make_integer(static_cast<int64_t>(std::round(args.unnamed[0].as_float())));
}

/*!
 * implementation of integer modulo.
 */
Object Interpreter::eval_mod(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT}, {ObjectType::INT}}, {});

    int64_t a = args.unnamed[0].as_integer();
    int64_t b = args.unnamed[1].as_integer();
    if (b == 0)
        throw_eval_error(form, "mod: division by zero");

    return Object::make_integer(a % b);
}

/*!
 * implementation of sinus.
 */
Object Interpreter::eval_sin(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
    return Object::make_float(std::sin(args.unnamed[0].as_float()));
}

/*!
 * implementation of cosine.
 */
Object Interpreter::eval_cos(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
    return Object::make_float(std::cos(args.unnamed[0].as_float()));
}

/*!
 * implementation of atan.
 */
Object Interpreter::eval_atan(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    size_t num_args = args.unnamed.size();

    if (num_args == 1) {
        vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});
        return Object::make_float(std::atan(args.unnamed[0].as_float()));
    } else if (num_args == 2) {
        vararg_check(form, args,
                     {{ObjectType::INT, ObjectType::FLOAT}, {ObjectType::INT, ObjectType::FLOAT}},
                     {});
        return Object::make_float(
            std::atan2(args.unnamed[0].as_float(), args.unnamed[1].as_float()));
    } else {
        throw_eval_error(form, fmt::format("atan: expected 1 or 2 arguments, got {}", num_args));
        return get_null();
    }
}

/*!
 * implementation of tangens.
 */
Object Interpreter::eval_tan(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Тангенс всегда принимает ровно один аргумент (угол в радианах)
    vararg_check(form, args, {{ObjectType::INT, ObjectType::FLOAT}}, {});

    return Object::make_float(std::tan(args.unnamed[0].as_float()));
}

// ============================================================
// Bit operations
// ============================================================

/*!
 * implementation of logical and (logand n1 n2 ..).
 */
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

/*!
 * implementation of logical or (logior n1 n2 ..).
 */
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

/*!
 * implementation of logical xor (logxor n1 n2 ..).
 */
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

/*!
 * implementation of logical not (lognot n1 n2 ..).
 */
Object Interpreter::eval_lognot(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Ожидаем ровно один аргумент
    auto val = number_to_integer(args.unnamed.at(0));
    return Object::make_integer(~val);
}

/*!
 * implementation of logical left shift (lshift n1 bits).
 */
Object Interpreter::eval_lshift(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    vararg_check(form, args, {{ObjectType::INT}, {ObjectType::INT}}, {});

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

/*!
 * implementation of logical right shift (rshift n1 bits).
 */
Object Interpreter::eval_rshift(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT}, {ObjectType::INT}}, {});

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
// Поиск в списках
// ============================================================

/*!
 * Search field by name (getf 'foo '(:a 1 :b 2 :foo 3))
 */
Object Interpreter::eval_getf(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Используем ANY для первого аргумента, так как null (пустой список) — это тоже list
    vararg_check(form, args, {{ObjectType::PAIR, ObjectType::EMPTY_LIST}, {}}, {});

    Object current = args.unnamed[0];
    Object key = args.unnamed[1];

    while (!current.is_null() && current.is_list()) {
        auto pair_ptr = current.as_pair();
        if (pair_ptr->car == key) {
            Object rest = pair_ptr->cdr;
            if (rest.is_list() && !rest.is_null()) {
                return rest.as_pair()->car;
            }
            return get_null();
        }

        // Прыгаем на два элемента вперед: (cddr current)
        Object next = pair_ptr->cdr;
        if (next.is_list() && !next.is_null()) {
            current = next.as_pair()->cdr;
        } else {
            break;
        }
    }
    return get_null();
}

/*!
 * Search field by name (getf 'foo '((:a . 1) (:b . 2) (:foo . 3)))
 */
Object Interpreter::eval_assoc(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // key может быть чем угодно, а список может быть пустым
    vararg_check(form, args, {{}, {ObjectType::PAIR, ObjectType::EMPTY_LIST}}, {});

    Object key = args.unnamed[0];
    Object current_list = args.unnamed[1];

    while (!current_list.is_null() && current_list.is_list()) {
        Object item = current_list.as_pair()->car;

        // В alist каждый элемент — это пара (key . value)
        if (item.is_pair()) {
            if (item.as_pair()->car == key) {
                return item; // Возвращаем всю пару
            }
        }

        current_list = current_list.as_pair()->cdr;
    }
    return get_null();
}

// ============================================================
// Предикаты типов с проверками
// ============================================================

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

    return Object::make_symbol(&symbol_table(), args.unnamed[0].type_name_obj());
}

Object Interpreter::eval_type_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}, {}}, {});

    auto type_sym = args.unnamed[0]; // Символ типа, например 'int
    auto value = args.unnamed[1];    // Объект для проверки

    // Единый метод проверки
    return value.is_class_name(type_sym.to_std_string()) ? get_true() : get_false();
}

Object Interpreter::eval_null_p(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_null());
}

Object Interpreter::eval_keyword_p(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_keyword());
}

// ============================================================
// Apply
// ============================================================

Object Interpreter::eval_apply(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Базовая проверка (нам нужно минимум 2 аргумента: функция и список)
    vararg_check(form, args,
                 {{ObjectType::SYMBOL, ObjectType::FUNCTION, ObjectType::PRIMITIVE,
                   ObjectType::SPECIAL_FORM},
                  {}},
                 {});

    Object callable_obj = args.unnamed[0];
    Object args_list = args.unnamed[1];

    // 2. Если первым аргументом пришел символ (например, '+), резолвим его
    if (callable_obj.is_symbol()) {
        callable_obj = eval_with_rewind(callable_obj, env);
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
        auto bf = callable_obj.as_heap_obj<BuiltinFunctionObject>();
        // ВАЖНО: Мы не вызываем eval_args, так как apply подразумевает,
        // что данные в списке уже готовы к употреблению.
        return (this->*(bf->method))(form, applied_args, env);
    }

    // СЛУЧАЙ Б: Это обычная Лямбда
    if (callable_obj.is_function()) {
        const auto &lam = callable_obj.as_function();
        auto        lam_env = EnvironmentObject::make_new("apply").as_env_ptr();
        lam_env->ctx = form;
        lam_env->is_function = true;
        lam_env->owner_function = callable_obj;
        lam_env->parent_env = lam->parent_env;
        ASSERT(lam_env->owner_function.is_function());

        set_args_in_env(form, applied_args, lam->args, lam_env);
        return eval_list_return_last(lam->body, lam->body, lam_env);
    }

    // СЛУЧАЙ В: Попытка применить спецформу (обычно запрещено, но на твой вкус)
    if (callable_obj.is_special_form()) {
        throw_eval_error(form, "apply: cannot apply a special form (like 'if' or 'define')");
    }

    throw_eval_error(form, "apply: first argument is not a procedure");
    return m_obj_null;
}
// ============================================================
// Функции сравнения
// ============================================================

/*!
 * Fancy equality check (using Object::operator==)
 */
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
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::INT}}, {}); // Строка и индекс

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
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::INT}, {ObjectType::INT}},
                 {}); // Строка, начало, конец

    const std::string &str = args.unnamed[0].as_string()->data;
    int64_t            arg_start = args.unnamed[1].as_integer();
    int64_t            arg_end = args.unnamed[2].as_integer();
    int64_t            start = arg_start;
    int64_t            end = arg_end;
    if (start < 0)
        start = str.length() + start;
    if (end < 0)
        end = str.length() + end;

    if (start < 0 || end < 0 || start > end || end > static_cast<int64_t>(str.length())) {
        throw_eval_error(
            form, fmt::format("substring: invalid start {} or end {} index for string \"{}\"",
                              arg_start, arg_end, str));
    }

    return Object::make_string(str.substr(arg_start, arg_end - arg_start));
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
    return m_sym_false;
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
    return m_sym_false;
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
    return m_sym_false;
}

Object Interpreter::eval_string_split(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::STRING}}, {});
    auto &str = args.unnamed.at(0).as_string()->data;
    auto &delim = args.unnamed.at(1).as_string()->data;
    auto  list = str_util::split_string(str, delim);
    return pretty_print::build_list(list);
}

Object Interpreter::eval_string_join(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::PAIR}, {ObjectType::STRING}}, {});

    auto  list = args.unnamed.at(0);
    auto &delim = args.unnamed.at(1).as_string()->data;

    std::string result;
    bool        first = true;

    // Используем твой хелпер для итерации по списку
    for_each_in_list(list, [&](const Object &obj) {
        if (!first) {
            result += delim;
        }

        // Преобразуем элемент в строку в зависимости от его типа
        if (obj.type == ObjectType::SYMBOL) {
            result += obj.symbol_obj.value.name_ptr;
        } else if (obj.type == ObjectType::STRING) {
            result += obj.as_string()->data;
        } else if (obj.type == ObjectType::INT) {
            result += std::to_string(obj.integer_obj.value);
        } else {
            // Если попало что-то странное, используем стандартный принт
            result += obj.print();
        }

        first = false;
    });

    return Object::make_string(result);
}

Object Interpreter::eval_string_to_symbol(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING, ObjectType::SYMBOL}}, {}); // Одна строка
    if (args.unnamed[0].is_symbol())
        return args.unnamed[0];
    return intern(args.unnamed[0].as_string()->data);
}

Object Interpreter::eval_symbol_to_string(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL, ObjectType::STRING}}, {}); // Один символ
    if (args.unnamed[0].is_string())
        return args.unnamed[0];
    return Object::make_string(
        args.unnamed[0].as_symbol().name_ptr ? args.unnamed[0].as_symbol().name_ptr : "");
}

Object Interpreter::eval_string_to_upper(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::to_upper(args.unnamed[0].as_string()->data));
}

Object Interpreter::eval_string_to_lower(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::to_lower(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_string_titlize(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::titlize(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_string_rtrim(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::rtrim(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_string_ltrim(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::ltrim(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_string_trim(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::trim(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_string_trim_indents(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем аргументы: 1. Список (Pair) 2. Разделитель (String)
    vararg_check(form, args, {{ObjectType::STRING}}, {});
    return Object::make_string(str_util::trim_newline_indents(args.unnamed[0].as_string()->data));
}
Object Interpreter::eval_crc32(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {});

    return Object::make_integer(args.unnamed[0].as_crc32());
}

// ============================================================
// Векторные функции с проверками
// ============================================================

Object Interpreter::eval_make_array(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {}, {}); // Любое количество элементов
    return Object::make_array(args.unnamed);
}

Object Interpreter::eval_array_ref(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::INT}}, {}); // Вектор и индекс

    auto    elements = args.unnamed[0].as_array();
    int64_t index = args.unnamed[1].as_integer();

    if (index < 0 || index >= static_cast<int64_t>(elements->size())) {
        throw_eval_error(form, "array-ref: index out of range");
    }

    return elements->get(index);
}

Object Interpreter::eval_array_set(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::INT}, {}},
                 {}); // Вектор, индекс, значение

    auto array = args.unnamed[0].as_array();

    int64_t index = args.unnamed[1].as_integer();
    if (index < 0 || index >= array->size()) {
        throw_eval_error(form, "array-set!: index out of range");
    }

    array->set(index, args.unnamed[2]);
    return args.unnamed[2];
}

Object Interpreter::eval_array_length(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::ARRAY}}, {}); // Один вектор
    return Object::make_integer(args.unnamed[0].as_array()->size());
}

Object Interpreter::eval_array_to_list(const Object &form, Arguments &args,
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

/*!
 * Утилитарная фунция. Генерирует ключ для таблицы
 */
const char *get_hash_key(Object item_pair) {
    if (item_pair.is_symbol()) {
        return item_pair.as_symbol().name_ptr;
    } else if (item_pair.is_string()) {
        return item_pair.as_string()->data.c_str();
    } else {
        return nullptr;
    }
}

/*!
 * Пустая таблица: (make-hash-table)
 *
 * Только данные (размер будет 8): (make-hash-table '((:HL . 1) (:BC . 2)))
 *
 * Данные и размер: (make-hash-table '((:A . 0)) 100)
 */
Object Interpreter::eval_make_hash_table(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {},
                 {{"size", {false, {ObjectType::INT}}}, {"type", {false, {ObjectType::SYMBOL}}}});

    // Устанавливаем значения по умолчанию
    Object initial_data = Object::make_null();
    if (args.unnamed.size() > 0) {
        if (!args.unnamed[0].is_pair() && !args.unnamed[0].is_null()) {
            throw_type_mismatch(form, args, 0, {ObjectType::PAIR}, args.unnamed[0].type);
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

/*!
 * Set a value in a hash table. Overwrites a previous value or inserts a new one.
 * Returns empty list always.
 */
Object Interpreter::eval_hash_table_set(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::HASH_TABLE}, {ObjectType::SYMBOL, ObjectType::STRING}, {}},
                 {}); // Таблица, ключ, значение

    auto ht = args.unnamed[0].as_hash_table();
    auto key = args.unnamed.at(1).to_std_string();
    ht->data[key] = args.unnamed.at(2);

    return Object::make_null();
}

/*!
 * Try to look up a value by key in a hash table. The result is a pair of (success . value).
 */
Object Interpreter::eval_hash_table_ref(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Разрешаем любое кол-во, проверку сделаем сами для гибкости
    if (args.unnamed.size() > 2) {
        vararg_check(form, args,
                     {{ObjectType::HASH_TABLE}, {ObjectType::STRING, ObjectType::SYMBOL}, {}}, {});
    } else {
        vararg_check(form, args,
                     {{ObjectType::HASH_TABLE}, {ObjectType::STRING, ObjectType::SYMBOL}}, {});
    }
    auto ht = args.unnamed[0].as_hash_table();
    auto key = args.unnamed.at(1).to_std_string();
    try {
        auto it = ht->data.find(key);
        if (it != ht->data.end()) {
            return it->second;
        }
    } catch (std::exception &e) {
        throw_eval_error(form, e.what());
    }

    catch (...) {
        // Другие ошибки (не C++ исключения) тоже не ловятся!
        throw_eval_error(form, "Unknown error in hash table operation");
    }
    // Ключ не найден:
    // Если передан 3-й аргумент — возвращаем его
    if (args.unnamed.size() == 3) {
        return args.unnamed[2];
    }

    // Если 3-го аргумента нет — кидаем ошибку, как и раньше
    throw_eval_error(form, "hash-table-ref: key not found (use default value as last argument): " +
                               std::string(key));
    return get_null();
}

/*!
 * Try to look up a value by key in a hash table.The result is a pair
 * of(success.value).
 */
Object Interpreter::eval_hash_table_try_ref(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::HASH_TABLE}, {}}, {});

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

/*!
 * Check if hashtable has the key, returns true or faslse
 */
Object Interpreter::eval_hash_table_containsp(const Object &form, Arguments &args,
                                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::HASH_TABLE}, {}}, {});

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

/*!
 * Get the size of hastable
 */
Object Interpreter::eval_hash_table_length(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент

    auto ht = args.unnamed[0].as_hash_table();

    return Object::make_integer(ht->data.size());
}

/*!
 * Convert hash table to list
 */
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

/*!
 * Check if the object is hashbale
 */
Object Interpreter::eval_hash_table_p(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Один аргумент
    return true_or_false(args.unnamed[0].is_hash_table());
}

// ============================================================
// Фунции работы с таблицами
//
// Некоторые типы объкктом могут предоставлять универсальный
// способ доступа и это не только hash table
// ============================================================

/*!
 * Try to look up a value by key in a table kind object. The result
 * is a pair of (success . value).
 */
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
    Object result = target.as_heap_obj()->get_at(&symbol_table(), key);

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

/*!
 * Try to set a value by key in a table kind object. The result is null
 */
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
        target.as_heap_obj()->set_at(&symbol_table(), key, value);
    } else {
        throw_eval_error(form, "set-at!: target must be a heap object (buffer, table, etc)");
    }

    return get_null();
}

// ============================================================
// Системные функции с проверками
// ============================================================

/*!
 * Читает весь файл как текст.
 */
Object Interpreter::eval_read_str(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    return Object::make_string(file_util::read_text(filename));
}

/*!
 * Превращает строку в список команд: (top-level .. ). Удобно для eval.
 */
Object Interpreter::eval_parse_str(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка
    return m_reader.read_from_string(args.unnamed[0].as_string()->data, true, "read string");
}

/*!
 * Читает весь файл как данные, обернутые в top-level.
 */
Object Interpreter::eval_read_file(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}}, {}); // Одна строка (имя файла)

    std::string filename = args.unnamed[0].as_string()->data;
    std::string content = file_util::read_text(filename);
    return m_reader.read_from_string(content, true, filename);
}

/*!
 * Читает и исполняет файл. (Обычно исполняет объекты по одному, top-level не нужен).
 */
Object Interpreter::eval_load(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{}}, {}); // Одна строка (имя файла)
    Object last_result = get_null();
    try {
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
    } catch (EvalException &e) {
        e.add_context(form, "load failed", env);
        throw;
    } catch (std::runtime_error &e) {
        throw_eval_error(form, e.what());
    } catch (std::exception &e) {
        throw_eval_error(form, e.what());
    }
    return last_result;
}

/*!
 * Проверяет если файл существует
 */
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

Object Interpreter::eval_get_sys_env(const Object &form, Arguments &args,
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
    vararg_check(form, args, {{ObjectType::INT}, {}}, {});

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
    return intern(name.c_str());
}

Object Interpreter::eval_eval(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env) {
    // Минимум 1 аргумент должен быть
    if (args.unnamed.empty()) {
        throw_eval_error(form, "eval: at least one argument required");
    }

    std::shared_ptr<EnvironmentObject> in_env;

    if (args.has_named("env")) {
        if (!args.named["env"].is_env())
            throw_eval_error(form, "eval: the &key eval except only environmet");
        in_env = args.named["env"].as_env_ptr();
    } else {
        in_env = env;
    }
    Object first = args.unnamed.at(0);

    // СЛУЧАЙ 1: Классический (eval '(+ 1 2))
    if (args.unnamed.size() == 1) {
        // Мы просто вычисляем то, что нам дали.
        // Важно: если 'first' это уже LAMBDA, наш исправленный eval
        // (с case ObjectType::LAMBDA: return obj) просто вернет её.
        return eval_with_rewind(first, in_env);
    }

    // СЛУЧАЙ 2: Расширенный (eval lambda arg1 arg2 ...)
    // Здесь мы ведем себя как apply/call.

    // Собираем все аргументы кроме первого в вектор
    std::vector<Object> call_args;
    for (size_t i = 1; i < args.unnamed.size(); ++i) {
        call_args.push_back(args.unnamed.at(i));
    }

    if (first.is_function()) {
        // Используем твой call_lambda_internal.
        // Он создаст окружение, привяжет аргументы и выполнит тело.
        return call_lambda_internal(form, first, call_args, in_env);
    } else if (first.is_primitive()) {
        // Если это встроенная функция (например, +), нам нужно
        // подготовить структуру Arguments и вызвать её метод.
        auto      builtin = first.as_heap_obj<BuiltinFunctionObject>();
        Arguments b_args;
        b_args.unnamed = call_args;
        return ((*this).*(builtin->method))(form, b_args, in_env);
    }

    throw_eval_error(form, "eval: first argument must be a code form or callable when multiple "
                           "arguments are provided");
    return Object::make_null();
}

/*!
 * Special method of computing arguments for macro
 *
 * (defmacro sum (&key (x *value-a*) (y *value-b*))
 *    (eval-args x y)
 * )
 */
Object Interpreter::eval_eval_args_special(const Object &form, const Object &rest,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    // Минимум 1 аргумент должен быть
    if (rest.is_null()) {
        throw_eval_error(form, "eval-args: at least one argument required");
    }

    auto item = rest;
    while (!item.is_null()) {
        if (!item.is_pair())
            throw_eval_error(form, "eval-args: invalid arguments list");

        auto arg_sym = item.as_pair()->car;

        // Аргумент должен быть символом (именем переменной)
        if (!arg_sym.is_symbol()) {
            throw_eval_error(form, "eval-args: argument must be a symbol");
        }

        // Получаем текущее значение символа из окружения
        Object *value = env->find(arg_sym.as_symbol());
        if (value) {
            // Вычисляем это значение
            Object result = eval_with_rewind(*value, env);
            // Сохраняем результат обратно в ту же переменную
            env->set(arg_sym.as_symbol(), result);
        } else {
            throw_eval_error(form, "eval-args: unknown argument " + arg_sym.to_std_string());
        }

        item = item.as_pair()->cdr;
    }

    return Object::make_null();
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
    vararg_check(form, args, {{}}, {{"base", {false, {ObjectType::INT}}}});

    // Проверяем что это ЧИСЛО (любое)
    if (!args.unnamed[0].is_number()) {
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
                 {{"base", {false, {ObjectType::INT}}}}); // Строка и опционально основание

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
    vararg_check(form, args, {{ObjectType::INT}}, {}); // Одно целое

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
// Итераторы
// ============================================================

/*
 * Iterate over each character
 */
Object Interpreter::eval_string_for_each(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::STRING}, {ObjectType::FUNCTION}}, {});

    const std::string &str = args.unnamed[0].as_string()->data;
    Object             lambda = args.unnamed[1];
    const auto        &lam_data = lambda.as_function();
    int                count = 0;
    int                lambda_args_count = lam_data->args.unnamed.size();
    for (unsigned char c : str) {

        switch (lambda_args_count) {
        case 1:
            // Передаем код символа как Integer
            call_lambda_internal(form, lambda, {Object::make_integer(static_cast<int>(c))}, env);
            break;
        case 2:
            // Передаем код символа как Integer
            call_lambda_internal(
                form, lambda,
                {Object::make_integer(count++), Object::make_integer(static_cast<int>(c))}, env);
            break;

        default:
            throw_eval_error(form, "eval-string-for-each: expected lambda with 1 or 2 arguments");
        }
    }
    return get_null();
}

/*
 * Iterate over each aray item
 */
Object Interpreter::eval_array_for_each(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверяем типы: первый аргумент — массив (вектор), второй — лямбда
    vararg_check(form, args, {{ObjectType::ARRAY}, {ObjectType::FUNCTION}}, {});

    const auto &vec = args.unnamed[0].as_array()->data;
    Object      lambda = args.unnamed[1];
    const auto &lam_data = lambda.as_function();
    int         count = 0;
    int         lambda_args_count = lam_data->args.unnamed.size();
    for (const auto &item : vec) {
        switch (lambda_args_count) {
        case 1:
            // Вызываем лямбду для каждого элемента вектора
            call_lambda_internal(form, lambda, {item}, env);
            break;
        case 2:
            // Вызываем лямбду для каждого элемента вектора
            call_lambda_internal(form, lambda, {Object::make_integer(count++), item}, env);
            break;

        default:
            throw_eval_error(form, "eval-array-for-each: expected lambda with 1 or 2 arguments");
        }
    }
    return get_null();
}

/*!
 * @brief Специальная форма для обхода хеш-таблицы с лямбдой.
 * @param form Специальная форма "for-each".
 * @param args Аргументы к форме:
 *     - 1-й аргумент: хеш-таблица
 *     - 2-й аргумент: лямбда, которая будет вызвана для каждого элемента хеш-таблицы
 * @return undefined
 */
Object Interpreter::eval_hash_table_for_each(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::HASH_TABLE}, {ObjectType::FUNCTION}}, {});

    auto       &table = args.unnamed[0].as_hash_table()->data;
    Object      lambda = args.unnamed[1];
    const auto &lam_data = lambda.as_function();
    int         lambda_args_count = lam_data->args.unnamed.size();
    for (auto const &[key, val] : table) {
        if (lambda_args_count == 1) {
            // Если лямбда ждет 1 аргумент, упаковываем в пару (entry)
            call_lambda_internal(form, lambda, {Object::make_pair(Object::make_string(key), val)},
                                 env);
        } else {
            // Если ждет 2 (или больше/rest), передаем как два аргумента
            call_lambda_internal(form, lambda, {Object::make_string(key), val}, env);
        }
    }
    return get_null();
}

/*
 * Iterate over each list item
 */
Object Interpreter::eval_list_for_each(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR}, {ObjectType::FUNCTION}}, {});

    Object      current = args.unnamed[0];
    Object      lambda = args.unnamed[1];
    const auto &lam_data = lambda.as_function();
    int         idx = 0;
    int         lambda_args_count = lam_data->args.unnamed.size();
    while (current.is_pair()) {
        switch (lambda_args_count) {
        case 1:
            // Вызываем твой надежный call_lambda_internal
            call_lambda_internal(form, lambda, {current.as_pair()->car}, env);
            break;
        case 2:
            // Вызываем твой надежный call_lambda_internal с индексом и значением
            call_lambda_internal(form, lambda,
                                 {Object::make_integer(idx++), current.as_pair()->car}, env);
            break;

        default:
            throw_eval_error(form, "eval-list-for-each: expected lambda with 1 or 2 arguments");
        }
        // print_form_info(form);
        current = current.as_pair()->cdr;
    }

    return get_null();
}

/*
 * Iterate over each list pair
 */
Object Interpreter::eval_list_for_each_pair(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{ObjectType::PAIR}, {ObjectType::FUNCTION}}, {});

    Object current = args.unnamed[0];
    Object lambda = args.unnamed[1];

    while (current.is_pair()) {
        Object key = current.as_pair()->car;
        current = current.as_pair()->cdr;

        if (!current.is_pair()) {
            throw_eval_error(
                form, "eval-list-for-pair: expected even number of elements for key-value mapping");
        }

        Object value = current.as_pair()->car;

        // Вызываем лямбду с двумя аргументами: (key, value)
        call_lambda_internal(form, lambda, {key, value}, env);

        current = current.as_pair()->cdr;
    }

    return get_null();
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
    } else if (object.is_function()) {
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
    if (args.unnamed[0].is_function()) {
        auto lambda = args.unnamed[0].as_function();
        result = get_db().get_short_info_for(lambda->body);
    } else {
        result = get_db().get_short_info_for(args.unnamed[0]);
    }

    if (!result) { // Если форма вычислена динамически и её нет в БД
        return get_null();
    }

    return pretty_print::build_list({
        intern(":file"), Object::make_string(result->filename), intern(":line"),
        Object::make_integer(result->line_number), intern(":column"),
        Object::make_integer(result->col_number), intern(":text"),
        Object::make_string(result->line_text) // Полезно для вывода "стрелочки" ^
    });
}

Object Interpreter::eval_get_context(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // Проверка аргумента (ожидаем INTEGER)
    vararg_check(form, args, {{ObjectType::INT}}, {});

    int64_t ctx_index = args.unnamed[0].as_integer();
    if (ctx_index < 0) {
        throw_eval_error(form, "context-ref: index cannot be negative");
    }

    // Начинаем с самого верхнего кадра
    auto current = env;

    auto index = m_dynamic_stack.size() - 1 - ctx_index;
    if (index >= 0)
        // Возвращаем форму, сохраненную в этом кадре
        return m_dynamic_stack[index]->ctx;
    // Если индекс за пределами глубины стека, возвращаем null (или можно кинуть ошибку)
    throw_eval_error(form, "Requested stack depth is to big");
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
        mac_env->ctx = form;
        mac_env->is_function = false; // Macro is not a function
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
        return intern(it->second.c_str());
    }

    // Если ничего не нашли, возвращаем пустой список (nil)
    return Object::make_null();
}

// ============================================================
// Register Alias
// ============================================================

/*!
 * Make register alias, a container used inside rlet form
 */
Object Interpreter::eval_reg_alias(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::SYMBOL}, {ObjectType::NATIVE_OBJECT, ObjectType::SYMBOL}},
                 {{"reg", {false, {ObjectType::SYMBOL}}}});

    auto reg = std::make_shared<Register>();
    reg->name = args.unnamed[0];

    // 1. Определяем имя типа
    auto second = args.unnamed[1];
    if (second.is_symbol())
        reg->type_name = second;
    else if (second.is_native_obj<Type>())
        reg->type_name =
            Object::make_symbol(&symbol_table(), second.as_heap_obj<Type>()->get_name()); // Используй get_name()

    // 2. Привязываем регистр
    if (args.has_named("reg"))
        reg->reg = args.named["reg"];

    // 3. ИНИЦИАЛИЗАЦИЯ ПАРАМЕТРОВ
    auto type = TypeSystem::instance().lookup_type(reg->type_name.to_std_string());
    if (type) {
        reg->offset = 0;     // Всегда 0 для корня!
        reg->bit_offset = 0; // Всегда 0 для корня!
        reg->bit_size = type->get_size_in_memory() * 8;
    } else {
        // Фоллбек, если тип не найден (например, для простых символов)
        reg->offset = 0;
        reg->bit_size = 0;
    }

    return Object::make_heap_obj(reg);
}

// ============================================================
// Type System
// ============================================================

/*!
 * Initialize the type system
 */
Object Interpreter::eval_init_types(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}}, {});
    TypeSystem::instance().clear();
    auto platform = args.unnamed[0].as_symbol();
    if (platform == "default") {
        init_types(SootPlatform::Default);
        return get_true();
    } else if (platform == "z80") {
        init_types(SootPlatform::Default);
        return get_true();
    } else {
        throw_eval_error(form,
                         fmt::format("Expected 'default or 'z80, got {}", args.unnamed[0].print()));
        return get_null();
    }
}

/*!
 * Helper Initialize the type system
 */
bool Interpreter::init_types(SootPlatform platform) {
    auto &ts = TypeSystem::instance();
    auto  env = m_global_environment.as_env();

    // 1. УДАЛЯЕМ старые типы из окружения
    std::vector<const char *> to_remove;
    const auto               &entries = env->vars.get_all_entries();
    for (const auto &entry : entries) {
        if (entry.value.is_native_obj()) {
            to_remove.push_back(entry.key);
        }
    }
    for (const auto &key : to_remove) {
        env->vars.remove(key);
    }

    // 2. ОЧИЩАЕМ TypeSystem
    ts.clear();

    // 3. СОЗДАЁМ новые типы
    ts.add_builtin_types(platform);
 
    // 4. ЭКСПОРТИРУЕМ новые типы
    const auto &all_types = ts.get_types(); // теперь константная ссылка!
    for (const auto &pair : all_types) {    // pair, а не [name, type_ptr]
        const auto &name = pair.first;
        auto       *type_ptr = pair.second.get();

        auto shared_type = std::shared_ptr<Type>(type_ptr, [](Type *) {});
        auto type_obj = Object::make_heap_obj(shared_type);
        env->vars.set(Object::intern(&symbol_table(), name.c_str()), type_obj);
    }

    // 5. Обновляем ссылку на TypeSystem
    define_var_in_env(get_global_environment(), ts.to_alias(), "*type-system*");

    return true;
}

/*!
 * Get all types to the script language
 */
Object Interpreter::eval_types_to_lisp(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)form;
    (void)args;
    (void)env;
    return TypeSystem::instance().get_all_type_names_as_objects();
}

// ------------------------------------------------------------
// Type System define type, enum
// ------------------------------------------------------------

/*!
 * Parse define type special form
 */
Object Interpreter::eval_deftype_special(const Object &form, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto env_ptr = get_global_environment().as_env();
    try {
        auto result = parse_deftype(rest, &TypeSystem::instance(), &env_ptr->vars);
        auto type_shared = std::shared_ptr<Type>(result.type_info, [](Type *) {
            /* Ничего не делаем, TypeSystem сама удалит его через unique_ptr */
        });
        auto name = result.type_info->get_name();

        m_global_environment.as_env()->vars.set(Object::intern(&symbol_table(), name.c_str()),
                                                Object::make_heap_obj(type_shared));
        return Object::make_heap_obj(type_shared);
    } catch (std::runtime_error &ex) {
        throw_eval_error(form, ex.what());
    }
    return get_null();
}

/*!
 * Parse defenum special form
 */
Object Interpreter::eval_defenum_special(const Object &, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto enum_ptr = parse_defenum(rest, &TypeSystem::instance(), nullptr);
    auto enum_shared = std::shared_ptr<Type>(enum_ptr, [](Type *) {
        /* Ничего не делаем, TypeSystem сама удалит его через unique_ptr */
    });

    auto name = enum_ptr->get_name();

    m_global_environment.as_env()->vars.set(Object::intern(&symbol_table(), name.c_str()),
                                            Object::make_heap_obj(enum_shared));
    return Object::make_heap_obj(enum_shared);
}

// ------------------------------------------------------------
// Type System define typespecification
// ------------------------------------------------------------

/*!
 * Make typespecification with the primitive
 */
Object Interpreter::eval_typespec(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::PAIR, ObjectType::SYMBOL, ObjectType::NATIVE_OBJECT}},
                 {});
    Object spec_input = args.unnamed[0];

    // Рекурсивная функция для очистки входных данных от алиасов
    std::function<Object(Object)> resolve_aliases = [&](Object obj) -> Object {
        if (obj.is_symbol())
            return obj;

        if (obj.is_pair()) {
            // Обрабатываем список (например, (function ,x))
            return Object::make_pair(resolve_aliases(obj.as_pair()->car),
                                     resolve_aliases(obj.as_pair()->cdr));
        }

        // КЛЮЧЕВОЙ МОМЕНТ:
        // Если встретили HEAP_OBJ, проверяем, не reg-alias ли это
        if (obj.type == ObjectType::NATIVE_OBJECT) {
            auto reg = obj.as_heap_obj<Register>();
            // Предположим, у тебя есть метод проверки типа в рантайме
            if (reg) {
                // Возвращаем СИМВОЛ типа (например, 'int'), который поймет parse_typespec
                return reg->type_name;
            }

            auto type = obj.as_heap_obj<Type>();
            // Предположим, у тебя есть метод проверки типа в рантайме
            if (type) {
                // Возвращаем СИМВОЛ типа (например, 'int'), который поймет parse_typespec
                return Object::make_symbol(&symbol_table(), type->get_name());
            }

            throw_eval_error(
                form, fmt::format("typespec parsing go unexpected type {}", obj.class_name()));
        }

        return obj;
    };

    // Очищаем входную форму от алиасов
    Object resolved_input = resolve_aliases(spec_input);

    // Теперь parse_typespec увидит (function int) вместо (function #<reg-alias...>)
    try {
        auto ts_ptr = parse_typespec(&TypeSystem::instance(), resolved_input);
        auto ts_shared = std::make_shared<TypeSpec>(ts_ptr);

        return Object::make_heap_obj(ts_shared);
    } catch (std::runtime_error &e) {
        throw_eval_error(form, e.what());
    }
    return get_none();
}

/*!
 * Make typespecification with the special form
 */
Object Interpreter::eval_deftypespec_special(const Object &, const Object &rest,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    if (rest.is_null())
        return get_null();

    Object spec_input = rest.as_pair()->car;
    auto   ts_ptr = parse_typespec(&TypeSystem::instance(), spec_input);
    auto   ts_shared = std::make_shared<TypeSpec>(ts_ptr);

    return Object::make_heap_obj(ts_shared);
}

/*!
 * Construct type specification
 */
TypeSpec Interpreter::parse_typespec_helper(const Object &obj) {
    if (obj.is_symbol()) {
        // Простой тип: 'int16
        return TypeSystem::instance().make_typespec(obj.as_symbol().name_ptr);
    } else if (obj.is_pair()) {
        auto list = obj.to_vector();
        auto head = list.at(0);

        if (head.is_symbol()) {
            std::string head_name = head.as_symbol().name_ptr;

            if (head_name == "function") {
                // (function (arg-types...) return-type)
                if (list.size() < 3)
                    throw std::runtime_error("Invalid function typespec");

                std::vector<std::string> arg_types;
                for (auto &arg : list.at(1).to_vector()) {
                    arg_types.push_back(parse_typespec_helper(arg).print());
                }
                std::string ret_type = parse_typespec_helper(list.at(2)).print();

                return TypeSystem::instance().make_function_typespec(arg_types, ret_type);
            } else if (head_name == "pointer") {
                // (pointer uint8)
                return TypeSystem::instance().make_pointer_typespec(
                    parse_typespec_helper(list.at(1)));
            } else if (head_name == "inline-array") {
                return TypeSystem::instance().make_inline_array_typespec(
                    parse_typespec_helper(list.at(1)));
            }
        }
    }
    throw std::runtime_error("Could not parse typespec: " + obj.print());
}

// ============================================================
// Type system -- Type navigation
// ============================================================

/*!
 * Dereference with primitive
 */
Object Interpreter::eval_deref(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Проверка на наличие аргументов (минимум корень)
    if (args.unnamed.empty()) {
        return get_null();
    }

    // 2. Вычисляем корень (Root)
    // Благодаря твоей новой системе, тут может быть символ 'vec3 (тип)
    // или символ 'self (экземпляр/регистр). Eval вернет живой объект.
    Object current = eval(args.unnamed[0], env);

    if (current.is_none()) {
        throw_eval_error(form, fmt::format("Deref root '{}' evaluated to NONE. "
                                           "Check if variable or type is defined.",
                                           args.unnamed[0].print()));
    }

    // 3. Итерация по ключам навигации
    // Начинаем с индекса 1, так как индекс 0 — это корень
    for (size_t i = 1; i < args.unnamed.size(); ++i) {
        const Object &key_form = args.unnamed[i];
        Object        key;

        // В спецформе (-> obj field) ключи обычно символы.
        // Если это символ — берем как есть. Если список/выражение — вычисляем.
        if (key_form.is_symbol()) {
            key = key_form;
        } else {
            key = eval(key_form, env);
        }

        // 4. Выполняем шаг навигации
        try {
            // Метод step теперь полиморфен: он знает, как работать
            // и с TypeObject, и с HeapObject/Pointer.
            Object next = current.step(&symbol_table(), key);

            if (next.is_none()) {
                throw_eval_error(form, fmt::format("Access error: field or property '{}' "
                                                   "not found in object of type {}",
                                                   key.print(), current.class_name()));
            }

            current = next;

        } catch (const std::exception &e) {
            // Пробрасываем внутренние ошибки step (например, выход за границы или неверный тип
            // ключа)
            throw_eval_error(form, fmt::format("Deref step failed: {}", e.what()));
        }
    }

    // Возвращаем результат последнего шага
    return current;
}

/*!
 * @brief Специальная форма глубокой навигации (Макрос `->`).
 * Роль в системе:
 * Последовательно применяет цепочку ключей к объекту, "проваливаясь" внутрь
 * структур, массивов или метаданных. Реализует высокоуровневый синтаксис доступа.
 * Особенности реализации:
 * 1. Первый аргумент вычисляется (eval) — это корень (например, переменная с TypePointer).
 * 2. Последующие аргументы-символы трактуются как имена полей БЕЗ вычисления.
 * 3. Аргументы-списки или числа вычисляются (позволяет динамические индексы).
 * Синтаксис в Lisp: (-> root field1 index field2)
 * Пример:
 * (-> my-vec 'x)           ; корень my-vec, шаг к полю x
 * (-> my-buf 10 'int)      ; корень my-buf, вычислить индекс 10, шаг к типу int
 * (-> obj (get-idx) 'name) ; индекс вычисляется вызовом функции
 * @param rest Список аргументов навигации.
 * @return Конечный объект (TypePointer, значение или метаданные) после прохождения всего пути.
 */
Object Interpreter::eval_deref_special(const Object &form, const Object &rest,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    if (!rest.is_pair())
        return get_null();

    auto pair = rest.as_pair();

    // 1. EVAL первого элемента.
    // Теперь это ОБЯЗАН быть объект (Type, Pointer, Register и т.д.)
    Object current = eval(pair->car, env);

    if (current.is_none()) {
        throw_eval_error(form, fmt::format("Symbol '{}' not found in environment. "
                                           "Did you forget to define the type or variable?",
                                           pair->car.print()));
    }

    Object iterator = pair->cdr;

    // 2. Навигация по ключам
    while (!iterator.is_null()) {
        Object key_form = iterator.as_pair()->car;

        // Ключи в спецформе -> по умолчанию символы (имена полей)
        // Но если это не символ (например, число), мы его вычисляем
        Object key = key_form.is_symbol() ? key_form : eval(key_form, env);

        // Весь интеллект теперь здесь.
        // Если current - это Type, он вернет метаданные.
        // Если current - это экземпляр, он вернет данные.
        try {
            current = current.step(&symbol_table(), key);
        } catch (std::runtime_error &e) {
            throw_eval_error(form, e.what());
        }
        if (current.is_none()) {
            throw_eval_error(form,
                             fmt::format("Field or meta-property '{}' is not accessible in '{}'",
                                         key.print(), current.class_name()));
        }

        iterator = iterator.as_pair()->cdr;
    }

    return current;
}

// ============================================================
// Low-level pointer operations
// ============================================================

/*!
 * @brief addr-of: Возвращает адрес объекта (TypePointer или Field) в виде целого числа.
 * addr-of принимает один аргумент - целевой объект (TypePointer, Field или StaticBuffer).
 * addr-of возвращает целое число, которое является адресом объекта в памяти.
 * - Если целевой объект является TypePointer, addr-of возвращает абсолютный адрес, на который
 * ссылается указатель.
 * - Если целевой объект является Field, addr-of возвращает относительное смещение поля в
 * структуре.
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

    // 3. Для всех HeapObject (Field, MethodInfo, StaticBuffer, StructureType...)
    // Правило одно: берем адрес самого объекта (или его данных), которые лежат в C++.
    if (target.is_native_obj()) {
        // Получаем shared_ptr на обертку
        auto hr = target.as_heap_obj<HeapObject>();

        // .get() возвращает сырой указатель (HeapObject*),
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
        } else if (auto f = arg.as_heap_obj<Field>()) {
            total += f->offset();
        } else if (auto f = arg.as_heap_obj<MethodInfo>()) {
            total += f->id;
        } else {
            throw_eval_error(form, "addr+ : argument is not a number, pointer: " + arg.print());
        }
    }

    return Object::make_integer(total);
}

// ============================================================
// Memory access
// ============================================================

/*!
 * @brief Чтение значения из ячейки памяти (Dereference).
 */
Object Interpreter::eval_mem_get(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::POINTER}}, {});
    try {
        // Просто дергаем метод Pointer::get(), который мы обсуждали
        return args.unnamed[0].as_pointer()->get();
    } catch (std::runtime_error &ex) {
        throw_eval_error(form, ex.what());
    }
}
/*!
 * @brief Запись значения в ячейку памяти (Assignment).
 */
Object Interpreter::eval_mem_set(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::POINTER}, {}}, {});
    auto ptr = args.unnamed[0].as_pointer();
    try {
        ptr->set(args.unnamed[1]); // Пишем значение
    } catch (std::runtime_error &ex) {
        throw_eval_error(form, ex.what());
    }
    return get_none();
}

// ============================================================
// Methods and types
// ============================================================

/*!
 * Define method
 */
Object Interpreter::eval_define_method(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {
                     {ObjectType::SYMBOL, ObjectType::NATIVE_OBJECT}, // 0: Тип
                     {ObjectType::SYMBOL},                            // 1: ID или Имя метода
                     {ObjectType::FUNCTION}                           // 2: Лямбда
                 },
                 {{"declare-extern", {false, {ObjectType::SYMBOL}}}});

    auto &ts = TypeSystem::instance();
    auto  arg0_obj = args.unnamed[0];
    // 1. Разрешаем тип
    Type *type = nullptr;
    if (arg0_obj.is_symbol())
        type = ts.lookup_type(arg0_obj.to_std_string());
    else if (arg0_obj.is_native_obj<Type>())
        type = arg0_obj.as_heap_obj<Type>().get();
    else
        throw_type_mismatch(form, args, 0, {"type", "symbol"}, arg0_obj.class_name());

    if (!type)
        throw_eval_error(form, "Could not resolve type n arg0");

    // 2. Получаем реализацию и проверяем наличие (declare)
    Object implementation = args.unnamed[2];
    auto   lambda_ptr = implementation.as_heap_obj<FunctionObject>();

    if (!lambda_ptr->declarations.is_set || lambda_ptr->declarations.typespec.is_null()) {
        throw_eval_error(
            form, fmt::format("set-method {}::{} expect declared typespec in the lambda object {}",
                              args.unnamed[0].to_std_string(), args.unnamed[1].print(),
                              implementation.print()));
    }

    const TypeSpec &impl_spec = *lambda_ptr->declarations.typespec.as_heap_obj<TypeSpec>();
    auto            success = false;
    // 3. ПРОВЕРКА ЧЕРЕЗ DEFINE_METHOD (Тот самый шаг!)
    try {
        std::string method_name;
        if (args.unnamed[1].is_symbol()) {
            method_name = args.unnamed[1].to_std_string();
        } else {
            // Если пришло число (ID), вытягиваем имя из типа, чтобы передать в define_method
            MethodInfo info;
            if (!type->get_my_method(args.unnamed[1].as_integer(), &info)) {
                throw_eval_error(form, fmt::format("Method ID {} not found in type {}",
                                                   args.unnamed[1].as_integer(), type->get_name()));
            }
            method_name = info.name;
        }

        // Вызываем высокоуровневый метод системы типов.
        // Он сделает всю грязную работу по проверке сигнатур и бросит подробный exception.
        ts.define_method(type, method_name, impl_spec, std::nullopt);

        if (args.has_named("declare-extern") && is_true(args.named["declare-extern"])) {
            auto full_name =
                fmt::format("{}::{}", type->get_name(), args.unnamed[1].to_std_string());
            auto full_name_ptr = intern_ptr(full_name);
            if (m_symbol_types.lookup(full_name_ptr)) {
                throw_eval_error(form, fmt::format("The method is already {} defined", full_name));
            } else {
                std::shared_ptr<TypeSpec> shared_type_spec = std::make_shared<TypeSpec>(impl_spec);
                m_symbol_types.set(full_name_ptr, shared_type_spec);
                success = true;
            }
        } else {
            success = true;
        }
    } catch (const std::exception &e) {
        // Пробрасываем качественную ошибку из TypeSystem в REPL
        throw_eval_error(form, e.what());
    }

    return success ? get_true() : get_false();
}

/*!
 * Get method of (method-of type method-name) or (method-of type method-id)
 */
Object Interpreter::eval_method_of(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL, ObjectType::INT}}, {});
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
        return Object::make_heap_obj(method_ptr);
    } else {
        auto method_name = args.unnamed[1].as_symbol();
        auto m_info = ts.lookup_method(type->get_name(), method_name.name_ptr);
        auto method_ptr = std::make_shared<MethodInfo>(m_info); // Честная копия
        return Object::make_heap_obj(method_ptr);
    }
}

/*!
 * Get method id of (method-id-of type-name method-name)
 */
Object Interpreter::eval_method_id_of(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

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

/*!
 * Get offset of field (offset-of type-name field-name)
 */
Object Interpreter::eval_offset_of(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}}, {});

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
/*!
 * Get the size of type or field (size-of type)
 */
Object Interpreter::eval_size_of(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    auto &ts = TypeSystem::instance();

    if (args.unnamed.size() == 1) {
        // Обычный size-of типа: (size-of vector)
        vararg_check(form, args, {{ObjectType::SYMBOL}}, {});
        auto type_name = args.unnamed[0].as_symbol();
        return Object::make_integer(ts.lookup_type(type_name.name_ptr)->get_size_in_memory());
    } else if (args.unnamed.size() == 2) {
        // Умный size-of поля: (size-of vector x)
        vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}}, {});
        auto type_name = args.unnamed[0].as_symbol();
        auto field_name = args.unnamed[1].as_symbol();

        auto type = ts.lookup_type(type_name.name_ptr);
        auto info = ts.lookup_field_info(type->get_name(), field_name.name_ptr);

        // Берем тип самого поля и узнаем его размер
        auto field_type = ts.lookup_type(info.field.type().base_type());
        return Object::make_integer(field_type->get_size_in_memory());
    }

    throw_arity_mismatch(form, args, 1, 2, args.unnamed.size());
    return get_null();
}

// ============================================================
// Type casting
// ============================================================

Object Interpreter::eval_the(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Проверка аргументов (тип и само выражение)
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "the: expected (the <type> <expression>)");
    }

    auto type_form = args.unnamed[0];
    auto target_form = args.unnamed[1];

    // 2. Определяем Expected Type
    TypeSpec expected_spec;

    // Если мы передали объект типа (уже вычисленный ранее или найденный в env)
    if (type_form.is_native_obj<Type>()) {
        expected_spec = TypeSpec(type_form.as_heap_obj<Type>()->get_name());
    }
    // Если это символ/строка (ищем в системе типов напрямую)
    else if (type_form.is_symbol() || type_form.is_string()) {
        expected_spec = TypeSpec(type_form.to_std_string());
    } else {
        throw_eval_error(form, "the: first argument must be a type object, symbol or string");
    }

    // 3. ВАЖНО: Вычисляем целевое выражение
    // В специальной форме аргументы могут быть еще не вычислены
    Object target = eval(target_form, env);

    // 4. Получаем актуальный тип объекта
    TypeSpec actual_spec;
    if (target.is_pointer()) {
        actual_spec = TypeSpec(target.as_pointer()->type());
    } else {
        // Используем встроенный метод получения типа объекта в рантайме
        actual_spec = TypeSpec(target.class_name());
    }

    // 5. Проверка совместимости типов
    // actual_spec должен быть таким же или подтипом expected_spec
    if (!TypeSystem::instance().tc(expected_spec, actual_spec)) {
        throw_eval_error(form, fmt::format("Type mismatch: expected {}, but object is {}",
                                           expected_spec.print(), actual_spec.print()));
    }

    return target;
}

Object Interpreter::eval_the_as(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env) {
    // 1. Проверка количества аргументов.
    // Нам нужно как минимум 2: тип и целевое выражение.
    if (args.unnamed.size() < 2) {
        throw_eval_error(form, "the-as: expected (the-as <type> <expression>)");
    }

    auto type_arg = args.unnamed[0];
    auto target_arg = args.unnamed[1];

    // 2. Определяем имя типа для нового указателя
    std::string new_type_name;

    // Поддержка "Типов как объектов"
    if (type_arg.is_native_obj<Type>()) {
        new_type_name = type_arg.as_heap_obj<Type>()->get_name();
    }
    // Поддержка символов/строк (классика)
    else if (type_arg.is_symbol() || type_arg.is_string()) {
        new_type_name = type_arg.to_std_string();
    } else {
        throw_eval_error(form, "the-as: first argument must be a type object, symbol or string");
    }

    // 3. Вычисляем целевое выражение
    Object target = eval(target_arg, env);

    // 4. Логика "Reinterpret"

    // Случай А: На входе Integer (сырой адрес) -> превращаем в типизированный указатель
    if (target.is_integer()) {
        void *addr = reinterpret_cast<void *>(static_cast<uintptr_t>(target.as_integer()));
        return Object::make_pointer(addr, new_type_name);
    }

    // Случай Б: На входе уже Pointer -> меняем его тип, не меняя адрес
    if (target.is_pointer()) {
        void *raw_addr = target.as_pointer()->resolve_addr();
        return Object::make_pointer(raw_addr, new_type_name);
    }

    // Случай В: Специальная логика для Native Object (если они хранятся по ссылке)
    // В GOAL можно сделать (the-as int my-object), чтобы получить его адрес
    if (target.is_native_obj()) {
        // Получаем адрес самого нативного объекта
        void *raw_addr = target.as_heap_obj();
        // Если мы кастуем к базовым типам (int/uint), возвращаем адрес как число
        if (new_type_name == "int" || new_type_name == "uint") {
            return Object::make_integer(reinterpret_cast<uintptr_t>(raw_addr));
        }
        // Иначе возвращаем новый указатель на этот адрес
        return Object::make_pointer(raw_addr, new_type_name);
    }

    // Если мы дошли сюда, значит пытаемся сделать cast того, что не имеет адреса (например,
    // nil)
    throw_eval_error(form, fmt::format("the-as: cannot cast object of type {} to {}",
                                       target.class_name(), new_type_name));
}

/*!
 * Iterate over all fields
 */
Object Interpreter::eval_type_for_each_field(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}, {ObjectType::FUNCTION}}, {});

    auto root_struct = args.unnamed[0].as_heap_obj<StructureType>();
    if (!root_struct) {
        throw_eval_error(form, "for-each-field: expected structure type");
    }
    Object lambda = args.unnamed[1];

    auto &ts = TypeSystem::instance();
    auto  idx = 0;
    // Рекурсивная функция обхода
    std::function<void(StructureType *, int)> walk = [&](StructureType *current_struct,
                                                         int            current_offset) {
        auto &fields = current_struct->fields();
        for (auto &field : fields) {
            int             field_offset = current_offset + field.offset();
            const TypeSpec &tspec = field.type();

            // Проверка: является ли поле указателем?
            // В твоей системе это (pointer <type>), т.е. base_type == "pointer"
            bool is_pointer = (tspec.base_type() == "pointer");

            // Ищем объект типа в системе типов
            auto field_type_ptr = ts.lookup_type(tspec.base_type());
            if (!field_type_ptr)
                continue;

            auto sub_struct = dynamic_cast<StructureType *>(field_type_ptr);

            // Условие рекурсии:
            // 1. Это структура (sub_struct != nullptr)
            // 2. Это inline-поле (m_inline == true)
            // 3. Это НЕ указатель
            if (sub_struct && field.is_inline() && !is_pointer) {
                walk(sub_struct, field_offset);
            } else {
                // Терминальное поле (базовый тип или указатель на структуру)
                Arguments callback_args;
                callback_args.unnamed = {Object::make_integer(idx),
                                         Object::make_integer(field_offset),
                                         // Передаем Field как HeapObject (shared_ptr)
                                         Object::make_heap_obj(std::make_shared<Field>(field))};
                call_lambda_internal(form, lambda, callback_args.unnamed, env);
                idx++;
            }
        }
    };

    walk(root_struct.get(), 0);
    return get_null();
}

/*!
 * Iterate over all methods
 */
Object Interpreter::eval_type_for_each_method(const Object &form, Arguments &args,
                                              const std::shared_ptr<EnvironmentObject> &env) {
    // Проверка аргументов: тип-структура и лямбда
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}, {ObjectType::FUNCTION}}, {});

    auto root_type = args.unnamed[0].as_heap_obj<Type>();
    if (!root_type) {
        throw_eval_error(form, "for-each-method: expected structure type");
    }
    Object lambda = args.unnamed[1];

    // У структур обычно есть список методов
    // Нам нужно пройтись по всем методам, включая унаследованные,
    // либо согласно их ID в vtable.

    auto max_id = root_type->methods_max_id();
    for (size_t i = 0; i <= max_id; ++i) {
        MethodInfo method;
        bool       found_method = false;
        Type      *current_type = root_type.get();

        while (current_type) {
            // Ищем метод в ТЕКУЩЕМ типе итерации
            if (i == 0) {
                if (current_type->has_new_method())
                    found_method = current_type->get_my_new_method(&method);
            } else {
                found_method = current_type->get_my_method(i, &method);
            }

            if (found_method) {
                // Нашли! Либо в самом типе, либо у предка.
                break;
            }

            // Если не нашли, идем выше
            if (current_type->get_name() == "object")
                break;

            auto parent_name = current_type->get_parent();
            if (parent_name.empty())
                break; // Защита от пустых имен родителей

            current_type = TypeSystem::instance().lookup_type(parent_name);
        }

        // Вызов лямбды
        Arguments callback_args;
        callback_args.unnamed.push_back(Object::make_integer(i));
        callback_args.unnamed.push_back(
            found_method ? Object::make_heap_obj(std::make_shared<MethodInfo>(method))
                         : get_null());

        call_lambda_internal(form, lambda, callback_args.unnamed, env);
    }

    return get_null();
}

// ============================================================
// Declaration
// ============================================================

Object Interpreter::eval_declare_type(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::SYMBOL}, {ObjectType::SYMBOL}}, {});

    auto kind = args.unnamed.at(1).to_std_string();
    auto type_name = args.unnamed.at(0).as_symbol();

    auto &ts = TypeSystem::instance();
    ts.forward_declare_type_as(type_name.name_ptr, kind);

    // Local table of types
    // 1. Достаем shared_ptr из таблицы
    auto existing_type_ptr = m_symbol_types.lookup(type_name);

    // 2. Если он есть, разыменовываем его дважды для сравнения объектов
    if (existing_type_ptr) {
        // (*existing_type_ptr) -> это shared_ptr
        // (**existing_type_ptr) -> это сам объект TypeSpec
        if (**existing_type_ptr != TypeSpec("type")) {
            throw_eval_error(form, "Cannot forward declare {} as a type: it is already a {}",
                             type_name.name_ptr, (*existing_type_ptr)->print());
        }
    }
    m_symbol_types.set(type_name, std::make_shared<TypeSpec>("type"));

    return get_none();
}

/*!
 * Declare type for type specification
 */
Object Interpreter::eval_declare_external(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    // Проверка аргументов: (define-extern симбол тип)
    if (args.unnamed.size() < 2) {
        throw_eval_error(form,
                         "declare-extern must have at least 2 arguments: symbol and typespec");
    }

    auto type_name = args.unnamed.at(0);
    if (!type_name.is_symbol()) {
        throw_type_mismatch(form, args, 0, {ObjectType::SYMBOL}, type_name.type);
    }
    auto     type_arg = args.unnamed.at(1);
    TypeSpec new_type;
    if (type_arg.is_symbol() || type_arg.is_pair()) {
        // 1. Парсим TypeSpec через рекурсивный парсер
        // (функцию parse_typespec нужно добавить в TypeSystem или Interpreter)
        try {
            new_type = parse_typespec_helper(type_arg);
        } catch (std::runtime_error &e) {
            throw_eval_error(form, e.what());
        }
    } else if (type_arg.is_native_obj<TypeSpec>()) {
        new_type = *type_arg.as_native_obj<TypeSpec>();
    } else {
        throw_type_mismatch(form, args, 1, {"symbol", "pair", "type-spec"}, type_arg.class_name());
    }

    // 1. Получаем указатель на запись в таблице
    auto existing_type_ptr = m_symbol_types.lookup(type_name.as_symbol());

    if (existing_type_ptr) {
        // Извлекаем сам объект TypeSpec для удобства (через разыменование shared_ptr)
        const TypeSpec &old_ts = **existing_type_ptr;

        if (old_ts != new_type) {
            // Проверяем совместимость: может ли new_type заменить old_ts?
            if (!TypeSystem::instance().tc(old_ts, new_type)) {
                fmt::print("WARNING: Redefining symbol {} from {} to {}\n",
                           type_name.as_symbol().name_ptr, old_ts.print(), new_type.print());
            }
        }
    }

    // 3. Регистрируем тип символа (оборачиваем в shared_ptr)
    m_symbol_types.set(type_name.as_symbol(), std::make_shared<TypeSpec>(new_type));

    return get_none();
}

/*!
 * A (declare ...) form can be used to configure settings inside a function.
 * Currently there aren't many useful settings, but more may be added in the future.
 *
 *  ;; Пример 1: Объявление встраиваемой функции
 * (defun add (x y)
 *   (declare (inline))  ;; Функция будет автоматически встраиваться
 *   (+ x y))
 *
 * ;; Пример 2: Разрешение встраивания (но не по умолчанию)
 * (defun multiply (x y)
 *   (declare (allow-inline))  ;; Компилятор может встроить по своему усмотрению
 *   (* x y))
 *
 * ;; Пример 3: Объявление ассемблерной функции с типом возврата
 * (defun sys-call (a b)
 *   (declare (asm-func int))  ;; Ассемблерная функция, возвращающая int
 *   (asm "..." a b))
 *
 * ;; Пример 4: Ассемблерная функция с разными типами
 * (defun get-pointer ()
 *   (declare (asm-func void*))  ;; Возвращает указатель
 *   (asm "mov rax, [rbp+8]; ret"))
 *
 * (defun read-byte ()
 *   (declare (asm-func unsigned-char))  ;; Возвращает беззнаковый символ
 *   (asm "mov al, [rdi]; ret"))
 *
 * ;; Пример 5: Включение печати ассемблерного кода
 * (defun optimized-func (x)
 *   (declare (print-asm))  ;; Выведет сгенерированный ассемблерный код
 *   (declare (inline))     ;; Комбинация нескольких declare
 *   (* x 42))
 *
 * ;; Пример 6: Ассемблерная функция с разрешением использования saved registers
 * (defun context-switch ()
 *   (declare (asm-func void))
 *   (declare (allow-saved-regs))  ;; Разрешает использовать регистры, сохраняемые между
 * вызовами (asm "push rbx; push r12; ..."))
 *
 * ;; Пример 7: Комбинация нескольких опций
 * (defun critical-func (x)
 *   (declare (inline))
 *   (declare (print-asm))
 *   (declare (allow-saved-regs))
 *   ;; тело функции
 *   )
 */
Object Interpreter::eval_declare_special(const Object &form, const Object &rest,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    // auto fe_asm = env->get_asm_function_env();
    // auto fe_func = env->get_function_env();
    // fmt::print("DEBUG ENTRY: env:{:p} | found_asm:{:p} | found_func:{:p}\n", (void *)env.get(),
    //            (void *)(fe_asm ? fe_asm.get() : nullptr),
    //            (void *)(fe_func ? fe_func.get() : nullptr));

    // 1. Поиск функционального окружения
    // По умолчанию ищем asm окружение которое уже существует
    auto fe = env->get_declared_function_env();

    // Если не нашли берем ближайше
    if (!fe)
        fe = env->get_function_env();

    // Если не нашли неудача
    if (!fe || fe->owner_function.is_none()) {
        throw_eval_error(form, "Cannot use 'declare' outside of a function body.");
    }

    // В болтшинство случаем декларируем это окружение но если нет аргумента (this)
    auto  func = fe->owner_function.as_function();
    auto *settings = &func->declarations;
    int   index = 0;
    fmt::print("DEBUG declare {:p} {}\n", (void *)fe.get(), settings->return_type.print());
    // 2. Итерация по списку спецификаций: ( (option1) (option2 ...) )
    for_each_in_list(rest, [&](const Object &o) {
        if (!o.is_pair()) {
            throw_eval_error(o, "Invalid declare specification: expected a list.");
        }

        auto spec = o.as_pair();
        auto first = spec->car;
        auto args = spec->cdr; // Это список аргументов опции

        if (!first.is_symbol()) {
            throw_eval_error(first, "Declare option must be a symbol.");
        }

        std::string name = first.to_std_string();
        if (name == "this") {
            // Необходимо сменить окружене нужно работать с ближайшим
            if (index > 0)
                throw_eval_error(o, "The option 'this' must be first in the list.");
            if (!args.is_null())
                throw_eval_error(first, "Option 'this' expects no arguments.");
            fe = env->get_function_env();
            func = fe->owner_function.as_function();
            settings = &func->declarations;
            // fmt::print("DEBUG declare {:p} {}\n", (void *)fe.get(),
            // settings->return_type.print());
        } else if (name == "inline") {
            if (!args.is_null())
                throw_eval_error(first, "Option 'inline' expects no arguments.");
            settings->allow_inline = true;
            settings->inline_by_default = true;
            settings->save_code = true;
        } else if (name == "allow-inline") {
            if (!args.is_null())
                throw_eval_error(first, "Option 'allow-inline' expects no arguments.");
            settings->allow_inline = true;
            settings->inline_by_default = false;
            settings->save_code = true;
        } else if (name == "asm-func") {
            fe->is_declared_func = true;

            // Ожидаем (asm-func return_type)
            if (!args.is_pair() || !args.as_pair()->cdr.is_null()) {
                throw_eval_error(first,
                                 "Option 'asm-func' expects exactly one argument: return_type.");
            }

            auto ret_type_expr = args.as_pair()->car;
            if (!(ret_type_expr.is_symbol() || ret_type_expr.is_string() ||
                  ret_type_expr.is_pair())) {
                throw_eval_error(ret_type_expr, "Return type must be a symbol, string or pair.");
            }
            // проверим тип возврата
            TypeSpec ts = parse_typespec(&TypeSystem::instance(), ret_type_expr);

            settings->return_type = ret_type_expr;
            // fmt::print("DEBUG declare {:p} {}\n", (void *)fe.get(),
            // settings->return_type.print());
        } else if (name == "print-asm") {
            if (!args.is_null())
                throw_eval_error(first, "Option 'print-asm' expects no arguments.");
            settings->print_asm = true;
        } else if (name == "allow-saved-regs") {
            if (!args.is_null())
                throw_eval_error(first, "Option 'allow-saved-regs' expects no arguments.");
            settings->allow_saved_regs = true;
        } else {
            throw_eval_error(first, "Unrecognized declare option: {}.", name);
            return;
        }
        index++;
    });

    settings->is_set = true;

    return get_none();
}

/*!
 * Make a list of all declarations of lambda (see declare method)
 */
Object Interpreter::eval_declarations(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    vararg_check(form, args, {}, {{"name", {false, {ObjectType::SYMBOL}}}});

    auto fe = env->get_function_env();
    if (fe.get() == nullptr) {
        throw_eval_error(form, "Cannot use function metadata outside of a function.");
    }

    if (!fe->owner_function.is_function()) {
        throw_eval_error(form, "Fuction environment does not have pointer to function.");
    }
    auto  func = fe->owner_function.as_function();
    auto &settings = func->declarations;
    // Make result
    ListBuilder lb(&symbol_table());
    lb.add_keyword("declarations");
    lb.add_key_value("is-set",
                     true_or_false(settings.is_set)); // has the user set these with a (declare)?
    lb.add_key_value(
        "inline-by-default",
        true_or_false(settings.inline_by_default)); // if a function, inline when possible?
    lb.add_key_value("save-code",
                     true_or_false(settings.save_code)); // if a function, should we save the code?
    lb.add_key_value(
        "allow-inline",
        true_or_false(
            settings.allow_inline)); // should we allow the user to use this an inline function
    lb.add_key_value(
        "print-asm",
        true_or_false(settings.print_asm)); // should we print out the asm for this function?
    lb.add_key_value("typespec", settings.typespec);

    return lb.build();
}

Object Interpreter::eval_define_function(const Object &form, Arguments &args,
                                         const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверка аргументов: (define-function ИМЯ ЛЯМБДА)
    vararg_check(form, args,
                 {
                     {ObjectType::SYMBOL},  // 0: Имя функции (символ)
                     {ObjectType::FUNCTION} // 1: Тело функции (лямбда)
                 },
                 {});

    auto   sym_obj = args.unnamed[0];
    auto   sym = sym_obj.as_symbol();
    Object implementation = args.unnamed[1];
    auto   lambda_ptr = implementation.as_heap_obj<FunctionObject>();

    // 2. Проверка наличия декларации типов внутри лямбды
    // В GOAL/SOOT функция обязана иметь typespec (сигнатуру), чтобы компилятор знал, что
    // делать.
    if (!lambda_ptr->declarations.is_set || lambda_ptr->declarations.typespec.is_null()) {
        throw_eval_error(
            form,
            fmt::format("define-function '{}' expects a declared typespec in the lambda object",
                        sym.name_ptr));
    }

    const TypeSpec &impl_spec = *lambda_ptr->declarations.typespec.as_heap_obj<TypeSpec>();

    // 3. Проверка констант и существующих типов (защита от коллизий)
    if (m_global_constants.lookup(sym)) {
        throw_eval_error(sym_obj, fmt::format("Cannot define function: symbol '{}' is a constant",
                                              sym.name_ptr));
    }

    // 4. Проверка сигнатуры через существующую таблицу типов символов
    auto existing_type_ptr = m_symbol_types.lookup(sym);
    if (existing_type_ptr) {
        const TypeSpec &old_ts = **existing_type_ptr;

        // Если это не функция или сигнатуры несовместимы — ругаемся или предупреждаем
        if (!TypeSystem::instance().tc(old_ts, impl_spec)) {
            fmt::print(stderr,
                       "WARNING: Signature mismatch for function '{}'. Expected {}, got {}\n",
                       sym.name_ptr, old_ts.print(), impl_spec.print());
        }
    }

    // 5. Регистрация
    // Записываем тип (сигнатуру) в глобальную таблицу типов
    m_symbol_types.set(sym, std::make_shared<TypeSpec>(impl_spec));

    // Записываем саму реализацию в глобальное окружение
    // (Используем m_global_environment, чтобы функция была видна везде)
    m_global_environment.as_env()->vars.set(sym, implementation);

    return sym_obj; // Обычно возвращают имя функции
}

/*!
 * Make type spec for function
 */
Object Interpreter::eval_function_type_make(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)args;

    // 1. Поиск функционального окружения.
    // Сначала ищем специализированное ASM-окружение, затем любое функциональное.
    auto fe = env->get_declared_function_env();
    if (!fe) {
        fe = env->get_function_env();
    }

    // Проверка на существование и валидность владельца
    if (!fe || fe->owner_function.is_none() || !fe->owner_function.is_function()) {
        throw_eval_error(form, "function-type-make: cannot find a valid function environment.");
    }

    // Извлекаем объект функции и его настройки
    auto  func_ptr = fe->owner_function.as_heap_obj<FunctionObject>();
    auto &settings = func_ptr->declarations;

    // fmt::print("DEBUG function type {:p} {}\n", (void *)fe.get(), settings.return_type.print());

    // Проверяем, что (declare (asm-func ...)) был вызван ранее и установил return_type
    if (settings.return_type.is_none()) {
        throw_eval_error(form,
                         "function-type-make: requires a return type (did you use 'declare'?)");
    }

    // 2. Поиск rlet окружения
    // Это сердце сигнатуры — здесь лежат алиасы регистров и их типы
    auto rlet_env = env->get_reg_let_env();
    if (!rlet_env) {
        throw_eval_error(
            form, "function-type-make: requires an active 'rlet' scope to resolve arguments.");
    }

    // 3. Сборка финального TypeSpec
    // build_typespec_from_env соберет (function <return_type> (<arg1-type> <arg2-type> ...))
    settings.typespec =
        TypeSystem::instance().build_typespec_from_env(&symbol_table(), rlet_env, settings.return_type);

    return settings.typespec;
}

/*!
 * Get type signature of function
 */
Object Interpreter::eval_function_type_get(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверка аргументов: (define-function ИМЯ ЛЯМБДА)
    vararg_check(form, args,
                 {
                     {ObjectType::FUNCTION} // 1: Тело функции (лямбда)
                 },
                 {});

    Object implementation = args.unnamed[0];
    auto   lambda_ptr = implementation.as_heap_obj<FunctionObject>();

    // 2. Проверка наличия декларации типов внутри лямбды
    // В GOAL/SOOT функция обязана иметь typespec (сигнатуру), чтобы компилятор знал, что
    // делать.
    if (!lambda_ptr->declarations.is_set || lambda_ptr->declarations.typespec.is_null()) {
        throw_eval_error(
            form, fmt::format("function-type '{}' expects a declared typespec in the lambda object",
                              implementation.print()));
    }

    return lambda_ptr->declarations.typespec; // Обычно возвращают имя функции
}

/*!
 * Set type signature of function
 */
Object Interpreter::eval_function_type_set(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::FUNCTION}, {ObjectType::NATIVE_OBJECT}}, {});

    Object function_arg = args.unnamed[0];
    auto   function_ptr = function_arg.as_heap_obj<FunctionObject>();

    auto type_arg = args.unnamed[1];
    if (!type_arg.is_native_obj<TypeSpec>()) {
        throw_type_mismatch(form, args, 1, {"type-spec"}, type_arg.class_name());
    }

    function_ptr->declarations.is_set = true;
    function_ptr->declarations.typespec = type_arg;
    return get_none();
}

/*!
 * Get name of function if it was defined with (function-name! func name)
 */
Object Interpreter::eval_function_name_get(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // 1. Проверка аргументов: (define-function ИМЯ ЛЯМБДА)
    vararg_check(form, args,
                 {
                     {ObjectType::FUNCTION} // 1: Тело функции (лямбда)
                 },
                 {});

    Object implementation = args.unnamed[0];
    auto   lambda_ptr = implementation.as_heap_obj<FunctionObject>();

    return Object::make_symbol(&symbol_table(), lambda_ptr->name);
}

/*!
 * Set name of function (function-name! func name)
 */
Object Interpreter::eval_function_name_set(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::FUNCTION}, {ObjectType::SYMBOL}}, {});

    Object implementation = args.unnamed[0];
    auto   lambda_ptr = implementation.as_heap_obj<FunctionObject>();
    lambda_ptr->name = args.unnamed[1].to_std_string();
    return get_none();
}

/*!
 * Check if types are equal
 */
Object Interpreter::eval_types_match_p(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(
        form, args,
        {
            {ObjectType::NATIVE_OBJECT}, // 0: Имя функции (символ)
            {ObjectType::NATIVE_OBJECT}, // 1: Тело функции (лямбда)

        },
        {
            {"print", {false, {ObjectType::SYMBOL}}},          // Print on screen when not match
            {"throw", {false, {ObjectType::SYMBOL}}},          // Throw exception when not match
            {"allow-alias", {false, {ObjectType::SYMBOL}}},    // Allow few hardcoded names
            {"allow-abstract", {false, {ObjectType::SYMBOL}}}, // Make _type_ match to anything
            {"source", {false, {ObjectType::STRING}}},         // The error will have this text
        });
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // N.B. The aliases are:
    // meters float
    // degrees float
    // time-frame int
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto doprint = is_true(args.unnamed[0]);
    auto func1_obj = args.unnamed[0];
    auto func1_ptr = func1_obj.as_heap_obj<TypeSpec>();
    auto func2_obj = args.unnamed[1];
    auto func2_ptr = func2_obj.as_heap_obj<TypeSpec>();

    // Проверка наличия декларации типов внутри лямбды
    if (!func1_ptr) {
        throw_type_mismatch(form, args, 0, {"type-spec"}, func1_obj.class_name());
    }
    if (!func2_ptr) {
        throw_type_mismatch(form, args, 1, {"type-spec"}, func2_obj.class_name());
    }

    std::string error_source_name;
    bool        print_on_error = false;
    bool        throw_on_error = false;
    bool        allow_type_alias = false;
    bool        allow_type_abstract = false;
    if (args.has_named("source"))
        error_source_name = args.named["source"].to_std_string();

    if (args.has_named("print"))
        print_on_error = is_true(args.named["print"]);

    if (args.has_named("throw"))
        throw_on_error = is_true(args.named["throw"]);

    if (args.has_named("allow-alias"))
        allow_type_alias = is_true(args.named["allow-alias"]);

    if (args.has_named("allow-abstract"))
        allow_type_abstract = is_true(args.named["allow-abstract"]);

    // Проверка сигнатуры через существующую таблицу типов символов
    auto t1 = func1_ptr.get();
    auto t2 = func2_ptr.get();

    // Если это не функция или сигнатуры несовместимы — ругаемся или предупреждаем
    if (!TypeSystem::instance().typecheck_and_throw(*t1, *t2, error_source_name, print_on_error,
                                                    throw_on_error, allow_type_alias,
                                                    allow_type_abstract)) {
        if (doprint) {
            throw_eval_error(form, fmt::format("WARNING: Signature mismatch, expected {}, got {}\n",
                                               t1->print(), t2->print()));
        }

        return get_false();
    }
    return get_true();
}

// ============================================================
// Archiving and buffering
// ============================================================

/*!
 * Create memory archive object
 */
Object Interpreter::eval_make_memory_archive(const Object &form, Arguments &args,
                                             const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}},
                 {{"reading", {false, {ObjectType::SYMBOL}}},
                  {"writing", {false, {ObjectType::SYMBOL}}},
                  {"persistant", {false, {ObjectType::SYMBOL}}}});

    bool reading = false;
    bool writing = false;
    bool persistant = false;

    if (args.has_named("reading"))
        reading = is_true(args.named["reading"]);
    if (args.has_named("writing"))
        writing = is_true(args.named["writing"]);
    if (args.has_named("persistant"))
        persistant = is_true(args.named["persistant"]);

    if (!args.unnamed[0].is_native_obj<MemoryRegion>())
        throw_type_mismatch(form, args, 0, {"memory-region"}, args.unnamed[0].class_name());

    auto memory_region = args.unnamed[0].as_native_obj<MemoryRegion>();

    auto memory_archive =
        std::make_shared<MemoryArchive>(&symbol_table(), memory_region, reading, writing, persistant);
    return Object::make_heap_obj(memory_archive, ObjectType::NATIVE_OBJECT);
}

/*!
 * Create memory archive object
 */
Object Interpreter::eval_make_memory_region(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::INT}}, {{"base", {false, {ObjectType::INT}}}});

    uint region_size = args.unnamed[0].as_integer();
    uint region_base = 0;

    if (args.has_named("base"))
        region_base = is_true(args.named["base"]);

    auto memory_region = std::make_shared<MemoryRegion>(region_size, region_base);
    return Object::make_heap_obj(memory_region, ObjectType::NATIVE_OBJECT);
}

/*!
 * Ожидаем: (memory-region-export-hex region "path/to/file.hex" [:start N] [:end M] [:append?])
 */
Object Interpreter::eval_memory_region_export_hex(const Object &form, Arguments &args,
                                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT}, // memory-region
                  {ObjectType::STRING}},       // path
                 {{"start", {false, {ObjectType::INT}}},
                  {"end", {false, {ObjectType::INT}}},
                  {"append", {false, {ObjectType::SYMBOL}}}});

    // 1. Получаем регион
    auto region = args.unnamed[0].as_native_obj<MemoryRegion>();
    if (!region) {
        throw_eval_error(form, "First argument must be memory-region");
    }

    // 2. Получаем путь
    std::string path = args.unnamed[1].to_std_string();

    // 3. Опции
    size_t start = 0;
    if (args.has_named("start")) {
        start = args.named["start"].as_integer();
    }

    size_t end = 0; // 0 означает до конца
    if (args.has_named("end")) {
        end = args.named["end"].as_integer();
    }

    bool append = false;
    if (args.has_named("append")) {
        append = is_true(args.named["append"]);
    }

    // 4. Экспортируем
    bool result = region->export_intel_hex_file(path, start, end, 0, append);

    return result ? get_true() : get_false();
}

/*!
 * Create memory buffer object
 */
Object Interpreter::eval_make_memory_buffer(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {},
                 {{"region", {false, {ObjectType::NATIVE_OBJECT}}},
                  {"size", {false, {ObjectType::INT}}},
                  {"base", {false, {ObjectType::INT}}}});

    uint                          region_size = 0;
    uint                          region_base = 0;
    std::shared_ptr<MemoryRegion> memory_region;
    if (args.has_named("region")) {
        auto region_obj = args.named["region"];
        if (region_obj.is_native_obj<MemoryRegion>()) {
            memory_region = region_obj.as_native_obj<MemoryRegion>();
        } else {
            throw_named_type_mismatch(form, "region", {"memory-region"}, region_obj.class_name());
        }
    } else {

        if (args.has_named("size"))
            region_size = args.named["size"].as_integer();
        if (args.has_named("base"))
            region_base = args.named["base"].as_integer();

        if (region_size == 0)
            throw_eval_error(form, "make memory buffer required a :region or :size");
        memory_region = std::make_shared<MemoryRegion>(region_size, region_base);
    }

    auto memory_buffer = std::make_shared<MemoryBuffer>(memory_region);
    return Object::make_heap_obj(memory_buffer, ObjectType::NATIVE_OBJECT);
}

/*!
 * @brief Визуализация содержимого памяти (Hex Dump).
 * Генерирует форматированную строку, представляющую сырые байты буфера
 * в человекочитаемом виде (шестнадцатеричный код + ASCII).
 */
Object Interpreter::eval_memory_region_dump(const Object &form, Arguments &args,
                                            const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}},
                 {{"address", {false, {ObjectType::INT}}},
                  {"size", {false, {ObjectType::INT}}},
                  {"ascii", {false, {ObjectType::SYMBOL}}},
                  {"width", {false, {ObjectType::INT}}}});
    auto memory_region = args.unnamed[0].as_native_obj<MemoryRegion>();
    if (!memory_region.get())
        throw_type_mismatch(form, args, 0, {"memory-region"}, args.unnamed[0].class_name());
    auto start_offset = args.has_named("address") ? args.named["address"].as_integer() : 0;
    auto bytes_to_dump = args.has_named("size") ? args.named["size"].as_integer() : 256;
    auto show_ascii = args.has_named("ascii") ? is_true(args.named["ascii"]) : true;
    int  bytes_per_line = args.has_named("width") ? args.named["width"].as_integer() : 16;
    auto dump_string =
        memory_region->hex_dump(start_offset, bytes_to_dump, show_ascii, bytes_per_line);
    return Object::make_string(dump_string);
}

/*!
 * (buffer-label-set! buffer name :offset offset :segment segment :info info)
 */
Object Interpreter::eval_memory_buffer_label_set(const Object &form, Arguments &args,
                                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    // Формат: (memory-buffer-label-set! buffer name
    //           [:address offset] [:segment seg] [:info metadata])
    vararg_check(
        form, args,
        {{ObjectType::NATIVE_OBJECT},               // memory-buffer
         {ObjectType::STRING, ObjectType::SYMBOL}}, // name
        {{"offset", {false, {ObjectType::INT}}},
         {"segment", {false, {ObjectType::STRING, ObjectType::SYMBOL, ObjectType::STRING}}},
         {"info", {false, {}}}}); // любой тип

    auto buffer_obj = args.unnamed[0];
    auto buffer = buffer_obj.as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form,
                         "First argument must be memory-buffer, got " + buffer_obj.class_name());
    }

    std::string name = args.unnamed[1].to_std_string();

    // Получаем значения из keyword аргументов
    size_t offset = 0;
    if (args.has_named("offset")) {
        offset = args.named["offset"].as_integer();
    }

    Object segment = Object::make_null();
    if (args.has_named("segment")) {
        segment = args.named["segment"];
    }

    Object info = Object::make_null();
    if (args.has_named("info")) {
        info = args.named["info"];
    }

    // Добавляем метку
    buffer->labels()->add_label(name, offset, segment, info);

    return get_true();
}

/*!
 * (buffer-label-ref buffer name) -> offset или null
 */
Object Interpreter::eval_memory_buffer_label_ref(const Object &form, Arguments &args,
                                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT},               // memory-buffer
                  {ObjectType::STRING, ObjectType::SYMBOL}}, // name
                 {{"offset-only", {false, {ObjectType::SYMBOL}}}});

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    std::string name = args.unnamed[1].to_std_string();

    // Если нужен только адрес (для обратной совместимости)
    if (args.has_named("offset-only") && is_true(args.named["offset-only"])) {
        auto addr = buffer->labels()->get_offset(name);
        return addr ? Object::make_integer(*addr) : Object::make_null();
    }

    // Иначе возвращаем объект MemoryLabel
    return buffer->labels()->get_label_object(name);
}

/*!
 * Write relocation (buffer-symbol-set! buffer name) или (buffer-symbol-set! buffer name true)
 */
Object Interpreter::eval_memory_buffer_symbol_set(const Object &form, Arguments &args,
                                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    // (buffer-symbol-set! buffer name) или (buffer-symbol-set! buffer name true)
    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT},               // memory-buffer
                  {ObjectType::STRING, ObjectType::SYMBOL}}, // name
                 {});

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    std::string name = args.unnamed[1].to_std_string();

    size_t index = buffer->symbols()->add_symbol(name);
    return Object::make_integer(index);
}

/*!
 * Read symbol
 * (buffer-symbol-ref buffer name) -> info или null
 * (buffer-symbol-ref buffer index) -> info или null
 */
Object Interpreter::eval_memory_buffer_symbol_ref(const Object &form, Arguments &args,
                                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT},                                // memory-buffer
                  {ObjectType::STRING, ObjectType::SYMBOL, ObjectType::INT}}, // name or index
                 {{"crc32", {false, {ObjectType::SYMBOL}}}});

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    if (args.unnamed[1].is_string() || args.unnamed[1].is_symbol()) {
        // Поиск по имени
        std::string name = args.unnamed[1].to_std_string();
        auto        idx = buffer->symbols()->find_by_name(name);
        if (idx) {
            // Возвращаем информацию о символе
            return pretty_print::build_list(
                pretty_print::build_list(Object::make_keyword(&symbol_table(),"name"),
                                         Object::make_string(buffer->symbols()->get_name(*idx))),
                pretty_print::build_list(Object::make_keyword(&symbol_table(),"crc32"),
                                         Object::make_integer(buffer->symbols()->get_crc32(*idx))),
                pretty_print::build_list(Object::make_keyword(&symbol_table(),"index"),
                                         Object::make_integer(*idx)));
        }
    } else if (args.unnamed[1].is_integer()) {
        // Поиск по индексу или CRC32
        size_t value = args.unnamed[1].as_integer();

        // Режим CRC32 если указан ключ :crc32
        if (args.has_named("crc32") && is_true(args.named["crc32"])) {
            // Поиск по CRC32
            auto idx = buffer->symbols()->find_by_crc32(value);
            if (idx) {
                return pretty_print::build_list(
                    pretty_print::build_list(
                        Object::make_keyword(&symbol_table(), "name"),
                        Object::make_string(buffer->symbols()->get_name(*idx))),
                    pretty_print::build_list(
                        Object::make_keyword(&symbol_table(),"crc32"),
                        Object::make_integer(buffer->symbols()->get_crc32(*idx))),
                    pretty_print::build_list(Object::make_keyword(&symbol_table(),"index"),
                                             Object::make_integer(*idx)));
            }
        } else {
            // Обычный поиск по индексу
            if (value < buffer->symbols()->size()) {
                return pretty_print::build_list(
                    pretty_print::build_list(
                        Object::make_keyword(&symbol_table(),"name"),
                        Object::make_string(buffer->symbols()->get_name(value))),
                    pretty_print::build_list(
                        Object::make_keyword(&symbol_table(),"crc32"),
                        Object::make_integer(buffer->symbols()->get_crc32(value))),
                    pretty_print::build_list(Object::make_keyword(&symbol_table(),"index"),
                                             Object::make_integer(value)));
            }
        }
    }

    return Object::make_null();
}

/*!
 * Write relocation
 * (buffer-reloc-set! buffer offset type target)
 * type может быть :abs, :rel, :crc, :table
 */
Object Interpreter::eval_memory_buffer_reloc_set(const Object &form, Arguments &args,
                                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT},               // memory-buffer
                  {ObjectType::INT},                         // offset
                  {ObjectType::SYMBOL},                      // type
                  {ObjectType::STRING, ObjectType::SYMBOL}}, // target
                 {});

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    size_t      offset = args.unnamed[1].as_integer();
    std::string type_str = args.unnamed[2].to_std_string();
    std::string target = args.unnamed[3].to_std_string();

    // Преобразуем тип
    RelocationTable::RelocType type;
    if (type_str == ":abs" || type_str == "abs" || type_str == "absolute") {
        type = RelocationTable::RelocType::ABS_ADDR;
    } else if (type_str == ":rel" || type_str == "rel" || type_str == "relative") {
        type = RelocationTable::RelocType::RELATIVES;
    } else if (type_str == ":crc" || type_str == "crc" || type_str == "symbol-crc") {
        type = RelocationTable::RelocType::SYMBOL_CRC;
    } else if (type_str == ":table" || type_str == "table" || type_str == "symbol-table") {
        type = RelocationTable::RelocType::SYMBOL_TABLE_REF;
    } else {
        throw_eval_error(form, "Invalid relocation type: " + type_str);
    }

    buffer->relocs()->add(offset, type, target);
    return get_true();
}

/*!
 * Read relocation (buffer-reloc-ref buffer [index]) -> список всех релокаций или одна по
 * индексу
 */
Object Interpreter::eval_memory_buffer_reloc_ref(const Object &form, Arguments &args,
                                                 const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}}, // memory-buffer
                 {{"index", {false, {ObjectType::INT}}},
                  {"target", {false, {ObjectType::STRING, ObjectType::SYMBOL}}}});

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    const auto &relocs = buffer->relocs()->get();

    // ============================================================
    // Режим 1: Поиск по индексу
    // ============================================================
    if (args.has_named("index")) {
        size_t index = args.named["index"].as_integer();

        if (index >= relocs.size()) {
            return Object::make_null();
        }

        const auto &reloc = relocs[index];
        std::string type_str = relocation_type_to_string(reloc.type);

        return pretty_print::build_list(
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "offset"),
                                     Object::make_integer(reloc.offset)),
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "type"), Object::make_symbol(&symbol_table(), type_str)),
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "target"),
                                     Object::make_string(reloc.target_name)));
    }

    // ============================================================
    // Режим 2: Поиск по target
    // ============================================================
    if (args.has_named("target")) {
        std::string         target_name = args.named["target"].to_std_string();
        std::vector<Object> found;

        for (const auto &reloc : relocs) {
            if (reloc.target_name == target_name) {
                std::string type_str = relocation_type_to_string(reloc.type);

                found.push_back(pretty_print::build_list(
                    pretty_print::build_list(Object::make_keyword(&symbol_table(), "offset"),
                                             Object::make_integer(reloc.offset)),
                    pretty_print::build_list(Object::make_keyword(&symbol_table(), "type"),
                                             Object::make_symbol(&symbol_table(), type_str)),
                    pretty_print::build_list(Object::make_keyword(&symbol_table(), "target"),
                                             Object::make_string(reloc.target_name))));
            }
        }

        if (found.empty()) {
            return Object::make_null();
        } else if (found.size() == 1) {
            return found[0];
        } else {
            return Object::make_list(found);
        }
    }

    // ============================================================
    // Режим 3: Без опций - возвращаем количество
    // ============================================================
    return Object::make_integer(relocs.size());
}

// Вспомогательная функция
std::string Interpreter::relocation_type_to_string(RelocationTable::RelocType type) {
    switch (type) {
    case RelocationTable::RelocType::ABS_ADDR:
        return ":abs";
    case RelocationTable::RelocType::RELATIVES:
        return ":rel";
    case RelocationTable::RelocType::SYMBOL_CRC:
        return ":crc";
    case RelocationTable::RelocType::SYMBOL_TABLE_REF:
        return ":table";
    default:
        return ":unknown";
    }
}

Object Interpreter::eval_memory_buffer_reloc_list(const Object &form, Arguments &args,
                                                  const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}}, {}); // memory-buffer

    auto buffer = args.unnamed[0].as_heap_obj<MemoryBuffer>();
    if (!buffer) {
        throw_eval_error(form, "First argument must be memory-buffer");
    }

    std::vector<Object> reloc_list;
    for (const auto &reloc : buffer->relocs()->get()) {
        std::string type_str;
        switch (reloc.type) {
        case RelocationTable::RelocType::ABS_ADDR:
            type_str = ":abs";
            break;
        case RelocationTable::RelocType::RELATIVES:
            type_str = ":rel";
            break;
        case RelocationTable::RelocType::SYMBOL_CRC:
            type_str = ":crc";
            break;
        case RelocationTable::RelocType::SYMBOL_TABLE_REF:
            type_str = ":table";
            break;
        default:
            type_str = ":unknown";
        }

        reloc_list.push_back(pretty_print::build_list(
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "offset"),
                                     Object::make_integer(reloc.offset)),
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "type"), Object::make_symbol(&symbol_table(), type_str)),
            pretty_print::build_list(Object::make_keyword(&symbol_table(), "target"),
                                     Object::make_string(reloc.target_name))));
    }

    return Object::make_list(reloc_list);
}
/*!
 * Два режима:
 * 1. Чтение:  (archive-serialize archive type) -> object
 * 2. Запись:  (archive-serialize archive type value) -> offset
 *
 * ;; Создаем буфер и архив
 * (define buffer (make-memory-buffer :size 65536 :origin 0))
 *
 * ;; ======
 * ;; ЗАПИСЬ
 * ;; ======
 *
 * ;; Вариант 1: явное создание архива
 * (define ar (make-memory-archive buffer :write #t))
 * (archive-serialize ar 'vec3 :value '(:x 1 :y 2 :z 3))
 * (archive-close! ar)
 *
 * ;; Вариант 2: с макросом (удобнее)
 * (with-archive (ar buffer :write)
 *   (archive-serialize ar 'vec3 :value '(:x 1 :y 2 :z 3))
 *   (archive-serialize ar 'point :value '(:x 10 :y 20)))
 *
 * ;; ======
 * ;; ЧТЕНИЕ
 * ;; ======
 *
 * (with-archive (ar buffer :read)
 *   (define v1 (archive-serialize ar 'vec3))    ; -> (:_type_ vec3 :x 1 :y 2 :z 3)
 *   (define p1 (archive-serialize ar 'point)))  ; -> (:_type_ point :x 10 :y 20)
 *
 * ;; Проверяем
 * (print v1)  ; (:_type_ vec3 :x 1 :y 2 :z 3)
 * (print p1)  ; (:_type_ point :x 10 :y 20)
 */
Object Interpreter::eval_archive_serialize(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;

    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT}, // archive
                  {ObjectType::SYMBOL}},       // type
                 {{"value", {false, {}}}});    // опциональное значение для записи

    // Получаем архив
    auto archive = args.unnamed[0].as_heap_obj<MemoryArchive>();
    if (!archive) {
        throw_eval_error(form, "First argument must be archive");
    }

    // Получаем тип
    std::string type_name = args.unnamed[1].to_std_string();
    Type       *type = TypeSystem::instance().lookup_type(type_name);
    if (!type) {
        throw_eval_error(form, "Unknown type: " + type_name);
    }

    // Режим записи: передан :value
    if (args.has_named("value")) {
        Object value = args.named["value"];

        // Запоминаем текущую позицию
        size_t start_pos = archive->tell();

        // Сериализуем значение
        type->serialize_obj(*archive, value);

        // Возвращаем смещение (сколько байт записано)
        size_t end_pos = archive->tell();
        return Object::make_integer(end_pos - start_pos);
    }

    // Режим чтения: нет :value
    else {
        // Создаем пустой объект
        Object result;

        // Читаем из архива
        type->serialize_obj(*archive, result);

        // Возвращаем прочитанный объект
        return result;
    }
}

/*!
 * Get position of archive
 */
Object Interpreter::eval_archive_tell(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}}, {});

    auto archive = args.unnamed[0].as_heap_obj<MemoryArchive>();
    if (!archive) {
        throw_eval_error(form, "Argument must be archive");
    }

    return Object::make_integer(archive->tell());
}

/*!
 * Change position of archive
 */
Object Interpreter::eval_archive_seek(const Object &form, Arguments &args,
                                      const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}, {ObjectType::INT}}, {});

    auto archive = args.unnamed[0].as_heap_obj<MemoryArchive>();
    if (!archive) {
        throw_eval_error(form, "First argument must be archive");
    }

    size_t pos = args.unnamed[1].as_integer();
    archive->seek(pos);
    return Object::make_none();
}

/*!
 * Check if archive at the end
 */
Object Interpreter::eval_archive_at_end(const Object &form, Arguments &args,
                                        const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}}, {});

    auto archive = args.unnamed[0].as_heap_obj<MemoryArchive>();
    if (!archive) {
        throw_eval_error(form, "Argument must be archive");
    }

    return archive->at_end() ? get_true() : get_false();
}

/*!
 * Close archive
 */
Object Interpreter::eval_archive_close(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}}, {});

    auto archive = args.unnamed[0].as_heap_obj<MemoryArchive>();
    if (!archive) {
        throw_eval_error(form, "Argument must be archive");
    }

    archive->close();
    return Object::make_none();
}

/*!
 * Direct read to region
 */
Object Interpreter::eval_region_read_byte(const Object &form, Arguments &args,
                                          const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args, {{ObjectType::NATIVE_OBJECT}, {ObjectType::INT}}, {});

    auto region = args.unnamed[0].as_native_obj<MemoryRegion>();
    if (!region) {
        throw_eval_error(form, "First argument must be memory-region");
    }

    size_t offset = args.unnamed[1].as_integer();
    size_t index = offset - region->base();
    if (index < 0 || index >= region->size()) {
        throw_eval_error(form,
                         fmt::format("Offset {:08x} out of bounds of full size {:08x}-{:08x}",
                                     offset, region->base(), region->base() + region->size()));
    }

    uint8_t value = region->data()[index];
    return Object::make_integer(value);
}

/*!
 * Direct wtite to region
 */
Object Interpreter::eval_region_write_byte(const Object &form, Arguments &args,
                                           const std::shared_ptr<EnvironmentObject> &env) {
    (void)env;
    vararg_check(form, args,
                 {{ObjectType::NATIVE_OBJECT}, // region
                  {ObjectType::INT},           // offset
                  {ObjectType::INT}},          // value
                 {});

    auto region = args.unnamed[0].as_native_obj<MemoryRegion>();
    if (!region) {
        throw_eval_error(form, "First argument must be memory-region");
    }

    size_t offset = args.unnamed[1].as_integer();
    size_t index = offset - region->base();
    if (index < 0 || index >= region->size()) {
        throw_eval_error(form,
                         fmt::format("Offset {:08x} out of bounds of full size {:08x}-{:08x}",
                                     offset, region->base(), region->base() + region->size()));
    }

    uint8_t value = args.unnamed[2].as_integer() & 0xFF;
    region->data()[index] = value;
    return Object::make_none();
}
} // namespace soot
