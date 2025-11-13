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
    
    // Основные методы
    Object eval(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_with_rewind(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    
    // REPL
    void execute_repl();
    
    Reader& get_reader() { return reader; }

private:
    // Типы функций как в OpenGOAL
    using SpecialForm = Object (Interpreter::*)(const Object&, const Object&, 
                                               const std::shared_ptr<EnvironmentObject>&);
    using BuiltinForm = Object (Interpreter::*)(const Object&, Arguments&,
                                               const std::shared_ptr<EnvironmentObject>&);

    // Таблицы
    std::unordered_map<std::string, SpecialForm> special_forms;
    std::unordered_map<std::string, BuiltinForm> builtin_forms;

    Reader reader;
    bool want_exit = false;
    
    // Глобальные переменные
    std::unordered_map<SymbolObject*, Object> global_vars;
    
    // Вспомогательные методы
    bool try_symbol_lookup(const Object& sym, const std::shared_ptr<EnvironmentObject>& env, Object* dest);
    Object intern(const std::string& name);
    
    // Аргументы
    Arguments get_args(const Object& form, const Object& rest, const ArgumentSpec& spec);
    void eval_args(Arguments* args, const std::shared_ptr<EnvironmentObject>& env);
    ArgumentSpec make_varargs();
    std::vector<Object> eval_list(const Object& list, const std::shared_ptr<EnvironmentObject>& env);
    
    // Специальные формы
    Object eval_quote(const Object& form, const Object& rest, 
                     const std::shared_ptr<EnvironmentObject>& env);
    Object eval_define(const Object& form, const Object& rest,
                      const std::shared_ptr<EnvironmentObject>& env);
    Object eval_lambda(const Object& form, const Object& rest,
                      const std::shared_ptr<EnvironmentObject>& env);
    Object eval_begin(const Object& form, const Object& rest,
                     const std::shared_ptr<EnvironmentObject>& env);
    Object eval_print(const Object& form, const Object& rest,
                     const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cons(const Object& form, const Object& rest,
                    const std::shared_ptr<EnvironmentObject>& env);
    Object eval_car(const Object& form, const Object& rest,
                   const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cdr(const Object& form, const Object& rest,
                   const std::shared_ptr<EnvironmentObject>& env);

    // Встроенные функции  
    Object eval_plus(const Object& form, Arguments& args,
                    const std::shared_ptr<EnvironmentObject>& env);
    Object eval_minus(const Object& form, Arguments& args,
                     const std::shared_ptr<EnvironmentObject>& env);
    Object eval_times(const Object& form, Arguments& args,
                     const std::shared_ptr<EnvironmentObject>& env);
    Object eval_divide(const Object& form, Arguments& args,
                            const std::shared_ptr<EnvironmentObject>& env);
    Object eval_print_builtin(const Object& form, Arguments& args,
                             const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cons_builtin(const Object& form, Arguments& args,
                            const std::shared_ptr<EnvironmentObject>& env);
    Object eval_car_builtin(const Object& form, Arguments& args,
                           const std::shared_ptr<EnvironmentObject>& env);
    Object eval_cdr_builtin(const Object& form, Arguments& args,
                           const std::shared_ptr<EnvironmentObject>& env);

    // Базовые методы оценки
    Object eval_symbol(const Object& sym, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_pair(const Object& obj, const std::shared_ptr<EnvironmentObject>& env);
    
    // Обработка ошибок
    void throw_eval_error(const Object& o, const std::string& err);
};