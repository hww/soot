#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "sootc/Env/Export.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "common/carbon/files/BinaryFileBuilder.hpp"
#include <functional>
#include <unordered_map>
#include <string>

namespace sootc {

class Compiler {
public:
    Compiler(TypeSystem& ts, std::string module_name);
    
    // Точка входа для файла
    std::shared_ptr<carbon::modules::Module> compile_module(const script::Object& forms, FileEnv* env);

    // Единый метод компиляции (один проход)
    IR_Value* compile(const script::Object& form, Env* env);
                            
    TypeSystem& ts() { return ts_; }

    // Для доступа к текущему env при ошибках
    Env* current_env() { return m_current_env; }
    void set_current_env(Env* env) { m_current_env = env; }
    
private:
    TypeSystem& ts_;
    carbon::files::BinaryFileBuilder builder_;
    Env* m_current_env = nullptr;
    
    using FormHandler = std::function<IR_Value*(Compiler*, const script::Object&, const script::Object&, Env*)>;
    std::unordered_map<std::string, FormHandler> m_forms;
    void setup_forms();

    // Обработчики форм
    IR_Value* compile_define(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_lambda(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_begin(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_defmethod(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_deftype(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_defstate(const script::Object& form, const script::Object& rest, Env* env);

    // Мультимодульность
    IR_Value* compile_require(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_define_extern(const script::Object& form, const script::Object& rest, Env* env);
    
    // Арифметика
    IR_Value* compile_add(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_sub(const script::Object& form, const script::Object& rest, Env* env);
    
    // Атомы и вызовы
    IR_Value* compile_number(const script::Object& form, Env* env);
    IR_Value* compile_symbol(const script::Object& form, Env* env);
    IR_Value* compile_call(const script::Object& form, Env* env);

    // Парсинг typespec
    TypeSpec parse_typespec(const script::Object& form, Env* env);    

    // пустой результат
    IR_Value* get_none() { return new IR_None(); }

    std::unordered_map<std::string, std::shared_ptr<Module>> m_loaded_modules;
    
};

} // namespace sootc