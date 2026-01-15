#include "common/soot/ReplWrapper.hpp"
#include "third_party/fmt/include/fmt/format.h"
#include <iostream>

void print_usage() {
    fmt::print("SOOT Lisp - REPL Environment\n\n");
    fmt::print("Usage:\n");
    fmt::print("  soot                    {} Interactive REPL\n", "→");
    fmt::print("  soot --server [port]    {} Network REPL server\n", "→");
    fmt::print("  soot --client [port]    {} Network REPL clinet\n", "→");
    fmt::print("  soot --script <file>    {} Execute script file\n", "→");
    fmt::print("  soot --help             {} Show this help\n", "→");
    fmt::print("\nExamples:\n");
    fmt::print("  soot                    {} Start interactive mode\n", "→");
    fmt::print("  soot --server 8181      {} Start network server\n", "→");
    fmt::print("  soot --script demo.lisp {} Run script\n", "→");
}

int main(int argc, char* argv[]) {
    // Парсим аргументы командной строки
    std::string script_file;
    std::string project_folder;
    bool server = false;
    bool client = false;
    int network_port = 8181;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--server") {
            server = true;
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
        else if (arg == "--client") {
            client = true;
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
        config.network_port = network_port;

        // Запускаем выбранный режим
        if (!script_file.empty()) {
            repl.run_script(script_file);
        }
        else if (server) {
            repl.run_server("localhost", network_port);
            // Важно: сервер не читает stdin, он просто спит или 
            // обрабатывает другие задачи, пока крутится сетевой поток.
            while(true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
        } 
        else if (client) {
            // Включаем клиентский режим
            repl.run_client("localhost", network_port);
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