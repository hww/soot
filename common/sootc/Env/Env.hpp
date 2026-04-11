#pragma once

#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/IR/IR_Node.hpp"  
#include "common/sootc/Env/Label.hpp" 
#include "common/type_system/TypeSpec.hpp"
#include <string>

namespace sootc {


enum class EnvKind {
    FILE_ENV,
    FUNCTION_ENV,
    SYMBOL_MACRO_ENV, 
    MACRO_EXPAND_ENV,
    GLOBAL_ENV,
    METHOD_ENV,
    STATE_ENV,
    TYPE_ENV,
    BLOCK_ENV,
    LABEL_ENV,
    LEXICAL_ENV,
    OTHER_ENV,
};

class IR_Node;
class IR_Method;
class FileEnv;
class GlobalEnv;
class BlockEnv;
class FunctionEnv;
class SymbolMacroEnv;
class MacroExpandEnv;
class MethodEnv;
class StateEnv;
class TypeEnv;
struct IRegConstraint;

// ===========================================================
// Базовый класс окружения 
// ===========================================================

class Env {
public:

    Env(EnvKind kind, Env* parent = nullptr);
    
    virtual ~Env() = default;

    // ========================================================================
    // Виртуальные методы для регистров и лексического поиска
    // ========================================================================
    
    virtual IR_Reg* make_ireg(const TypeSpec& ts, RegClass reg_class) {
        (void)ts; (void)reg_class;
        throw std::runtime_error("make_ireg not implemented in this Env");
    }
    
    virtual void constrain_reg(const IRegConstraint& constraint) {
        (void)constraint;
        throw std::runtime_error("constrain_reg not implemented in this Env");
    }
    
    virtual IR_Value* lexical_lookup(const script::Object& sym) {
        (void)sym;
        return nullptr;
    }
    
    virtual BlockEnv* find_block(const std::string& name) {
        (void)name;
        return nullptr;
    }
    
    virtual std::unordered_map<std::string, Label>& get_label_map() {
        static std::unordered_map<std::string, Label> empty;
        return empty;
    }
    
    virtual const std::vector<IR_Value*>& symbols() const {
        static const std::vector<IR_Value*> empty;
        return empty;
    }

    // Собираем все скомпилированные IR_Value
    std::vector<std::pair<std::string, IR_Value*>> sybols_table() const {
        std::vector<std::pair<std::string, IR_Value*>> table;
        for (auto& value : symbols()) {
            auto name = get_value_name(value);
            table.push_back({name, value});
        }
        return table;
    }

    // ========================================================================
    // Основные операции
    // ========================================================================

    virtual IR_Value* lookup(const std::string& name) { 
        return m_parent ? m_parent->lookup(name) : nullptr; 
    }
    
    virtual std::string get_value_name(IR_Value* value)  const {
        (void)value;
        return "<unknown>";
    }

    virtual void bind(const std::string& name, IR_Value* val) {
        if (m_parent) m_parent->bind(name, val);
        else throw std::runtime_error("No place to bind " + name);
    }
    
    virtual void emit(const script::Object& form, std::unique_ptr<IR_Node> node) {
        if (m_parent) m_parent->emit(form, std::move(node));
    }
    
    // Быстрые геттеры (без dynamic_cast!)
    GlobalEnv* global_env() const { return m_global_env; }
    FunctionEnv* function_env() const { return m_function_env; }
    TypeEnv* type_env() const { return m_type_env; }
    FileEnv* file_env() const { return m_file_env; }
    SymbolMacroEnv* symbol_macro_env() { return m_symbol_macro_env; }
    MacroExpandEnv* macro_expand_env() { return m_macro_expand_env; }

    Env* parent() const { return m_parent; }
    EnvKind kind() const { return m_kind; }
    
    // Для отладки
    virtual std::string print() const = 0;
    
protected:
    EnvKind m_kind;
    Env* m_parent = nullptr;
    
    // Кэш (наследуется и обновляется)
    GlobalEnv* m_global_env = nullptr;
    FunctionEnv* m_function_env = nullptr;
    TypeEnv* m_type_env = nullptr;
    FileEnv* m_file_env = nullptr;
    SymbolMacroEnv* m_symbol_macro_env = nullptr;
    MacroExpandEnv* m_macro_expand_env = nullptr;
};

} // namespace sootc