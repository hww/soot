#pragma once
#include <string>

struct Config {
    // REPL настройки
    std::string prompt = "aleste> ";
    std::string username = "aleste-user";
    bool colors_enabled = true;
    
    // История
    std::string history_file = ".aleste_history";
    size_t max_history_size = 1000;
    
    // Сеть
    bool enable_network = false;
    int network_port = 8181;
    
    // Загрузка/сохранение (пока заглушки)
    void load() { /* TODO: загрузка из файла */ }
    void save() { /* TODO: сохранение в файл */ }
};