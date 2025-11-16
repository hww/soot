#pragma once
#include "fmt/format.h"
#include "fmt/color.h"
#include "script/interpreter.h"  // ← добавляем интерпретатор
#include <iostream>
#include <vector>
#include <string>

class SimpleRepl {
private:
    std::vector<std::string> history_;
    std::string history_file_ = ".aleste_history";
    script::Interpreter interpreter_;  // ← наш интерпретатор!

public:
    SimpleRepl() : interpreter_("aleste-user") {}  // ← инициализируем

    void run() {
        load_history();
        print_welcome();

        std::string input;
        while (true) {
            std::cout << "aleste> ";
            if (!std::getline(std::cin, input)) break;

            if (input == "quit" || input == "exit") break;
            if (input.empty()) continue;

            handle_command(input);
            history_.push_back(input);
        }

        save_history();
        fmt::print(fg(fmt::color::green), "👋 Goodbye!\n");
    }

private:
    void print_welcome() {
        fmt::print(fg(fmt::color::steel_blue), "┌────────────────────────────────────────┐\n");
        fmt::print(fg(fmt::color::gold),       "│                ALESTE LISP             │\n");
        fmt::print(fg(fmt::color::light_blue), "│            REPL with Eval v1.0         │\n");
        fmt::print(fg(fmt::color::steel_blue), "└────────────────────────────────────────┘\n");
        fmt::print("Type 'help' for commands, 'quit' to exit\n\n");
    }

    void handle_command(const std::string& command) {
        // Встроенные команды REPL
        if (command == "help") {
            print_help();
            return;
        }
        if (command == "clear") {
            clear_screen();
            return;
        }
        if (command == "history") {
            show_history();
            return;
        }

        // ВЫПОЛНЕНИЕ LISP КОДА! 🚀
        try {
            std::string filename("repl");
            std::string expression(command);
            auto result = interpreter_.eval_string(expression, filename);
            fmt::print(fg(fmt::color::green), "=> {}\n", result.print().c_str());
        }
        catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
        }
    }

    // ... остальные методы без изменений ...
    void print_help() {
        fmt::print("Available commands:\n");
        fmt::print(fg(fmt::color::green), "  help    {} Show this help\n", "→");
        fmt::print(fg(fmt::color::green), "  clear   {} Clear screen\n", "→");
        fmt::print(fg(fmt::color::green), "  history {} Show command history\n", "→");
        fmt::print(fg(fmt::color::green), "  quit    {} Exit REPL\n", "→");
        fmt::print("\nLisp examples:\n");
        fmt::print("  (+ 1 2 3)           {} Add numbers\n", "→");
        fmt::print("  (define x 42)       {} Define variable\n", "→");
        fmt::print("  (lambda (x) (* x x)) {} Create function\n", "→");
    }

    void clear_screen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        print_welcome();
    }

    void show_history() {
        if (history_.empty()) {
            fmt::print("No command history\n");
            return;
        }
        fmt::print("Command history:\n");
        for (size_t i = 0; i < history_.size(); ++i) {
            fmt::print("  {:3}: {}\n", i + 1, history_[i]);
        }
    }

    void load_history() {
        // TODO: загрузка из файла
    }

    void save_history() {
        // TODO: сохранение в файл
    }
};