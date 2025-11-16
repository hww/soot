#include "repl_wrapper.h"
#include "common/log/log.h"
#include "common/versions/revision.h"
#include <iostream>
#include <fstream>

ReplWrapper::ReplWrapper(const std::string& username)
    : username(username), interpreter_(username), reader() {
    init_settings();
}

ReplWrapper::~ReplWrapper() {
    stop_network_server();
}

void ReplWrapper::run_interactive() {
    load_history();
    print_welcome({ "core", "stdlib" });

    while (true) {
        const char* input = repl.input("aleste> ");
        if (!input || std::string(input) == "quit" || std::string(input) == "(quit)") break;

        std::string line(input);
        if (line.empty()) continue;

        execute_line(line);
        add_to_history(line);
    }

    save_history();
    fmt::print(fg(fmt::color::green), "рџ‘‹ Goodbye!\n");
}

void ReplWrapper::execute_line(const std::string& line) {
    // Встроенные команды REPL
    if (line == "(help)") {
        print_help();
        return;
    }
    if (line == "(keybinds)") {
        print_keybind_help();
        return;
    }
    if (line == "(clear)") {
        clear_screen();
        return;
    }

    // Выполнение Lisp кода
    try {
        auto result = interpreter_.eval_string(line, "repl");
        fmt::print(fg(fmt::color::green), "=> {}\n", result.print());
    }
    catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
    }
}

void ReplWrapper::print_welcome(const std::vector<std::string>& loaded_projects) {
    fmt::print(fg(fmt::color::steel_blue),                  "----------------------------------\n");
    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold,  "             ALESTE LISP          \n");
    fmt::print(fg(fmt::color::light_blue),                  "          Professional REPL       \n");
    fmt::print(fg(fmt::color::steel_blue),                  "--------------------------------- \n");
    fmt::print(fg(fmt::color::gray),                        " version: {} tag: {}\n", BUILT_SHA, BUILT_TAG);

    fmt::print("Loaded: ");
    for (const auto& project : loaded_projects) {
        fmt::print(fg(fmt::color::cyan), "{} ", project);
    }
    fmt::print("\n");

    fmt::print("Type {} or {} for help\n",
        fmt::format(fg(fmt::color::cyan), "(help)"),
        fmt::format(fg(fmt::color::cyan), "(keybinds)"));
}

void ReplWrapper::print_help() {
    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nREPL Commands:\n");
    fmt::print(fg(fmt::color::cyan), "  (help)              {} Show this help\n", "в†’");
    fmt::print(fg(fmt::color::cyan), "  (keybinds)          {} Show key bindings\n", "в†’");
    fmt::print(fg(fmt::color::cyan), "  (clear)             {} Clear screen\n", "в†’");
    fmt::print(fg(fmt::color::cyan), "  (quit)              {} Exit REPL\n", "в†’");

    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nLisp Examples:\n");
    fmt::print("  (+ 1 2 3)           {} Add numbers\n", "в†’");
    fmt::print("  (define x 42)       {} Define variable\n", "в†’");
    fmt::print("  (lambda (x) (* x x)) {} Create function\n", "в†’");
}

void ReplWrapper::print_keybind_help() {
    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nKey Bindings:\n");
    for (const auto& bind : config_.keybinds) {  // Исправляем config на config_
        fmt::print(fg(fmt::color::cyan), "  {:<15} {} {}\n",
            bind.toString(), "в†’", bind.description);
    }
}

void ReplWrapper::clear_screen() {
    repl.clear_screen();
    print_welcome({ "core", "stdlib" });  // Добавляем аргумент
}

void ReplWrapper::init_settings() {
    repl.set_word_break_characters(" \t");
    repl.set_complete_on_empty(false);
    repl.set_max_history_size(1000);
    setup_keybinds();
}

void ReplWrapper::setup_keybinds() {
    for (const auto& bind : config_.keybinds) {  // Исправляем config на config_
        // Настройка горячих клавиш через replxx
        // (реализация зависит от твоей библиотеки)
    }
}

void ReplWrapper::load_history() {
    repl.history_load(".aleste_history");
}

void ReplWrapper::save_history() {
    repl.history_save(".aleste_history");
}

void ReplWrapper::add_to_history(const std::string& line) {
    repl.history_add(line);
}

void ReplWrapper::run_network(int port) {
    fmt::print(fg(fmt::color::cyan), "рџЊђ Network REPL starting on port {}...\n", port);
    start_network_server(port);
    // После запуска сети также запускаем интерактивный режим
    run_interactive();
}

void ReplWrapper::run_script(const std::string& filename) {
    fmt::print(fg(fmt::color::cyan), "рџ“њ Running script: {}\n", filename);
    // TODO: выполнение файла
}

void ReplWrapper::start_network_server(int port) {
    if (network_running_) return;

    // Исправляем конструктор - добавляем shutdown callback
    network_server_ = std::make_unique<ReplServer>(
        [this]() { return !network_running_; },  // shutdown callback
        port
    );

    network_server_->set_message_handler([this](const std::string& msg, int client) {
        handle_network_message(msg, client);
        });

    // Исправляем вызовы методов
    if (network_server_->init_server()) {  // БЫЛО: init()
        network_running_ = true;
        network_thread_ = std::thread(&ReplWrapper::network_server_worker, this, port);
        fmt::print(fg(fmt::color::green), "✓ Network REPL server started on port {}\n", port);
    }
    else {
        fmt::print(fg(fmt::color::red), "✗ Failed to start network REPL server\n");
    }
}

void ReplWrapper::stop_network_server() {
    network_running_ = false;
    if (network_thread_.joinable()) {
        network_thread_.join();
    }
    if (network_server_) {
        network_server_->shutdown_server();  // БЫЛО: shutdown()
        network_server_.reset();
    }
}

void ReplWrapper::network_server_worker(int port) {
    while (network_running_) {
        auto message = network_server_->get_msg();
        // Сообщения обрабатываются в handle_network_message через callback

        if (!network_running_) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ReplWrapper::handle_network_message(const std::string& message, int client_socket) {
    fmt::print(fg(fmt::color::blue), "[NETWORK] Client {}: {}\n", client_socket, message);

    try {
        auto result = interpreter_.eval_string(message, "network");
        std::string response = "=> " + result.print();

        // Используем глобальную write_to_socket вместо ReplServer::
        write_to_socket(client_socket, response.c_str(), response.size());
        fmt::print(fg(fmt::color::green), "[NETWORK] Response sent: {}\n", result.print());

    }
    catch (const std::exception& e) {
        std::string error = "Error: " + std::string(e.what());
        write_to_socket(client_socket, error.c_str(), error.size());
        fmt::print(fg(fmt::color::red), "[NETWORK] Error: {}\n", e.what());
    }
}

void ReplWrapper::load_startup_files() {
    // Загружаем pre-network startup файл
    std::ifstream pre_file("startup-pre.gc");
    if (pre_file) {
        std::vector<std::string> pre_commands;
        std::string line;
        while (std::getline(pre_file, line)) {
            if (!line.empty() && line[0] != ';') { // игнорируем пустые строки и комментарии
                pre_commands.push_back(line);
            }
        }
        execute_startup_commands(pre_commands);
        lg::info("Loaded {} commands from startup-pre.gc", pre_commands.size());
    }

    // Загружаем post-network startup файл (если сеть включена)
    if (config_.enable_network) {
        std::ifstream post_file("startup-post.gc");
        if (post_file) {
            std::vector<std::string> post_commands;
            std::string line;
            while (std::getline(post_file, line)) {
                if (!line.empty() && line[0] != ';') {
                    post_commands.push_back(line);
                }
            }
            execute_startup_commands(post_commands);
            lg::info("Loaded {} commands from startup-post.gc", post_commands.size());
        }
    }
}

void ReplWrapper::execute_startup_commands(const std::vector<std::string>& commands) {
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