#pragma once

#include "common/sootc/Env/Env.hpp"

namespace sootc {

/*!
 * An Env for managing symbol macros.
 */
class SymbolMacroEnv : public Env {
public:
    explicit SymbolMacroEnv(Env* parent) 
        : Env(EnvKind::SYMBOL_MACRO_ENV, parent) {}
    
    void add_macro(const std::string& name, const script::Object& form) {
        m_macros[name] = form;
    }
    
    script::Object* lookup_macro(const std::string& name) {
        auto it = m_macros.find(name);
        if (it != m_macros.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    std::string print() const override {
        return fmt::format("SymbolMacroEnv(macros={})", m_macros.size());
    }
    
private:
    std::unordered_map<std::string, script::Object> m_macros;
};

} // namespace sootc