#include "config.h"
#include "third_party/fmt/include/fmt/format.h"
#include <fstream>
#include <iostream>

bool Config::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        fmt::print("Config file not found, using defaults\n");
        set_defaults();
        return false;
    }

    // TODO: JSON парсинг
    fmt::print("Loading config from {}\n", filename);
    return true;
}

bool Config::save_to_file(const std::string& filename) {
    std::ofstream file(filename);
    if (!file) return false;

    // TODO: JSON сериализация
    fmt::print("Saving config to {}\n", filename);
    return true;
}

void Config::set_defaults() {
    nrepl_port = 8181;
    project_name = "aleste";
    save_history = true;
    history_size = 1000;
    colors_enabled = true;
    enable_network = false;
    network_port = 8181;
}