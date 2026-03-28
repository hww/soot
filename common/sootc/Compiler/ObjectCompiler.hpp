#pragma once
#include "common/sooti/Object.hpp"
#include "common/sootc/Compiler/FunctionCompiler.hpp"
#include "common/type_system/TypeSystem.hpp"
#include <memory>

namespace sootc {

class ObjectCompiler {
public:
    ObjectCompiler(TypeSystem& ts);
    
    // Компилируем один S-выражение в функцию
    RelocatableBuffer  compile_function(const script::Object& form, const std::string& func_name);
    
    // Компилируем файл (список форм)
    RelocatableBuffer  compile_file(const script::Object& forms, const std::string& module_name);
    
private:
    TypeSystem& ts_;
    
    // Рекурсивная компиляция формы
    IR_Reg* compile_form(const script::Object& form, FunctionCompiler& compiler);
    
    // Специализированные компиляторы для разных форм
    IR_Reg* compile_define(const script::Object& form, FunctionCompiler& compiler);
    IR_Reg* compile_lambda(const script::Object& form, FunctionCompiler& compiler);
    IR_Reg* compile_if(const script::Object& form, FunctionCompiler& compiler);
    IR_Reg* compile_begin(const script::Object& form, FunctionCompiler& compiler);
    
    // Компиляция вызова функции
    IR_Reg* compile_call(const script::Object& form, FunctionCompiler& compiler);
    
    // Компиляция примитивных операций
    IR_Reg* compile_binary_op(const script::Object& form, FunctionCompiler& compiler, IR_Binary::Op op);
    
    // Таблица символов для локальных переменных
    std::unordered_map<std::string, IR_Reg*> local_vars_;
    
    // Создание временного регистра
    IR_Reg* create_temp_reg(Type* type, FunctionCompiler& compiler);
};

} // namespace sootc