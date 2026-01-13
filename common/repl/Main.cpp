#include "common/repl/ReplWrapper.hpp"
#include "third_party/fmt/include/fmt/format.h"
#include <iostream>

void print_usage() {
    fmt::print("SOOT Lisp - REPL Environment\n\n");
    fmt::print("Usage:\n");
    fmt::print("  soot                    {} Interactive REPL\n", "→");
    fmt::print("  soot --network [port]   {} Network REPL server\n", "→");
    fmt::print("  soot --script <file>    {} Execute script file\n", "→");
    fmt::print("  soot --help             {} Show this help\n", "→");
    fmt::print("\nExamples:\n");
    fmt::print("  soot                    {} Start interactive mode\n", "→");
    fmt::print("  soot --network 8181     {} Start network server\n", "→");
    fmt::print("  soot --script demo.lisp {} Run script\n", "→");
}

int main(int argc, char* argv[]) {
    // Парсим аргументы командной строки
    std::string script_file;
    std::string project_folder;
    bool use_network = false;
    int network_port = 8181;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--network" || arg == "-n") {
            use_network = true;
            if (i + 1 < argc) {
                try {
                    network_port = std::stoi(argv[++i]);
                }
                catch (...) {
                    fmt::print(fg(fmt::color::red), "Invalid port number: {}\n", argv[i]);
                    return 1;
                }
            }
        }
        else if (arg == "--script" || arg == "-s") {
            if (i + 1 < argc) {
                script_file = argv[++i];
            }
            else {
                fmt::print(fg(fmt::color::red), "Script filename required\n");
                return 1;
            }
        }
        else if (arg == "--project" || arg == "-p") {
            if (i + 1 < argc) {
                project_folder = argv[++i];
                file_util::set_project_path(project_folder);
            }
            else {
                fmt::print(fg(fmt::color::red), "Project path required\n");
                return 1;
            }
        }
        else if (arg == "--help" || arg == "-h") {
            show_help = true;
        }
        else {
            fmt::print(fg(fmt::color::red), "Unknown argument: {}\n", arg);
            print_usage();
            return 1;
        }
    }

    if (show_help) {
        print_usage();
        return 0;
    }

    try {
        // Создаем REPL
        ReplWrapper repl("soot-user");

        // Настраиваем конфигурацию
        auto& config = repl.get_config();
        config.enable_network = use_network;
        config.network_port = network_port;

        // Запускаем выбранный режим
        if (!script_file.empty()) {
            repl.run_script(script_file);
        }
        else if (use_network) {
            repl.run_network(network_port);
            // После запуска сети тоже запускаем интерактивный режим
            repl.run_interactive();
        }
        else {
            repl.run_interactive();
        }

    }
    catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "Fatal error: {}\n", e.what());
        return 1;
    }

    return 0;
}