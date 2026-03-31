#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "sootc/Compiler/Env.hpp"
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
    std::shared_ptr<carbon::modules::Module> compile_module(const script::Object& forms, Env* env);

    // СЕРДЦЕ: Рекурсивная компиляция формы в IR-значение
    IR_Value* declare(const script::Object& form, Env* env);
    
    // Регистрация в бинарном билдере
    void add_definition(const std::string& name, const std::string& type, 
                        carbon::files::RelocatableBuffer buffer, 
                        carbon::files::SymbolFlags flags = carbon::files::SymbolFlags::Export);
    
    TypeSystem& ts() { return ts_; }

private:
    TypeSystem& ts_;
    carbon::files::BinaryFileBuilder builder_;
    
    // Таблица диспетчеризации (теперь возвращает IR_Value*)
    using FormHandler = std::function<IR_Value*(Compiler*, const script::Object&, const script::Object&, Env*)>;
    std::unordered_map<std::string, FormHandler> m_forms;
    void setup_forms();

    // Обработчики спецформ
    IR_Value* compile_define(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_lambda(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_if(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_set(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_begin(const script::Object& form, const script::Object& rest, Env* env);
    
    // Арифметика (через IR_Binary)
    IR_Value* compile_add(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_sub(const script::Object& form, const script::Object& rest, Env* env);
    
    // Атомы и вызовы
    IR_Value* compile_number(const script::Object& form, Env* env);
    IR_Value* compile_symbol(const script::Object& form, Env* env);
    IR_Value* compile_call(const script::Object& form, Env* env);

    IR_Value* compile_defmethod(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_deftype(const script::Object& form, const script::Object& rest, Env* env);

    // Хелпер для финальной сборки FunctionEnv в байткод
    RelocatableBuffer finalize_function(FunctionEnv* fe);
};

} // namespace sootc