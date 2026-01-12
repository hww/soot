#include "common/script/Export.hpp"
#include "third_party/fmt/include/fmt/format.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include "fmt/format.h"
#include "fmt/color.h"
#include "common/util/Log.hpp"
namespace fs = std::filesystem;

script::Interpreter soot("xiff");

std::string get_config_dir() {
#ifdef _WIN32
    return std::string(std::getenv("APPDATA")) + "/soot";
#else
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) return std::string(xdg_config) + "/soot";
    return std::string(std::getenv("HOME")) + "/.config/soot";
#endif
}

std::string get_cache_dir() {
#ifdef _WIN32
    return get_config_dir() + "/cache";
#else
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    if (xdg_cache) return std::string(xdg_cache) + "/soot";
    return std::string(std::getenv("HOME")) + "/.cache/soot";
#endif
}


// Универсальный поиск: Проект -> Пользователь -> Система
std::string find_file(const std::string& name) {
    // 1. Текущая папка проекта
    if (fs::exists(name)) return name;

    // 2. Домашняя папка пользователя
    fs::path user_path = fs::path(get_config_dir()) / name;
    if (fs::exists(user_path)) return user_path.string();

    // 3. Глобальная папка (установленная через make install)
    fs::path sys_path = fs::path("/usr/local/share/soot") / name;
    if (fs::exists(sys_path)) return sys_path.string();

    return ""; 
}

void execute_line(const std::string& line) {
    // Выполнение Lisp кода (только если не built-in команда)
    try {
        auto result = soot.eval_string(line,"xiff");
        fmt::print(fg(fmt::color::green), "=> {}\n", result.print());
    }
    catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
    }
}

void execute_startup_commands(const std::vector<std::string>& commands) {
    for (const auto& command : commands) {
        try {
            lg::debug("Executing startup command: {}", command);
            execute_line(command);
        }
        catch (const std::exception& e) {
            lg::warn("Startup command failed: {} - Error: {}", command, e.what());
        }
    }
}

// Вспомогательный метод для чтения и выполнения
void load_and_execute(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    std::vector<std::string> commands;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != ';') {
            commands.push_back(line);
        }
    }
    execute_startup_commands(commands);
}

void load_startup_files() {
    std::string lib_path = find_file("lib.sot");
    if (!lib_path.empty()) {
        fmt::print(fg(fmt::color::cyan), "Loading file {}\n", lib_path.c_str());
        execute_line(fmt::format("(load-file \"{}\")", lib_path));
    }
}

void print_xiff_usage() {
    fmt::print(fg(fmt::color::cyan), "XIFF (eXternal Interface Function Fabric) - SOOT Edition\n\n");
    fmt::print("Usage:\n");
    fmt::print("  xiff [options] <asm_files...>\n\n");
    fmt::print("Options:\n");
    fmt::print("  -o, --output <file>    {} Output header/config file\n", "→");
    fmt::print("  -l, --lib <file>       {} Load custom SOOT library for parsing\n", "→");
    fmt::print("  -v, --verbose          {} Show debug info during scan\n", "→");
    fmt::print("  -h, --help             {} Show this help\n", "→");
    fmt::print("\nExample:\n");
    fmt::print("  xiff -o interface.h src/*.asm\n");
}



int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_xiff_usage();
        return 0;
    }
    

    std::vector<std::string> input_files;
    std::string output_file = "output.h";
    std::string xiff_lib = "xiff-core.sot";
    bool verbose = false;

    // 1. Парсинг аргументов
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_xiff_usage();
            return 0;
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) output_file = argv[++i];
        } else if (arg == "--lib" || arg == "-l") {
            if (i + 1 < argc) xiff_lib = argv[++i];
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg.starts_with("-")) {
            fmt::print(fg(fmt::color::red), "Unknown option: {}\n", arg);
            return 1;
        } else {
            input_files.push_back(arg);
        }
    }

    try {
        // 2. Инициализируем SOOT Runtime (без интерактивного режима)
        load_startup_files();

        // Загружаем ядро XIFF (наш Lisp-обработчик)
        std::string lib_path = find_file(xiff_lib);
        if (!lib_path.empty()) {
            if (verbose) fmt::print("Loading XIFF core: {}\n", lib_path);
            auto expression = fmt::format("(load-file \"{}\")", lib_path);
            soot.eval_string(expression, lib_path);
        }

        // 3. Сканирование файлов
        for (const auto& path : input_files) {
            if (verbose) fmt::print("Scanning: {}\n", path);
            
            std::ifstream file(path);
            std::string line;
            std::string soot_buffer;
            
            while (std::getline(file, line)) {
                // Ищем маркеры в комментариях
                size_t pos = line.find("; xiff");
                if (pos == std::string::npos) pos = line.find("; soot");

                if (pos != std::string::npos) {
                    soot_buffer += line.substr(pos + 6) + " ";
                } else if (!soot_buffer.empty()) {
                    // Конец блока аннотаций - отправляем в SOOT
                    soot.eval_string(soot_buffer, lib_path);
                    soot_buffer.clear();
                }
            }
        }

        // 4. Генерация результата
        fmt::print(fg(fmt::color::green), "Processing complete. Generating output to {}...\n", output_file);
        
        // Передаем управление в SOOT для финальной сборки данных
        // Мы можем либо забрать данные в C++, либо вызвать функцию генерации в Lisp
        auto out_expression = fmt::format("(generate-output \"{}\")", "XiffMain");
        soot.eval_string(out_expression, output_file);

    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "XIFF Fatal Error: {}\n", e.what());
        return 1;
    }

    return 0;
}