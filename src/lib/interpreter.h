#pragma once

#include "reader.h"
#include "object.h"
#include <iostream>
#include <functional>
#include <unordered_map>
#include <memory>

class Interpreter {
public:
    Interpreter();
    
    // Основные методы оценки
    Object eval(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    
    // Символы и окружение
    Object intern(const std::string& name);
    bool try_symbol_lookup(const Object& sym, const std::shared_ptr<EnvironmentObject>& env, Object* dest);
    Object eval_symbol(const Object& sym, const std::shared_ptr<EnvironmentObject>& env);
    
    // Вспомогательные методы
    Arguments get_args(const Object& form, const Object& rest, const ArgumentSpec& spec);
    void eval_args(Arguments* args, const std::shared_ptr<EnvironmentObject>& env);
    ArgumentSpec make_varargs();
    std::vector<Object> eval_list(const Object& list, const std::shared_ptr<EnvironmentObject>& env);
    bool truthy(const Object& o);
    void register_form(const std::string& name,
        const std::function<Object(const Object&, Arguments&,
            const std::shared_ptr<EnvironmentObject>&)>& form);

    // Обработка ошибок
    void throw_eval_error(const Object& o, const std::string& err);
    
    // REPL
    void execute_repl();
private:
    // Специальные формы
    Object eval_quote(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_define(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_lambda(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_begin(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_print(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cons(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_car(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cdr(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
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

    // Встроенные функции
    Object eval_plus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_minus(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_times(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_divide(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_print_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cons_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_car_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cdr_builtin(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_equals(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_lt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_gt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_leq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_geq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_null_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_pair_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_symbol_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_number_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_list_func(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_eq(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_gensym(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_eval(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_set_car(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_set_cdr(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_exit(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_read(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_load_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_append(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_substring(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_to_symbol(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_symbol_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_vector(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_vector_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_vector_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_vector_length(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_vector_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_make_hash_table(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_hash_table_set(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_hash_table_ref(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_hash_table_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
   
    Object eval_open_input_file(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_read_line(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_close_input_port(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_eof_object_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_file_exists_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_system(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    // Преобразования типов
    Object eval_number_to_string(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_string_to_number(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_char_to_integer(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_integer_to_char(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    // Дополнительные предикаты
    Object eval_char_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_procedure_p(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_eqv(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    // Математические функции
    Object eval_abs(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_max(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_min(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_expt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_sqrt(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

        // Системные утилиты
    Object eval_current_directory(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    // Quasiquote helpers
    Object quasiquote_helper(const Object& form, const std::shared_ptr<EnvironmentObject>& env);

    // Улучшенная обработка аргументов
    ArgumentSpec parse_arg_spec(const Object& form, Object& rest);
    void set_args_in_env(const Object& form, const Arguments& args,
        const ArgumentSpec& arg_spec, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_get_environment_variable(const Object& form, Arguments& args,
        const std::shared_ptr<EnvironmentObject>& env);
    // Конвертация чисел
    int64_t number_to_integer(const Object& obj);
    double number_to_float(const Object& obj);
    template<typename T> T number(const Object& obj);

private:
    // Основной метод оценки пар
    Object eval_pair(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    
    // Таблицы форм
    std::unordered_map<std::string, 
        Object (Interpreter::*)(const Object&, const Object&, const std::shared_ptr<EnvironmentObject>&)> special_forms;
    
    std::unordered_map<std::string,
        Object (Interpreter::*)(const Object&, Arguments&, const std::shared_ptr<EnvironmentObject>&)> builtin_forms;
    
    // Custom forms поддержка
    std::vector<std::pair<void*, std::function<Object(const Object&, Arguments&,
        const std::shared_ptr<EnvironmentObject>&)>>> m_custom_forms;

    // Глобальные переменные
    std::unordered_map<SymbolObject*, Object> global_vars;
    
    // Состояние
    Reader reader;
    bool want_exit = false;

    Object m_true_object;
    Object m_false_object;
    int gensym_id = 0;


};