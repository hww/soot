#include "common/sootc/Env/Env.hpp"
#include "common/sootc/Env/GlobalEnv.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/SymbolMacroEnv.hpp"
#include "common/sootc/Env/MacroExpandEnv.hpp"
#include "common/sootc/Env/TypeEnv.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/MethodEnv.hpp"
#include "common/sootc/Env/StateEnv.hpp"

namespace sootc {
    Env::Env(EnvKind kind, Env* parent) 
        : m_kind(kind), m_parent(parent) {
        // Кэшируем важные Env
        if (parent) {
            m_global_env = parent->m_global_env;
            m_file_env = parent->m_file_env;
            m_function_env = parent->m_function_env;
            m_type_env = parent->m_type_env;
            m_symbol_macro_env = parent->m_symbol_macro_env;
            m_macro_expand_env = parent->m_macro_expand_env;
        }
        
        switch (kind) {
            case EnvKind::FILE_ENV: m_file_env = static_cast<FileEnv*>(this); break;
            case EnvKind::GLOBAL_ENV: m_global_env = static_cast<GlobalEnv*>(this); break;
            case EnvKind::FUNCTION_ENV: m_function_env = static_cast<FunctionEnv*>(this); break;
            case EnvKind::TYPE_ENV: m_type_env = static_cast<TypeEnv*>(this); break;
            case EnvKind::SYMBOL_MACRO_ENV: m_symbol_macro_env = static_cast<SymbolMacroEnv*>(this); break;
            case EnvKind::MACRO_EXPAND_ENV: m_macro_expand_env = static_cast<MacroExpandEnv*>(this); break;            
            default: break;
        }
    }

} // namespace sootc


