#pragma once

#include "common/sootc/Env/Env.hpp"

namespace sootc {

/*  
    // В компиляторе
    script::Object Compiler::macroexpand(const script::Object& form, Env* env) {
        if (!is_macro_call(form, env)) {
            return form;
        }
        
        auto macro_name = get_macro_name(form);  // script::InternedSymbolPtr
        
        // Проверка на рекурсию
        if (env->macro_expand_env() && 
            env->macro_expand_env()->is_expanding(macro_name)) {
            throw std::runtime_error("Macro expansion loop: " + macro_name->name);
        }
        
        auto macro_body = get_macro_body(macro_name, env);
        
        // Создаем окружение для раскрытия
        auto* macro_env = new MacroExpandEnv(env, macro_name, macro_body, form);
        
        // Раскрываем макрос
        auto expanded = expand_macro_body(macro_body, macro_env);
        
        // Продолжаем раскрытие (вложенные макросы)
        return macroexpand(expanded, macro_env);
    }
*/

/*!
 * An Env which manages the scope for (declare ...) statements.
 */
class MacroExpandEnv : public Env {
public:
    MacroExpandEnv(Env* parent,
                   const script::InternedSymbolPtr& macro_name,
                   const script::Object& macro_body,
                   const script::Object& macro_use)
        : Env(EnvKind::MACRO_EXPAND_ENV, parent),
          m_macro_name(macro_name),
          m_macro_body(macro_body),
          m_macro_use_location(macro_use) {
        
        // Находим корневую форму (для вложенных макросов)
        MacroExpandEnv* parent_macro = nullptr;
        if (parent) {
            parent_macro = parent->macro_expand_env();
        }
        if (parent_macro) {
            m_root_form = parent_macro->m_root_form;
        } else {
            m_root_form = m_macro_use_location;
        }
    }
    
    std::string print() const override {
        return fmt::format("MacroExpandEnv(macro={})", 
                           m_macro_name ? m_macro_name.c_str() : "unknown");
    }
    
    // Геттеры
    const script::InternedSymbolPtr& macro_name() const { return m_macro_name; }
    const script::Object& macro_body() const { return m_macro_body; }
    const script::Object& macro_use_location() const { return m_macro_use_location; }
    const script::Object& root_form() const { return m_root_form; }
    
    // Проверка, не зациклился ли макрос
    bool is_expanding(const script::InternedSymbolPtr& sym) const {
        if (m_macro_name == sym) return true;
        auto* parent_macro = parent() ? parent()->macro_expand_env() : nullptr;
        return parent_macro ? parent_macro->is_expanding(sym) : false;
    }
    
    // Проверка по имени (если нужно)
    bool is_expanding(const std::string& name) const {
        if (m_macro_name && m_macro_name  == name) return true;
        auto* parent_macro = parent() ? parent()->macro_expand_env() : nullptr;
        return parent_macro ? parent_macro->is_expanding(name) : false;
    }
    
private:
    script::InternedSymbolPtr m_macro_name;
    script::Object m_macro_body;
    script::Object m_macro_use_location;
    script::Object m_root_form;
};

} // namespace sootc