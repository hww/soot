#pragma  once

#include "repl/config.h"
#include "script/Interpreter.hpp"
class MakeSystem {
    public:

    MakeSystem(const std::optional<repl::Config>& repl_config, const std::string& user_profile) : m_soot() {
        // Реализация конструктора
        // Пока можно оставить пустым или с минимальной инициализацией
        (void)repl_config;
        (void)user_profile;
    }

    std::vector<std::string> get_loaded_projects() const { return m_loaded_projects; }
    
    
    script::Interpreter m_soot;
    std::optional<repl::Config> m_repl_config;
    std::vector<std::string> m_loaded_projects;
};
