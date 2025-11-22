#pragma once

#include "reader.h"
#include "object.h"
#include <iostream>
#include <functional>
#include <unordered_map>
#include <memory>

namespace script
{
    class Interpreter {
    public:
        Interpreter(const std::string& username = "user", bool load_libs = false);

        void load_library();

        // Основные методы оценки
        Object eval(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_string(const std::string& expression, const std::string& filename);

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
        Object eval_list_return_last(const Object& form,
            Object rest,
            const std::shared_ptr<EnvironmentObject>& env);

        // Обработка ошибок
        void throw_eval_error(const Object& o, const std::string& err);

        // REPL
        void execute_repl();

        // Доступ к ридеру
        Reader& get_reader() { return reader; }

        // Лоступ к окружению
        Object get_global_environmet() { return global_environment; }

        // Boolean helpers (используют символы)
        Object make_bool(bool value) { return value ? m_true_object : m_false_object; }
        bool is_true(const Object& o) const { return !is_false(o); }
        bool is_false(const Object& o) const { return o.is_symbol() && o.as_symbol().name_ptr == m_false_object.as_symbol().name_ptr; }
        bool is_bool(const Object& o) const { return o.is_symbol() && (o.as_symbol().name_ptr == m_false_object.as_symbol().name_ptr || o.as_symbol().name_ptr == m_true_object.as_symbol().name_ptr); }
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
        Object eval_type(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

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

        // Хэш-таблицы
        Object eval_make_hash_table(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_hash_table_try_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Системные и ввод-вывод
        Object eval_print(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_pprint(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_inspect(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_error(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_format(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);


        // файлы
        Object eval_read(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_load_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_file_exists_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_read_data_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_try_load_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Система
        Object eval_system(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_get_env(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_current_directory(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
        Object eval_exit(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

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

        // Quasiquote helpers
        Object quasiquote_helper(const Object& form, const std::shared_ptr<EnvironmentObject>& env);

        // Улучшенная обработка аргументов
        ArgumentSpec parse_arg_spec(const Object& form, Object& rest);
        void set_args_in_env(const Object& form, const Arguments& args,
            const ArgumentSpec& arg_spec, const std::shared_ptr<EnvironmentObject>& env);

        std::string read_entire_file(const std::string& filename);

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
        Reader reader;
        bool want_exit = false;
        Object m_true_object;
        Object m_false_object;
        int gensym_id = 0;
        Object global_environment;
        Object comp_env;
        bool disable_printing = false;
    };
} // namespace script