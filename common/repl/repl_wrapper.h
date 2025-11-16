#pragma once

#include "config.h"
#include "keybinds.h"
#include "../script/interpreter.h"
#include "../script/reader.h"
#include "third_party/fmt/include/fmt/format.h"
#include "third_party/fmt/include/fmt/color.h"
#include "third_party/replxx/include/replxx.hxx"
#include "nrepl/repl_server.h"
#include "nrepl/repl_client.h"

#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

class ReplWrapper {
public:
    ReplWrapper(const std::string& username);
    ~ReplWrapper();

    // Основные режимы
    void run_interactive();
    void run_network(int port = 8181);
    void run_script(const std::string& filename);

    // Управление
    void print_welcome(const std::vector<std::string>& loaded_projects = {});
    void print_help();
    void print_keybind_help();
    void clear_screen();

    // История
    void load_history();
    void save_history();
    void add_to_history(const std::string& line);

    // Доступ к компонентам
    Config& get_config() { return config_; }
    script::Interpreter& get_interpreter() { return interpreter_; }

    // Сеть
    void start_network_server(int port);
    void stop_network_server();
    void handle_network_message(const std::string& message, int client_socket);

private:
    void init_settings();
    void execute_line(const std::string& line);
    void setup_keybinds();
    void network_server_worker(int port);
    std::string get_current_repl_token(const std::string& context);
    void load_startup_files();
    void execute_startup_commands(const std::vector<std::string>& commands);

    std::unique_ptr<ReplServer> network_server_;
    std::thread network_thread_;
    std::atomic<bool> network_running_{ false };
    replxx::Replxx repl;
    Config config_;
    std::vector<KeyBind> keybinds_;  // Исправляем KeyBinds на vector<KeyBind>
    script::Interpreter interpreter_;
    script::Reader reader;
    std::string username;
    bool nrepl_alive = false;
};