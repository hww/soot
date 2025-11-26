#pragma once
#include "third_party/fmt/include/fmt/format.h"
#include "third_party/fmt/include/fmt/color.h"
#include "common/script/interpreter.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

class Repl {
private:
    std::vector<std::string> history_;
    script::Interpreter interpreter_;
    std::atomic<bool> network_running_{ false };
    std::thread network_thread_;
    int server_socket_ = -1;

public:
    Repl() : interpreter_("aleste-user") {}
    ~Repl() { stop_network(); }

    void run() {
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

        fmt::print(fg(fmt::color::green), "� Goodbye!\n");
    }

    void start_network(int port = 8181) {
        if (network_running_) return;

        // Инициализация сокетов
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fmt::print(fg(fmt::color::red), "❌ Failed to initialize sockets\n");
            return;
        }
#endif

        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            fmt::print(fg(fmt::color::red), "❌ Failed to create socket\n");
            return;
        }

        // Настройка сервера
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            fmt::print(fg(fmt::color::red), "❌ Failed to bind socket\n");
            close_socket(server_socket_);
            return;
        }

        if (listen(server_socket_, 5) < 0) {
            fmt::print(fg(fmt::color::red), "❌ Failed to listen on socket\n");
            close_socket(server_socket_);
            return;
        }

        network_running_ = true;
        network_thread_ = std::thread(&Repl::network_worker, this, port);

        fmt::print(fg(fmt::color::green), "✅ Network server started on port {}\n", port);
        fmt::print(fg(fmt::color::gray), "   Connect with: telnet localhost {}\n", port);
    }

    void stop_network() {
        network_running_ = false;
        if (network_thread_.joinable()) {
            network_thread_.join();
        }
        if (server_socket_ >= 0) {
            close_socket(server_socket_);
            server_socket_ = -1;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }

private:
    void print_welcome() {
        fmt::print(fg(fmt::color::steel_blue), "┌────────────────────────────────────────┐\n");
        fmt::print(fg(fmt::color::gold), "│                ALESTE LISP             │\n");
        fmt::print(fg(fmt::color::light_blue), "│            REPL with Network           │\n");
        fmt::print(fg(fmt::color::steel_blue), "└────────────────────────────────────────┘\n");
        fmt::print("Type 'help' for commands, 'quit' to exit\n\n");
    }

    void handle_command(const std::string& command) {
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

        try {
            auto result = interpreter_.eval_string(command, "repl");
            fmt::print(fg(fmt::color::green), "=> {}\n", result.print().c_str());
        }
        catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
        }
    }

    void network_worker(int port) {
        while (network_running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            int client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);

            if (client_socket >= 0) {
                char buffer[1024];
                int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::string message(buffer);

                    fmt::print(fg(fmt::color::blue), "[NETWORK] Received: {}\n", message);

                    try {
                        auto result = interpreter_.eval_string(message, "network");
                        fmt::print(fg(fmt::color::green), "[NETWORK] Result: {}\n", result.print().c_str());
                    }
                    catch (const std::exception& e) {
                        fmt::print(fg(fmt::color::red), "[NETWORK] Error: {}\n", e.what());
                    }
                }

                close_socket(client_socket);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

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

    static void close_socket(int sock) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
};