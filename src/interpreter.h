#pragma once

#include "reader.h"
#include "object.h"
#include <iostream>
#include <functional>
#include <unordered_map>

class Interpreter {
public:
    Interpreter();
    
    // Основные методы
    Object eval(const Object& obj);
    Object eval_with_rewind(const Object& obj);
    
    // REPL
    void execute_repl();
    
    Reader& get_reader() { return reader; }

private:
    Reader reader;
    bool want_exit = false;
    
    // Очень простые формы
    Object eval_symbol(const Object& sym);
    Object eval_pair(const Object& obj);
    
    // Вспомогательные
    void throw_eval_error(const Object& o, const std::string& err);
    
    // Базовые формы
    Object eval_quote(const Object& form, const Object& rest);
    Object eval_define(const Object& form, const Object& rest);
    Object eval_begin(const Object& form, const Object& rest);
    Object eval_print(const Object& form, const Object& rest);
    Object eval_cons(const Object& form, const Object& rest);
    Object eval_car(const Object& form, const Object& rest);
    Object eval_cdr(const Object& form, const Object& rest);
    Object eval_plus(const Object& form, const Object& rest);
    
    // Простая среда (глобальные переменные)
    std::unordered_map<SymbolObject*, Object> global_vars;
    
    Object intern(const std::string& name) {
        return reader.get_symbol_table().intern(name);
    }
    
    bool try_symbol_lookup(const Object& sym, Object* dest);

    Object apply_function(const Object& function, const std::vector<Object>& args);
    Object eval_application(const Object& function, const Object& args, const Object& form);
    std::vector<Object> eval_list(const Object& list);
    Object eval_builtin(const std::string& name, const std::vector<Object>& args, const Object& form);
};