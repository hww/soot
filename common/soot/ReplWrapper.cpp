#include "common/soot/ReplWrapper.hpp"
#include "common/sooti/Errors.hpp"
#include "common/sooti/Object.hpp"
#include "common/util/FileUtil.hpp"
#include "common/util/Log.hpp"
#include "common/versions/revision.h"
#include "common/versions/version.h"
#include "fmt/color.h"
#include "fmt/format.h"

#include "common/sooti/PrettyPrinter.hpp"


namespace fs = std::filesystem;

// ============================================================
// Constructor/Destructor
// ============================================================

ReplWrapper::ReplWrapper(const std::string &username)
    : username_(username), interpreter_(username), reader_(), loaded_files_(8), prompt_("soot> "),
      prompt_incomplete_("      ") {
    init_settings();
    // Загружаем конфиг (клавиши, порты)
    load_config("config.sot");
}

ReplWrapper::~ReplWrapper() {
    interpreter_.Terminate();
    stop_network_server();
}

// ============================================================
// Init/Settings
// ============================================================

void ReplWrapper::init_settings() {
    repl_.set_word_break_characters(" \t");
    repl_.set_max_history_size(1000);

    repl_.set_complete_on_empty(false);
    repl_.set_indent_multiline(true);
    repl_.set_beep_on_ambiguous_completion(false);
    repl_.set_no_color(false);

    // Настраиваем автодополнение - правильные типы
    repl_.set_completion_callback(
        [this](const std::string &input, int &context_len) -> replxx::Replxx::completions_t {
            return get_completions(input, context_len);
        });

    // Настраиваем подсказки - правильные типы
    repl_.set_hint_callback([this](const std::string &input, int &context_len,
                                   replxx::Replxx::Color &color) -> replxx::Replxx::hints_t {
        color = replxx::Replxx::Color::GREEN;
        return get_hints(input, context_len, color);
    });

    setup_keybinds();
}

// ============================================================
// REPL
// ============================================================

void ReplWrapper::run_interactive() {
    load_history();
    load_startup_files();
    print_welcome({"core", "stdlib"});

    while (true) {
        // Читаем многострочное выражение
        std::string code = read_multiline_expression("");

        // Проверяем EOF
        if (code.empty()) {
            // Это может быть EOF или просто пустой ввод
            // Проверяем, был ли это реально EOF
            if (should_exit_)
                break;
            continue;
        }

        // Добавляем в историю ДО выполнения, чтобы даже команды с ошибками сохранялись
        if (!code.empty() && code != "\n" && code != "\r\n") {
            add_to_history(code);
        }

        execute_line(code);

        if (should_exit_) {
            break;
        }
    }

    save_history();
}

void ReplWrapper::execute_line(const std::string &line) {
    // Специальные команды тоже должны сохраняться в истории,
    // но мы не хотим выполнять их как Lisp код

    if (line == "(help)" || line == "help") {
        print_help();
        return;
    }
    if (line == "(keybinds)" || line == "keybinds") {
        print_keybind_help();
        return;
    }
    if (line == "(clear)" || line == "clear") {
        clear_screen();
        return;
    }
    if (line == "(quit)" || line == "quit" || line == "exit") {
        should_exit_ = true; // Устанавливаем флаг выхода
        return;
    }
    if (line == "(history)" || line == "history") {
        show_history();
        return;
    }
    if (line == "(ee)") {
        inspect_top_env();
        return;
    }
    if (line == "(edb)") {
        inspect_text_db();
        return;
    }
    if (line == "(esym)") {
        inspect_symbol_table();
        return;
    }
    if (line == ":multi on") {
        set_multi_line_enabled(true);
        fmt::print("Multi-line mode enabled\n");
        return;
    }
    if (line == ":multi off") {
        set_multi_line_enabled(false);
        fmt::print("Multi-line mode disabled\n");
        return;
    }
    if (line == ":check on") {
        set_check_completion(true);
        fmt::print("Completion checking enabled\n");
        return;
    }
    if (line == ":check off") {
        set_check_completion(false);
        fmt::print("Completion checking disabled\n");
        return;
    }
    if (line == ":modes") {
        fmt::print("Multi-line: {}\n", multi_line_enabled_ ? "ON" : "OFF");
        fmt::print("Completion check: {}\n", check_completion_ ? "ON" : "OFF");
        return;
    }
    execute_line_internal(line, true);
}

void ReplWrapper::print_welcome(const std::vector<std::string> &loaded_projects) {
    (void)loaded_projects;
    // Используем "графитовый" или стальной цвет для рамок
    auto border_color = fg(fmt::color::dim_gray);
    auto title_color = fg(fmt::color::light_gray) | fmt::emphasis::bold;
    // auto accent_color = fg(fmt::color::orange_red); // Цвет тлеющего уголька для акцента

    // Логотип SOOT
    fmt::print(border_color, "----------------------------------------\n");
    fmt::print(title_color, "              S  O  O  T                \n");
    fmt::print(fg(fmt::color::slate_gray), "   Scriptable Object-Oriented Toolkit   \n");
    fmt::print(border_color, "----------------------------------------\n");

    // Техническая информация
    fmt::print(fg(fmt::color::gray), " core:    ");
    fmt::print(fg(fmt::color::antique_white), "{} {}\n", SOOT_VERSION, SOOT_NAME);

    fmt::print(fg(fmt::color::gray), " build:   ");
    fmt::print(fg(fmt::color::cadet_blue), "sha:{} tag:{}\n", BUILT_SHA, BUILT_TAG);

    fmt::print(fg(fmt::color::gray), " type:    ");
    fmt::print(fg(fmt::color::gold), "Interactive Shell (REPL)\n");

    fmt::print(border_color, "----------------------------------------\n");

    fmt::print("Type {} or {} for help\n\n", fmt::format(fg(fmt::color::cyan), "(help)"),
               fmt::format(fg(fmt::color::cyan), "(keybinds)"));
}

void ReplWrapper::print_help() {
    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nREPL Commands:\n");
    fmt::print(fg(fmt::color::cyan), "  (help)              {} Show this help\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (keybinds)          {} Show key bindings\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (clear)             {} Clear screen\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (history)           {} Show command history\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (quit)              {} Exit REPL\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (ee)                {} Inspect environment\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (edb)               {} Inspect text DB\n", "→");
    fmt::print(fg(fmt::color::cyan), "  (esym)              {} Inspect symbol table\n", "→");

    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nStartup Files:\n");
    fmt::print("  startup-pre.sot      {} Run before network\n", "→");
    fmt::print("  startup-post.sot     {} Run after network\n", "→");

    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nLisp Examples:\n");
    fmt::print("  (+ 1 2 3)            {} Add numbers\n", "→");
    fmt::print("  (define x 42)        {} Define variable\n", "→");
    fmt::print("  (lambda (x) (* x x)) {} Create function\n", "→");
}

void ReplWrapper::print_keybind_help() {
    fmt::print(fg(fmt::color::gold) | fmt::emphasis::bold, "\nKey Bindings:\n");
    for (const auto &bind : config_.keybinds) { // Исправляем config на config_
        fmt::print(fg(fmt::color::cyan), "  {:<15} {} {}\n", bind.toString(), "в†’",
                   bind.description);
    }
}

void ReplWrapper::clear_screen() {
    repl_.clear_screen();
    print_welcome({"core", "stdlib"});
}

// ============================================================
// Completitions
// ============================================================

// Completions возвращает vector<pair<string, Color>>
replxx::Replxx::completions_t ReplWrapper::get_completions(const std::string &input,
                                                           int               &context_len) {
    replxx::Replxx::completions_t completions;
    if (input.empty())
        return completions;

    // 1. Находим начало последнего слова (границы: пробел, скобки, кавычки)
    size_t last_token_pos = input.find_last_of(" ( ' \" \t");

    std::string word_to_match;
    if (last_token_pos == std::string::npos) {
        word_to_match = input;
    } else {
        word_to_match = input.substr(last_token_pos + 1);
    }

    // Сообщаем Replxx, сколько символов с конца строки мы заменяем
    context_len = static_cast<int>(word_to_match.length());

    // 2. Дополнения из интерпретатора (функции, переменные)
    std::string       matched_symbols = interpreter_.get_all_symbols_matching(word_to_match);
    std::stringstream ss(matched_symbols);
    std::string       sym;
    while (ss >> sym) {
        // Добавляем только само слово, Replxx подставит его после префикса
        completions.emplace_back(sym, replxx::Replxx::Color::CYAN);
    }

    // 3. Встроенные команды (только если это самое начало строки)
    if (last_token_pos == std::string::npos) {
        std::vector<std::string> builtins = {"help", "clear", "quit", "history"};
        for (const auto &cmd : builtins) {
            if (cmd.find(word_to_match) == 0) {
                completions.emplace_back(cmd, replxx::Replxx::Color::GREEN);
            }
        }
    }

    return completions;
}

// Hints возвращает vector<pair<string, Color>>
replxx::Replxx::hints_t ReplWrapper::get_hints(const std::string &input, int &context_len,
                                               replxx::Replxx::Color &color) {
    (void)color;

    replxx::Replxx::hints_t hints;

    if (input.empty())
        return hints;

    // Подсказки для built-in команд
    std::string hint_text;
    if (input == "(help" || input == "help")
        hint_text = " - show help";
    else if (input == "(keybinds" || input == "keybinds")
        hint_text = " - show key bindings";
    else if (input == "(clear" || input == "clear")
        hint_text = " - clear screen";
    else if (input == "(quit" || input == "quit")
        hint_text = " - exit REPL";
    else if (input == "(history" || input == "history")
        hint_text = " - show command history";
    else
        return hints;

    hints.push_back(hint_text);
    context_len = input.length();
    return hints;
}

std::string ReplWrapper::extract_prefix(const std::string &command) {
    // Ищем первую и последнюю кавычку
    size_t first_quote = command.find('"');
    size_t last_quote = command.find_last_of('"');

    if (first_quote != std::string::npos && last_quote != std::string::npos &&
        last_quote > first_quote) {
        // Вырезаем содержимое между кавычками
        return command.substr(first_quote + 1, last_quote - first_quote - 1);
    }

    // Если кавычек нет, возвращаем пустую строку или саму команду (на всякий случай)
    return "";
}

// ============================================================
// History
// ============================================================

void ReplWrapper::load_history() {
    try {
        fs::path cache_path = file_util::get_path(file_util::PathType::CACHE);
        fs::create_directories(cache_path); // Автоматическое создание папки

        std::string history_file = (cache_path / "history").string();

        // Проверяем, существует ли файл истории
        if (fs::exists(history_file)) {
            repl_.history_load(history_file);
            lg::debug("Loaded {} history entries from {}", repl_.history_size(), history_file);
        } else {
            lg::debug("No history file found at {}, starting fresh", history_file);
        }
    } catch (const std::exception &e) {
        lg::warn("Failed to load history: {}", e.what());
    }
}

void ReplWrapper::save_history() {
    try {
        fs::path cache_path = file_util::get_path(file_util::PathType::CACHE);
        fs::create_directories(cache_path);

        std::string history_file = (cache_path / "history").string();
        repl_.history_save(history_file);

        lg::debug("Saved {} history entries to {}", repl_.history_size(), history_file);
    } catch (const std::exception &e) {
        lg::warn("Failed to save history: {}", e.what());
    }
}

void ReplWrapper::add_to_history(const std::string &line) {
    // Не добавляем пустые строки или строки только с пробелами
    if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos) {
        return;
    }

    // Не добавляем дубликаты последней команды
    if (repl_.history_size() > 0) {
        auto scan = repl_.history_scan();
        if (scan.next()) {
            auto last_entry = scan.get();
            if (last_entry.text() == line) {
                return; // Пропускаем дубликат
            }
        }
    }

    repl_.history_add(line);
    lg::debug("Added to history: {}", line);
}

void ReplWrapper::show_history() {
    int history_size = repl_.history_size();

    if (history_size == 0) {
        fmt::print("No command history\n");
        return;
    }

    fmt::print("Command history ({} commands):\n", history_size);

    // Используем HistoryScan для доступа к истории
    auto scan = repl_.history_scan();
    int  index = 1;

    while (scan.next()) {
        auto entry = scan.get();
        // Показываем только непустые команды
        std::string text = entry.text();
        if (!text.empty() && text.find_first_not_of(" \t\n\r") != std::string::npos) {
            fmt::print("  {:3}: {}\n", index++, text);
        }
    }

    // Если после фильтрации ничего не осталось
    if (index == 1) {
        fmt::print("  (no visible commands)\n");
    }
}

void ReplWrapper::debug_history() {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "\n=== History Debug ===\n");
    fmt::print("History size: {}\n", repl_.history_size());
    fmt::print("History file: {}\n",
               (file_util::get_path(file_util::PathType::CACHE) / "history").string());

    fmt::print(fg(fmt::color::cyan), "\nRaw history entries:\n");
    auto scan = repl_.history_scan();
    int  index = 0;

    while (scan.next()) {
        auto entry = scan.get();
        fmt::print("  [{}] '{}' (length: {})\n", index++, entry.text(), entry.text().length());
    }
}
// ============================================================
// Starting
// ============================================================

void ReplWrapper::load_file(const std::string &filename) {
    fmt::print(fg(fmt::color::cyan), "✓ Running script: {}\n", filename);
    script::Object result = script::Object::make_null();
    try {
        // Предполагаем, что у интерпретатора есть метод для загрузки файла
        result = interpreter_.eval_string(fmt::format("(load \"{}\")", filename), "script");
    } catch (script::EvalException &e) {
        if (e.already_printed)
            return;
        std::string report = e.full_report(interpreter_.get_reader());
        fmt::print("{}", report);
        e.already_printed = true;
    } catch (script::ExitException &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
        exit(e.exit_code);
    } catch (const std::exception &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "✗ Script error: {}\n", e.what());
    }
}

// Режим СЕРВЕРА
void ReplWrapper::run_server(std::string host, int port) {
    is_client_mode_ = false;
    fmt::print(fg(fmt::color::cyan), "✓ Starting SOOT Server on {}:{}...\n", host, port);

    run_server_impl(host, port);

    // В режиме сервера мы можем запустить локальный REPL для управления сервером
    run_interactive();
}

// Режим КЛИЕНТА
void ReplWrapper::run_client(std::string host, int port) {
    is_client_mode_ = true;
    fmt::print(fg(fmt::color::cyan), "✓ Connecting to SOOT Server at {}:{}...\n", host, port);

    if (run_client_impl(host, port)) {
        run_interactive();
    } else {
        fmt::print(fg(fmt::color::red), "✗ Connection failed. Exiting.\n");
    }
}

// ============================================================
// Networking Implementation
// ============================================================

bool ReplWrapper::run_server_impl(std::string host, int port) {
    (void)host;
    if (is_server_running_)
        return true;

    // Серверная часть: слушаем входящие
    network_server_ = std::make_unique<ReplServer>([this]() { return !is_server_running_; }, port);

    network_server_->set_message_handler(
        [this](const std::string &msg, int client) { handle_network_message(msg, client); });

    if (network_server_->init_server()) {
        is_server_running_ = true;
        network_thread_ = std::thread(&ReplWrapper::network_server_worker, this, port);
        fmt::print(fg(fmt::color::green), "✓ Network REPL server started on port {}\n", port);
        return true;
    } else {
        fmt::print(fg(fmt::color::red), "✗ Failed to start network REPL server\n");
    }
    return false;
}

// КЛИЕНТСКАЯ реализация (подключение к серверу)
bool ReplWrapper::run_client_impl(std::string host, int port) {
    fmt::print("DEBUG: Starting client connection to {}:{}\n", host, port);

    if (is_client_running_) {
        fmt::print("DEBUG: Client already running\n");
        return true;
    }

    // 1. Создаем сокет
    client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    fmt::print("DEBUG: Socket created: {}\n", client_socket_);

    if (client_socket_ < 0) {
        fmt::print(fg(fmt::color::red), "✗ Failed to create socket: {}\n", strerror(errno));
        return false;
    }

    // 2. Устанавливаем таймаут на чтение
    struct timeval tv;
    tv.tv_sec = 3; // 3 секунды таймаут
    tv.tv_usec = 0;
    setsockopt(client_socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    std::string ip = (host == "localhost") ? "127.0.0.1" : host;
    fmt::print("DEBUG: Using IP: {}\n", ip);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        fmt::print(fg(fmt::color::red), "✗ Invalid address: {}\n", ip);
        close(client_socket_);
        client_socket_ = -1;
        return false;
    }

    fmt::print(fg(fmt::color::cyan), "Connecting...\n");

    int result = connect(client_socket_, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    fmt::print("DEBUG: connect() returned: {}\n", result);

    if (result < 0) {
        fmt::print(fg(fmt::color::red), "✗ Connection failed: {}\n", strerror(errno));
        close(client_socket_);
        client_socket_ = -1;
        return false;
    }

    fmt::print("DEBUG: Connected! Waiting for welcome message...\n");

    // 3. Читаем приветствие с таймаутом
    char    welcome[256] = {0};
    ssize_t n = recv(client_socket_, welcome, sizeof(welcome) - 1, 0);
    fmt::print("DEBUG: recv() returned: {} bytes\n", n);

    if (n > 0) {
        welcome[n] = '\0';
        fmt::print(fg(fmt::color::green), "{}", welcome);
    } else if (n == 0) {
        fmt::print("DEBUG: Server closed connection immediately\n");
    } else {
        fmt::print("DEBUG: recv error: {}\n", strerror(errno));
    }

    is_client_running_ = true;
    is_client_mode_ = true;
    fmt::print(fg(fmt::color::green), "✓ Connected to server\n");
    return true;
}

void ReplWrapper::stop_network_server() {
    is_server_running_ = false;
    if (network_thread_.joinable()) {
        network_thread_.join();
    }
    if (network_server_) {
        network_server_->shutdown_server(); // БЫЛО: shutdown()
        network_server_.reset();
    }
}

void ReplWrapper::network_server_worker(int port) {
    (void)port;
    lg::info("Network worker started, calling get_msg() loop");

    while (is_server_running_) {
        auto message = network_server_->get_msg();

        if (message.has_value()) {
            lg::info("Got network message: {}", *message);
        }

        if (!is_server_running_)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    lg::info("Network worker stopped");
}

void ReplWrapper::handle_network_message(const std::string &message, int client_socket) {
    // 1. Обработка системных команд автодополнения
    if (message.starts_with("(__get_completions")) {
        std::string prefix = extract_prefix(message);
        std::string completions = interpreter_.get_all_symbols_matching(prefix);
        std::string response = "COMPLETIONS:" + completions + "\n";
        send(client_socket, response.c_str(), response.size(), 0);
        return;
    }

    // 2. Очистка обычного кода
    std::string trimmed = message;
    trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);

    if (trimmed.empty())
        return;

    // 3. Выполнение кода на сервере
    try {
        // Здесь мы используем интерпретатор сервера
        auto        result = interpreter_.eval_string(trimmed, "network");
        auto        result_str = script::pretty_print::to_string(result);
        std::string response = fmt::format(fg(fmt::color::green), "=> {}\n", result_str);
        send(client_socket, response.c_str(), response.size(), 0);
    } catch (script::ExitException &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
        exit(e.exit_code);
    } catch (script::EvalException &e) {
        if (e.already_printed)
            return;
        std::string report = e.full_report(interpreter_.get_reader());

        // Печатаем локально (для отладки сервера)
        fmt::print("{}", report);

        // Отправляем клиенту в сокет
        send(client_socket, report.c_str(), report.size(), 0);

        e.already_printed = true;
    } catch (const std::exception &e) {
        // Не печатаем детали тут, они уже ушли в консоль сервера через eval_with_rewind
        std::string error = "Error: " + std::string(e.what()) + "\n";
        send(client_socket, error.c_str(), error.size(), 0);
    }
}

// ============================================================
// Основная логика выполнения (Локально vs Сеть)
// ============================================================

void ReplWrapper::execute_line_internal(const std::string &line, bool print_result) {
    if (is_client_mode_) {
        // КЛИЕНТ: шлет код на сервер
        if (client_socket_ != -1) {
            std::string msg = line + "\n";
            ssize_t     sent = send(client_socket_, msg.c_str(), msg.size(), 0);

            if (sent <= 0) {
                fmt::print(fg(fmt::color::red), "✗ Failed to send data to server: {}\n",
                           strerror(errno));
                should_exit_ = true;
                return;
            }

            // Ждем ответа с таймаутом
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(client_socket_, &read_fds);

            struct timeval tv;
            tv.tv_sec = 5; // 5 секунд таймаут
            tv.tv_usec = 0;

            int activity = select(client_socket_ + 1, &read_fds, NULL, NULL, &tv);

            if (activity > 0 && FD_ISSET(client_socket_, &read_fds)) {
                char    buffer[4096] = {0};
                ssize_t valread = recv(client_socket_, buffer, sizeof(buffer) - 1, 0);

                if (valread > 0) {
                    buffer[valread] = '\0';
                    fmt::print("{}", buffer);
                } else if (valread == 0) {
                    fmt::print(fg(fmt::color::red), "Server disconnected.\n");
                    should_exit_ = true;
                } else {
                    fmt::print(fg(fmt::color::red), "Error receiving data: {}\n", strerror(errno));
                    should_exit_ = true;
                }
            } else if (activity == 0) {
                fmt::print(fg(fmt::color::yellow), "⚠ Server timeout (no response in 5 seconds)\n");
            } else {
                fmt::print(fg(fmt::color::red), "Select error: {}\n", strerror(errno));
            }
        } else {
            fmt::print(fg(fmt::color::red), "Not connected to server.\n");
        }
    } else {
        // СЕРВЕР или ОБЫЧНЫЙ РЕЖИМ: выполняем локально
        try {
            auto result = interpreter_.eval_string(line, "repl");
            if (print_result) {
                auto result_str = script::pretty_print::to_string(result);
                fmt::print(fg(fmt::color::green), "=> {}\n", result_str);
            }
        } catch (script::ExitException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
            exit(e.exit_code);
        } catch (script::EvalException &e) {
            if (e.already_printed)
                return;

            std::string report = e.full_report(interpreter_.get_reader());
            fmt::print("{}", report);
            // Если работаешь через сокеты:
            // send(client_socket, report.c_str(), report.size(), 0);
            e.already_printed = true;
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
        }
    }
}
// ============================================================
// Files
// ============================================================

void ReplWrapper::load_startup_files() {

    std::string lib_path = file_util::find_config_file("lib.sot").string();
    if (!lib_path.empty()) {
        fmt::print(fg(fmt::color::gray), "✓ Loading: {}\n", lib_path);
        execute_line_internal(fmt::format("(load \"{}\")", lib_path), false);
    }

    // ЭТАП 1: Pre-Network (Настройка окружения, загрузка библиотек)
    // Ищем везде, где может лежать startup-pre.gc
    std::string pre_path = file_util::find_config_file("startup-pre.sot").string();
    if (!pre_path.empty()) {
        // lg::info("Loading pre-startup: {}", pre_path);
        fmt::print(fg(fmt::color::gray), "✓ Loading: {}\n", pre_path);
        execute_line_internal(fmt::format("(load \"{}\")", pre_path), false);
    }

    // ЭТАП 2: Post-Network (Только если сеть успешно поднята)
    if (config_.enable_network && is_server_running_) {
        std::string post_path = file_util::find_config_file("startup-post.sot").string();
        if (!post_path.empty()) {
            lg::info("Loading post-startup: {}", post_path);
            fmt::print(fg(fmt::color::gray), "✓ Loading: {}\n", post_path);
            execute_line_internal(fmt::format("(load \"{}\")", post_path), false);
        }
    }
}

void ReplWrapper::execute_startup_commands(const std::vector<std::string> &commands) {
    for (const auto &command : commands) {
        try {
            lg::debug("Executing startup command: {}", command);
            execute_line(command);
        } catch (const std::exception &e) {
            lg::warn("Startup command failed: {} - Error: {}", command, e.what());
        }
    }
}

// ============================================================
// Keyboard
// ============================================================

void ReplWrapper::setup_keybinds() {
    // Только наши кастомные keybinds из config
    for (const auto &bind : config_.keybinds) {
        char32_t key_code = 0;

        switch (bind.modifier) {
        case KeyBind::Modifier::CTRL:
            if (bind.key.length() == 1) {
                key_code = replxx::Replxx::KEY::control(bind.key[0]);
            }
            break;
        case KeyBind::Modifier::SHIFT:
            if (bind.key.length() == 1) {
                key_code = replxx::Replxx::KEY::shift(bind.key[0]);
            }
            break;
        case KeyBind::Modifier::META:
            if (bind.key.length() == 1) {
                key_code = replxx::Replxx::KEY::meta(bind.key[0]);
            }
            break;
        }

        if (key_code != 0) {
            repl_.bind_key(key_code, [this, command = bind.command](char32_t code) {
                repl_.set_state(
                    replxx::Replxx::State(command.c_str(), static_cast<int>(command.size())));
                return repl_.invoke(replxx::Replxx::ACTION::COMMIT_LINE, code);
            });
        }
    }

    // Системные hotkeys (не конфликтуют с нашими)
    repl_.bind_key(replxx::Replxx::KEY::control('L'), [this](char32_t) {
        clear_screen();
        return repl_.invoke(replxx::Replxx::ACTION::CLEAR_SCREEN, 0);
    });

    repl_.bind_key(replxx::Replxx::KEY::control('R'), [this](char32_t) {
        return repl_.invoke(replxx::Replxx::ACTION::HISTORY_INCREMENTAL_SEARCH, 0);
    });

    // CTRL+D - сразу выход
    repl_.bind_key(replxx::Replxx::KEY::control('D'), [this](char32_t) {
        should_exit_ = true;
        return repl_.invoke(replxx::Replxx::ACTION::COMMIT_LINE, 0);
    });
}

// ============================================================
// Config
// ============================================================

void ReplWrapper::load_config(const std::string &filename) {
    // 1. Пытаемся найти полный путь к конфигу
    std::string actual_path = file_util::find_config_file(filename).string();

    // 2. Если файл не найден ни в одной из локаций
    if (actual_path.empty()) {
        // Не выводим ошибку, если это дефолтный конфиг,
        // просто используем зашитые в код дефолты
        lg::debug("Config file '{}' not found, using internal defaults.", filename);
        return;
    }

    try {
        // 3. Читаем уже по найденному реальному пути
        // Используем actual_path вместо filename
        auto config_data = reader_.read_from_file({actual_path}, true, false);
        parse_config_data(config_data);

        fmt::print(fg(fmt::color::gray), "✓ Loaded config from: {}\n", actual_path);
    } catch (const std::exception &e) {
        // Здесь уже выводим ошибку, так как файл существует, но он битый
        fmt::print(fg(fmt::color::red), "✗ Error parsing config [{}]: {}\n", actual_path, e.what());
    }
}

void ReplWrapper::parse_config_data(const script::Object &config_list) {
    // Рекурсивно разыменовываем quote формы
    script::Object data = config_list;

    while (data.is_pair() && data.as_pair()->car.is_symbol() &&
           data.as_pair()->car.as_symbol().name_ptr == "quote") {
        data = data.as_pair()->cdr;
        if (data.is_pair()) {
            data = data.as_pair()->car;
        }
    }

    auto commands = data.as_c_vector();

    for (const auto &command : commands) {
        if (command.is_pair()) {
            auto pair = command.as_pair();
            if (pair->car.is_symbol()) {
                std::string cmd_name = pair->car.as_symbol().name_ptr;

                // Обрабатываем как связанный список
                if (cmd_name == "nrepl-port") {
                    config_.nrepl_port = pair->cdr.as_pair()->car.as_integer();
                } else if (cmd_name == "prompt") {
                    config_.prompt = pair->cdr.as_pair()->car.as_string()->data;
                } else if (cmd_name == "keybind") {
                    auto rest = pair->cdr;
                    if (rest.is_pair()) {
                        KeyBind bind;
                        bind.modifier = parse_modifier(rest.as_pair()->car.as_symbol().name_ptr);
                        rest = rest.as_pair()->cdr;
                        if (rest.is_pair()) {
                            bind.key = rest.as_pair()->car.as_string()->data;
                            rest = rest.as_pair()->cdr;
                            if (rest.is_pair()) {
                                bind.description = rest.as_pair()->car.as_string()->data;
                                rest = rest.as_pair()->cdr;
                                if (rest.is_pair()) {
                                    bind.command = rest.as_pair()->car.as_string()->data;
                                    config_.keybinds.push_back(bind);
                                }
                            }
                        }
                    }
                }
                // Аналогично для других параметров...
            }
        }
    }
}

KeyBind::Modifier ReplWrapper::parse_modifier(const std::string &mod_str) {
    if (mod_str == "ctrl")
        return KeyBind::Modifier::CTRL;
    if (mod_str == "shift")
        return KeyBind::Modifier::SHIFT;
    if (mod_str == "meta")
        return KeyBind::Modifier::META;
    return KeyBind::Modifier::CTRL;
}

// ============================================================
// Multilines on Input
// ============================================================

std::string ReplWrapper::read_multiline_expression(const std::string &first_line) {
    if (!multi_line_enabled_) {
        return first_line;
    }

    if (check_completion_) {
        return read_multiline_with_check();
    } else {
        return read_multiline_simple();
    }
}

// Режим БЕЗ проверки - просто ждем пустую строку
std::string ReplWrapper::read_multiline_simple() {
    std::string result;
    bool        first_line = true;

    while (true) {
        const char *input;
        if (first_line) {
            input = repl_.input(prompt_);
            first_line = false;
        } else {
            input = repl_.input(prompt_incomplete_);
        }

        if (!input)
            break; // EOF

        std::string line(input);

        // Пустая строка завершает ввод
        if (line.empty() && !result.empty()) {
            break;
        }

        if (!result.empty()) {
            result += "\n";
        }
        result += line;

        // Если отключена проверка и введена пустая строка - завершаем
        if (line.empty()) {
            break;
        }
    }

    return result;
}

// Режим С проверкой - проверяем завершенность выражения
std::string ReplWrapper::read_multiline_with_check() {
    std::string result;
    bool        first_line = true;

    while (true) {
        const char *input;
        if (first_line) {
            input = repl_.input(prompt_);
            first_line = false;
        } else {
            input = repl_.input(prompt_incomplete_);
        }

        if (!input)
            break; // EOF

        std::string line(input);

        // Добавляем к результату
        if (!result.empty()) {
            result += "\n";
        }
        result += line;

        // Проверяем, завершено ли выражение
        if (reader_.is_expression_complete(result)) {
            break;
        }

        // Если введена пустая строка и выражение не завершено, все равно продолжаем
        // Пользователь может ввести пустую строку внутри строкового литерала и т.д.
    }

    return result;
}

// ============================================================
// Inspectors
// ============================================================

void ReplWrapper::inspect_top_env() {
    script::Object obj = interpreter_.get_global_environment();
    fmt::print("Simple:\n{}\n", script::EnvironmentPrettyPrinter::to_string(obj.as_env()));
}

void ReplWrapper::inspect_text_db() {
    auto &db = reader_.get_db();

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "=== Text Database ===\n");

    fmt::print("Fragments: {}\n", db.get_fragment_count());
    fmt::print("Objects with source info: {}\n", db.get_object_count());

    if (db.get_fragment_count() > 0) {
        fmt::print("\n");
        fmt::print(fg(fmt::color::yellow), "Source fragments:\n");
        fmt::print(fg(fmt::color::white), ""); // Сбрасываем цвет

        int i = 0;
        for (const auto &desc : db.get_fragment_descriptions()) {
            fmt::print("  {:2d}: {}\n", i++, desc);
        }
    }
}

void ReplWrapper::inspect_symbol_table() {
    auto &st = interpreter_.symbol_table();

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "=== Symbol Table ===\n");

    fmt::print("Total symbols: {}\n", st.get_symbol_count());

    if (st.get_symbol_count() > 0) {
        // Группируем символы по типам
        std::vector<std::string> regular_symbols;
        std::vector<std::string> keyword_symbols;

        st.for_each_symbol([&](const script::InternedSymbolPtr &sym) {
            if (sym.starts_with_colon()) {
                keyword_symbols.push_back(sym.name_ptr);
            } else {
                regular_symbols.push_back(sym.name_ptr);
            }
        });

        if (!regular_symbols.empty()) {
            fmt::print("\n");
            fmt::print(fg(fmt::color::green), "Regular symbols: ({})\n", regular_symbols.size());
            fmt::print(fg(fmt::color::white), ""); // Сбрасываем цвет
            for (const auto &sym : regular_symbols) {
                fmt::print("  {}\n", sym);
            }
        }

        if (!keyword_symbols.empty()) {
            fmt::print("\n");
            fmt::print(fg(fmt::color::blue), "Keyword symbols: ({})\n", keyword_symbols.size());
            fmt::print(fg(fmt::color::white), ""); // Сбрасываем цвет
            for (const auto &sym : keyword_symbols) {
                fmt::print("  {}\n", sym);
            }
        }

        // Статистика
        fmt::print("\n");
        fmt::print(fg(fmt::color::yellow), "Statistics:\n");
        fmt::print(fg(fmt::color::white), "");
        fmt::print("  Regular symbols: {}\n", regular_symbols.size());
        fmt::print("  Keyword symbols: {}\n", keyword_symbols.size());
        fmt::print("  Memory efficiency: pointer-based comparison\n");
    }
}