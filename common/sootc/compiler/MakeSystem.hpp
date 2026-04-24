#pragma  once

#include "repl/config.h"
#include "soot/Interpreter.hpp"
class MakeSystem {
    public:

    MakeSystem(const std::optional<REPL::Config>& repl_config, const std::string& user_profile) {
        // Реализация конструктора
        // Пока можно оставить пустым или с минимальной инициализацией
        (void)repl_config;
        (void)user_profile;
    }

    std::vector<std::string> get_loaded_projects() const { return m_loaded_projects; }
    
    
    //soot::Interpreter m_soot;
    std::optional<REPL::Config> m_repl_config;
    std::vector<std::string> m_loaded_projects;
};
