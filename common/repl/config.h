#pragma once

#include "keybinds.h"
#include <string>
#include <vector>
#include <unordered_map>

class Config {
public:
    // Основные настройки
    int nrepl_port = 8181;
    std::string project_name = "aleste";
    std::vector<std::string> search_paths = {};

    // REPL настройки
    bool save_history = true;
    size_t history_size = 1000;
    bool colors_enabled = true;
    std::string prompt = "aleste> ";

    // Клавиши
    bool append_keybinds = true;
    std::vector<KeyBind> keybinds = {
        {KeyBind::Modifier::CTRL, "C", "Clear screen", "(clear)"},
        {KeyBind::Modifier::CTRL, "H", "Show help", "(help)"},
        {KeyBind::Modifier::CTRL, "K", "Show keybinds", "(keybinds)"},
        {KeyBind::Modifier::CTRL, "D", "Exit REPL", "(quit)"},
        {KeyBind::Modifier::CTRL, "L", "List history", "(history)"}
    };

    // Сеть
    bool enable_network = false;
    int network_port = 8181;

    // Методы
    bool load_from_file(const std::string& filename);
    bool save_to_file(const std::string& filename);
    void set_defaults();

private:
    std::unordered_map<std::string, std::string> custom_settings;
};