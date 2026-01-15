#pragma once

#include "common/soot/Config.hpp"
#include "common/soot/KeyBinds.hpp"
#include "common/sooti/Interpreter.hpp"
#include "common/sooti/Reader.hpp"
#include "common/sooti/PrinterEnv.hpp"
#include "nrepl/ReplServer.hpp"
#include "nrepl/ReplClient.hpp"

#include "third_party/fmt/include/fmt/format.h"
#include "third_party/fmt/include/fmt/color.h"
#include "third_party/replxx/include/replxx.hxx"

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
    void run_server(std::string host, int port = 8181);
    void run_client(std::string host, int port = 8181);
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


    // Работа в многострочном режиме
    void set_multi_line_enabled(bool enabled) { multi_line_enabled_ = enabled; }
    void set_check_completion(bool check) { check_completion_ = check; }

    void inspect_text_db();
    void inspect_symbol_table();
private:
    std::string read_multiline_expression(const std::string& first_line);
    std::string read_multiline_simple();
    std::string read_multiline_with_check();

    // Сеть
    bool run_server_impl(std::string host, int port);
    bool run_client_impl(std::string host, int port);
    void stop_network_server();
    void handle_network_message(const std::string& message, int client_socket);


    void init_settings();
    void setup_keybinds();
    void network_server_worker(int port);
    void load_startup_files();
    void show_history();
    void inspect_top_env();

    void execute_line(const std::string& line);
    void execute_line_internal(const std::string& line, bool print_result);
    void execute_startup_commands(const std::vector<std::string>& commands);

    std::string get_current_repl_token(const std::string& context);
    
    replxx::Replxx::completions_t get_completions(const std::string& input, int& context_len);
    replxx::Replxx::hints_t get_hints(const std::string& input, int& context_len, replxx::Replxx::Color& color);
    std::string extract_prefix(const std::string& command);

    void load_config(const std::string& filename);
    void parse_config_data(const script::Object& config_list);
    KeyBind::Modifier parse_modifier(const std::string& mod_str);

    std::unique_ptr<ReplServer> network_server_;
    std::thread network_thread_;
    std::atomic<bool> is_server_running_ { false };
    std::atomic<bool> is_client_running_ { false };
    std::atomic<bool> is_client_mode_ { false };
    int client_socket_ = -1;
    replxx::Replxx repl_;
    script::Interpreter interpreter_;
    script::Reader reader_;
    std::vector<KeyBind> keybinds_;  // Исправляем KeyBinds на vector<KeyBind>
    std::string username_;
    std::atomic<bool> should_exit_{ false };
    std::vector<std::string> loaded_files_;
    std::string prompt_;
    std::string prompt_incomplete_;
    Config config_;

    bool nrepl_alive_ = false;
    bool multi_line_enabled_ = true;  // Можно сделать настраиваемым
    bool check_completion_ = true;    // Проверять завершенность выражений
};