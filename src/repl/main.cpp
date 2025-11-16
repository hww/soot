#include "repl.hpp"
#include "script/interpreter.h"
#include "fmt/format.h"

void print_help() {
    fmt::print("REPL Custom Lisp\n");
    fmt::print("Available arguments : \n");
    fmt::print(fg(fmt::color::green), "  --help      {} Show this help\n", "→");
    fmt::print(fg(fmt::color::green), "  --network   {} Use networking\n", "→");
    fmt::print(fg(fmt::color::green), "  --script    {} Execute script\n", "→");
    fmt::print("\nLisp examples:\n");
    fmt::print("  repl --repl                       {} Run REPL\n", "→");
    fmt::print("  repl --script  \"(define x 42)\"  {} Execute script\n", "→");
}

int main(int argc, char* argv[]) {
    bool use_network = false;
    int network_port = 8181;

    std::string script_file;

    // Парсим аргументы
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--network" || arg == "-n") {
            use_network = true;
            if (i + 1 < argc) {
                network_port = std::stoi(argv[++i]);
            }
        }
        else if (arg == "--script" || arg == "-s") {
            if (i + 1 < argc) {
                script_file = argv[++i];
            }
        }
        else if (arg == "--help" || arg == "-h") {
            // print the command line tool help
            print_help();
            return 0;
        }
    }

    // Запускаем выбранный режим
    if (!script_file.empty()) {
        // Выполнение скрипта
    } else {
        SimpleRepl repl;
        repl.run();
    }

    return 0;
}