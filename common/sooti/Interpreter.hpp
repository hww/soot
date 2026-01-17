#pragma once

#include "common/sooti/Reader.hpp"
#include "common/sooti/Object.hpp"
#include <iostream>
#include <functional>
#include <unordered_map>
#include <memory>
#include "fmt/format.h"
#include "fmt/color.h"

namespace script
{
    class EvalException : public std::exception {
    public:
        Object form;                        // Тот самый объект (Pair или LexToken)
        std::string message;                // Текст ошибки
        bool already_printed = false;       // Не печай второй раз
        bool error_header_required = true; 
        bool detailed_error_required = true; 

        EvalException(Object f, std::string m) : form(f), message(std::move(m)) {}

        // Чтобы соответствовать стандарту std::exception
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class ExitException : public std::exception {
    public:
        int exit_code;
        std::string message; // Храним строку здесь

        explicit ExitException(int code = 0) 
            : exit_code(code), message(fmt::format("Exit with code {}", code)) {}

        const char* what() const noexcept override {
            return message.c_str(); // Теперь это безопасно
        }
    };

    class Interpreter {
    public:
        Interpreter(const std::string& username = "user", bool load_libs = false);

        void load_library();

        // Основные методы оценки
        Object eval(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string(const std::string& expression, const std::string& filename);
        Object call_lambda(const Object& lambda, const std::vector<Object>& args);

        // Для REPL и LSP
        std::string get_all_symbols_matching(const std::string& prefix);

        // Символы и окружение
        Object intern(const std::string& name);
        InternedSymbolPtr intern_ptr(const std::string& name);
        bool try_symbol_lookup(const Object& sym, const std::shared_ptr<EnvironmentObject>& env, Object* dest);
        Object eval_symbol(const Object& sym, const std::shared_ptr<EnvironmentObject>& env);
        void define_var_in_env(const Object& env, const Object& var, const char* name);

        // Вспомогательные методы
        Arguments get_args(const Object& form, const Object& rest, const ArgumentSpec& spec);
        void eval_args(Arguments* args, const std::shared_ptr<EnvironmentObject>& env);
        ArgumentSpec make_varargs();
        std::vector<Object> eval_list(const Object& list, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_list_return_last(const Object& form, Object rest, const std::shared_ptr<EnvironmentObject>& env);

        // Обработка ошибок
        void throw_eval_error(const Object& o, const std::string& err);

        // REPL
        void execute_repl();

        // Доступ к ридеру
        Reader& get_reader() { return reader; }

        // Лоступ к окружению
        Object get_global_environment() { return global_environment; }

        // Boolean helpers (используют символы)
        Object make_bool(bool value) { return value ? true_object : false_object; }
        bool is_true(const Object& o) const { return !is_false(o); }
        bool is_false(const Object& o) const { return o.is_symbol() && o.as_symbol().name_ptr == false_object.as_symbol().name_ptr; }
        bool is_bool(const Object& o) const { return o.is_symbol() && (o.as_symbol().name_ptr == false_object.as_symbol().name_ptr || o.as_symbol().name_ptr == true_object.as_symbol().name_ptr); }
        bool truthy(const Object& o) { return !is_false(o); }

        // Помощники для чисел
        bool is_number(const Object& obj);
        int64_t number_to_integer(const Object& obj);
        double number_to_float(const Object& obj);

    private:
        // === СПЕЦИАЛЬНЫЕ ФОРМЫ (не вычисляют аргументы) ===
        Object eval_quote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_define(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lambda(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_begin(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_set(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_let(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_if(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_and(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_or(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_cond(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_while(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_macro(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_let_star(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_quasiquote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_apply(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_macroexpand(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // === ВСТРОЕННЫЕ ФУНКЦИИ (вычисляют аргументы) ===
        // Математические
        Object eval_plus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_minus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_times(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_divide(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_numequals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_gt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_leq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_geq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Списки и пары
        Object eval_cons(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_list_func(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_null_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_pair_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Предикаты типов
        Object eval_symbol_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_number_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_char_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_vector_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_procedure_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_boolean_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_type_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_type_name(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Сравнение
        Object eval_equals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_eqv(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Строки
        Object eval_string_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_substr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_to_symbol(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_symbol_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Векторы
        Object eval_vector(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_vector_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_vector_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_vector_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_vector_to_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Хэш-таблицы
        Object eval_make_hash_table(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_try_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_to_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Системные и ввод-вывод
        Object eval_print(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_pprint(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_inspect(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_error(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_fmt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_cfmt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        
        // Logger
        Object eval_log(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) ;

        // файлы
        Object eval_file_exists_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_str(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_parse_str(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_load(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Reader
        Object eval_set_macro_character(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_get_macro_character(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_remove_macro_character(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_peek_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_delimited_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_reader_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // LexToken
        Object eval_make_lextoken(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lextoken_type(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lextoken_value(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lextoken_info(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_lextoken_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Система
        Object eval_system(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_get_env(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_exit(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_get_path(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_find_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Прочие
        Object eval_gensym(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_eval(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_set_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_set_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Преобразования типов
        Object eval_number_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string_to_number(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_char_to_integer(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_integer_to_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Математические функции
        Object eval_abs(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_max(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_min(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_expt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_sqrt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_ash(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Время
        Object eval_time_seconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_time_milliseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_time_microseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_time_nanoseconds(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);        
        // Quasiquote helpers
        Object quasiquote_helper(const Object& form, const std::shared_ptr<EnvironmentObject>& env);

    
        
        // Улучшенная обработка аргументов
        ArgumentSpec parse_arg_spec(const Object& form, Object& rest);
        void set_args_in_env(const Object& form, const Arguments& args,
            const ArgumentSpec& arg_spec, const std::shared_ptr<EnvironmentObject>& env);

        // Основной метод оценки пар
        Object eval_pair(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);

        // Таблицы форм
        std::unordered_map<
            void*,
            Object(Interpreter::*)(const Object&, Arguments&, const std::shared_ptr<EnvironmentObject>&)>
            builtin_forms;

        std::vector<std::pair<
            void*,
            std::function<Object(const Object&, Arguments&, const std::shared_ptr<EnvironmentObject>&)>>>
            m_custom_forms;

        std::vector<std::pair<void*,
            Object(Interpreter::*)(const Object& form,
                const Object& rest,
                const std::shared_ptr<EnvironmentObject>& env)>>
            special_forms;

        void init_special_forms(
            const std::unordered_map<std::string,
            Object(Interpreter::*)(const Object&,
                const Object&,
                const std::shared_ptr<EnvironmentObject>&)>&
            forms);

        void init_builtin_forms(
            const std::unordered_map<std::string,
            Object(Interpreter::*)(const Object&,
                Arguments&,
                const std::shared_ptr<EnvironmentObject>&)>&
            forms);

        // Для проверки типов
        std::unordered_map<std::string, ObjectType> string_to_type;

        void vararg_check(const Object& form,
            const Arguments& args,
            const std::vector<std::optional<ObjectType>>& unnamed,
            const std::unordered_map<std::string, std::pair<bool, std::optional<ObjectType>>>& named);
        
        

        // Состояние
        Reader  reader;
        Object  true_object;
        Object  false_object;
        int     gensym_id = 0;
        Object  global_environment;
        Object  comp_env;
        bool    disable_printing = false;
        int     stack_depth;
    };

    fmt::terminal_color string_to_color(const std::string& name);
} // namespace script