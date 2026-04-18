#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/sootc/IR/StaticObject.hpp"  
#include "sootc/IR/IR_FunctionValue.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace sootc {

class FunctionEnv;
class StaticObject;
class IR_Value;

/*!
 * An Env for an entire file (or input to the REPL)
 */
class FileEnv : public Env {
public:
    // Конструктор
    FileEnv(Env* parent, const std::string& name);
    
    // Env interface
    std::string print() const override;
    IR_Value* lookup(const std::string& name) override;
    void bind(const std::string& name, IR_Value* val) override;
    std::string get_value_name(IR_Value* value) const override;
    const std::vector<IR_Value*>& symbols() const override;
    
    // Импорты
    void add_import(FileEnv* imported);
    const std::vector<FileEnv*>& imports() const;
    bool has_import(const std::string& name) const;
    
    // Поиск функций (для resolve)
    IR_FunctionValue* find_function(const std::string& name) const;
    
    // Управление функциями
    void add_function(IR_FunctionValue* fn);
    const std::vector<IR_FunctionValue*>& functions() const { return m_functions; }
    
    // Геттеры
    const std::string& name() const { return m_name; }
    const std::unordered_map<std::string, IR_Value*>& symbols_map() const { return m_symbols; }
    
    // Для совместимости с существующим кодом
    const std::vector<std::unique_ptr<FunctionEnv>>& functions_old() const { return m_functions_old; }
    void add_function_old(std::unique_ptr<FunctionEnv> fe);
    
private:
    std::string m_name;
    std::unordered_map<std::string, IR_Value*> m_symbols;
    std::vector<IR_Value*> m_ordered_symbols;
    std::vector<FileEnv*> m_imports;
    std::vector<IR_FunctionValue*> m_functions;        // новые функции
    std::vector<std::unique_ptr<FunctionEnv>> m_functions_old; // для совместимости
};

} // namespace sootc