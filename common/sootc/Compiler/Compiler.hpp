#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "sootc/Env/Export.hpp"
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

    // ФАЗА 1: DECLARE — создает IR_Value
    IR_Value* declare(const script::Object& form, Env* env);
    
    // ФАЗА 2: RESOLVE — наполняет IR_Value логикой (создает IR_Node)
    IR_Value* resolve(const script::Object& form, Env* env);
    
    // ФАЗА 3: BUILD — генерирует байткод
    void add_definition(const std::string& name, const std::string& type, 
                        carbon::files::RelocatableBuffer buffer, 
                        carbon::files::SymbolFlags flags = carbon::files::SymbolFlags::Export);
    
    // Удобный метод для компиляции (выбирает фазу в зависимости от контекста)
    IR_Value* compile(const script::Object& form, Env* env);
                            
    TypeSystem& ts() { return ts_; }

    TypeSpec build_typespec_from_env(FunctionEnv* env);
    
    void error_in_macro(const script::Object& form, const std::string& msg);
    
    // Для доступа к текущему env при ошибках
    Env* current_env() { return m_current_env; }
    void set_current_env(Env* env) { m_current_env = env; }
    
private:
    TypeSystem& ts_;
    carbon::files::BinaryFileBuilder builder_;
    Env* m_current_env = nullptr;
    
    // Таблица диспетчеризации (теперь возвращает IR_Value*)
    using FormHandler = std::function<IR_Value*(Compiler*, const script::Object&, const script::Object&, Env*)>;
    std::unordered_map<std::string, FormHandler> m_forms;
    void setup_forms();

    // Обработчики спецформ для DECLARE фазы
    IR_Value* compile_define(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_lambda(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_if(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_set(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_begin(const script::Object& form, const script::Object& rest, Env* env);
    
    // Обработчики для RESOLVE фазы
    IR_Value* resolve_define(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* resolve_lambda(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* resolve_begin(const script::Object& form, const script::Object& rest, Env* env);
    
    // Арифметика (через IR_Binary)
    IR_Value* compile_add(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_sub(const script::Object& form, const script::Object& rest, Env* env);
    
    // Атомы и вызовы
    IR_Value* compile_number(const script::Object& form, Env* env);
    IR_Value* compile_symbol(const script::Object& form, Env* env);
    IR_Value* compile_call(const script::Object& form, Env* env);
    IR_Value* resolve_call(const script::Object& form, Env* env);

    IR_Value* compile_defmethod(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_deftype(const script::Object& form, const script::Object& rest, Env* env);
    
    IR_Value* resolve_defmethod(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* resolve_deftype(const script::Object& form, const script::Object& rest, Env* env);

    // Хелпер для финальной сборки FunctionEnv в байткод
    RelocatableBuffer finalize_function(FunctionEnv* fe);
};

} // namespace sootc