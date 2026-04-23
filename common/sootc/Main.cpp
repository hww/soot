// main.cpp
#include "CommonTypes.hpp"
#include "repl/nrepl/ReplServer.h"
#include "sootc/compiler/Compiler.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"
#include "common/util/FileUtil.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include <iostream>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include <optional>

namespace fs = std::filesystem;

struct CommandLineOptions {
    std::vector<std::string> input_files;
    std::string target_dir = "build";
    std::string source_root = ".";
    std::string output_name;
    std::string user_profile = "#f";
    bool flat_output = false;
    bool interactive = true;
    bool debug_ast = false;
    bool debug_ir = false;
    bool debug_asm = false;
    bool help = false;
    bool version = false;
    sootc::CompilerMode mode = sootc::CompilerMode::HYBRID;

    // Client/Server режимы
    bool connect = false;           // --connect, -c
    int port = 8181;                // --port
    int temp_port = -1;             // --temp-port
    int timeout_seconds = 30;       // --timeout
    bool disconnect_after = false;  // --disconnect
    bool no_send = false;           // --no-send
    bool wait_connection = false;   // --wait
    
    // Target управление
    bool reset_target = false;      // --reset
    bool stop_target = false;       // --stop
    bool resume_target = false;     // --resume
    bool check_status = false;      // --status
    
    // Debug
    bool debug_mode = false;        // --debug
    bool debug_segment = false;     // --debug-segment
    bool listen_debugger = false;   // --listen
    SootPlatform platform = SootPlatform::Default;
};

void print_banner() {
    fmt::print(fmt::fg(fmt::color::cyan) | fmt::emphasis::bold,  "=== SOOT Compiler & Interpreter v1.0 ===");
    fmt::print("Type :help for help, :exit to quit\n");
}

void print_help(const char* program_name) {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, 
               "SOOT Compiler and Interpreter\n\n");
    fmt::print("Usage: {} [options] [files...]\n\n", program_name);
    
    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Options:\n");
    fmt::print("  --target <dir>       Target directory for compiled files (default: build)\n");
    fmt::print("  --source-root <dir>  Root directory for module namespace (default: .)\n");
    fmt::print("  --profile <name>     Load user profile\n");
    fmt::print("  -o <name>            Output base name\n");
    fmt::print("  --flat               Don't preserve directory structure\n");
    fmt::print("  --no-repl, -n        Disable interactive REPL\n");
    fmt::print("  --compile-only       Only compile, no interpretation\n");
    fmt::print("  --interpret-only     Only interpret, no compilation\n");
    fmt::print("  --debug-ast          Print AST during compilation\n");
    fmt::print("  --debug-ir           Print intermediate representation\n");
    fmt::print("  --debug-asm          Print generated assembly\n");
    fmt::print("  --platfirm           Set platform (z80, ...)\n");
    fmt::print("  -h, --help           Show this help\n");
    fmt::print("  -v, --version        Show version\n");
    
    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nExamples:\n");
    fmt::print("  {} script.soot\n", program_name);
    fmt::print("  {} --target out --source-root src math/add.soot\n", program_name);
    fmt::print("  {} --compile-only --debug-asm program.soot\n", program_name);
    fmt::print("  {} --profile myprofile --no-repl init.soot\n", program_name);
    
    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nExamples:\n");
    fmt::print("  {} script.soot\n", program_name);
    fmt::print("  {} --connect --port 8182 script.soot\n", program_name);
    fmt::print("  {} --target out --source-root src math/add.soot\n", program_name);
    fmt::print("  {} --compile-only --debug-asm program.soot\n", program_name);
    fmt::print("  {} --profile myprofile --no-repl init.soot\n", program_name);
    fmt::print("  {} --connect --wait --reset kernel.soot\n", program_name);

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nREPL Commands:\n");
    fmt::print("  :exit, :quit     Exit the REPL\n");
    fmt::print("  :reload          Reload environment\n");
    fmt::print("  :load <file>     Load and compile a file\n");
    fmt::print("  :help            Show this help\n");
}

CommandLineOptions parse_args(int argc, char* argv[]) {
    CommandLineOptions opts;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--target" && i + 1 < argc) {
            opts.target_dir = argv[++i];
        }
        else if (arg == "--source-root" && i + 1 < argc) {
            opts.source_root = argv[++i];
        }
        else if (arg == "--profile" && i + 1 < argc) {
            opts.user_profile = argv[++i];
        }
        else if (arg == "-o" && i + 1 < argc) {
            opts.output_name = argv[++i];
        }
        else if (arg == "--flat") {
            opts.flat_output = true;
        }
        else if (arg == "--no-repl" || arg == "-n") {
            opts.interactive = false;
        }
        else if (arg == "--compile-only") {
            opts.mode = sootc::CompilerMode::COMPILE_ONLY;
        }
        else if (arg == "--interpret-only") {
            opts.mode = sootc::CompilerMode::INTERPRET_ONLY;
        }
        else if (arg == "--debug-ast") {
            opts.debug_ast = true;
        }
        else if (arg == "--debug-ir") {
            opts.debug_ir = true;
        }
        else if (arg == "--debug-asm") {
            opts.debug_asm = true;
        }
        else if (arg == "-h" || arg == "--help") {
            opts.help = true;
        }
        else if (arg == "-v" || arg == "--version") {
            opts.version = true;
        }
        else if (arg[0] != '-') {
            opts.input_files.push_back(arg);
        }
        // -- Client/Server флаги --
        else if (arg == "--connect" || arg == "-c") {
            opts.connect = true;
        }
        else if (arg == "--port" && i + 1 < argc) {
            opts.port = std::stoi(argv[++i]);
        }
        else if (arg == "--temp-port" && i + 1 < argc) {
            opts.temp_port = std::stoi(argv[++i]);
        }
        else if (arg == "--timeout" && i + 1 < argc) {
            opts.timeout_seconds = std::stoi(argv[++i]);
        }
        else if (arg == "--disconnect") {
            opts.disconnect_after = true;
        }
        else if (arg == "--no-send") {
            opts.no_send = true;
        }
        else if (arg == "--wait") {
            opts.wait_connection = true;
        }
        // Target управление
        else if (arg == "--reset") {
            opts.reset_target = true;
        }
        else if (arg == "--stop") {
            opts.stop_target = true;
        }
        else if (arg == "--resume") {
            opts.resume_target = true;
        }
        else if (arg == "--status") {
            opts.check_status = true;
        }
        // Debug флаги
        else if (arg == "--debug") {
            opts.debug_mode = true;
        }
        else if (arg == "--debug-segment") {
            opts.debug_segment = true;
        }
        else if (arg == "--listen") {
            opts.listen_debugger = true;
        }        
        else {
            fmt::print(stderr, "Unknown option: {}\n", arg);
            opts.help = true;
        }
    }
    
    return opts;
}

int main(int argc, char* argv[]) {
    try {
        auto opts = parse_args(argc, argv);
        
        if (opts.help) { print_help(argv[0]); return 0; }
        if (opts.version) { fmt::print("SOOT Compiler v1.0\n"); return 0; }
        
        // Настройка логирования
        lg::set_file_level(lg::level::info);
        lg::set_stdout_level(lg::level::info);
        lg::set_file("compiler");
        lg::initialize();
        
        // Загрузка конфигурации
        REPL::Config repl_config(opts.platform);
        repl_config.asm_file_search_dirs.push_back(file_util::get_path(file_util::PathType::PROJECT).string());
        repl_config.per_game_history = true;
        repl_config.nrepl_port = opts.port;
        
        auto startup_file = REPL::load_user_startup_file(opts.user_profile, opts.platform);
        
        // Инициализация nREPL
        std::mutex compiler_mutex;
        sootc::ReplStatus status = sootc::ReplStatus::OK;
        
        std::function<bool()> shutdown_callback = [&status]() { 
            return status == sootc::ReplStatus::WANT_EXIT; 
        };
        
        ReplServer repl_server(shutdown_callback, repl_config.get_nrepl_port());
        bool nrepl_ok = repl_server.init_server(true);
        
        std::thread nrepl_thread;
        
        // Создание компилятора
        sootc::Compiler::CompilationOptions comp_options;
        comp_options.mode = sootc::CompilerMode::HYBRID;
        comp_options.debug_print_ir = false;
        comp_options.debug_print_ast = false;
        comp_options.debug_print_asm = false;
        comp_options.user_profile = opts.user_profile;
        comp_options.search_paths = {".", "scripts", "src", "examples"};
        comp_options.mode = opts.mode;

        auto compiler = std::make_unique<sootc::Compiler>(
            opts.platform, 
            comp_options,
            repl_config, 
            opts.user_profile,
            std::make_unique<REPL::Wrapper>(opts.user_profile, repl_config, startup_file, nrepl_ok)
        );
        
        // Запуск nREPL потока
        if (nrepl_ok) {
            nrepl_thread = std::thread([&]() {
                while (!shutdown_callback()) {
                    auto msg = repl_server.get_msg();
                    if (msg) {
                        std::lock_guard<std::mutex> lock(compiler_mutex);
                        status = compiler->handle_repl_string(msg.value());
                        compiler->print_to_repl(compiler->get_prompt());
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        }
        
        // Выполнение startup команд
        for (const auto& cmd : startup_file.run_before_listen) {
            status = compiler->handle_repl_string(cmd);
        }
        
        // Компиляция файлов из командной строки
        for (const auto& input_file : opts.input_files) {
            lg::info("Processing: {}", input_file);
            auto result = compiler->compile_file(input_file);
            if (result) {
                lg::info("Compiled: {}", input_file);
            } else {
                lg::error("Failed: {} - {}", input_file, result.error());
            }
        }

        // Главный REPL цикл
        while (status != sootc::ReplStatus::WANT_EXIT) {
            if (status == sootc::ReplStatus::WANT_RELOAD) {
                lg::info("Reloading compiler...");
                std::lock_guard<std::mutex> lock(compiler_mutex);
                compiler->save_repl_history();
                compiler = std::make_unique<sootc::Compiler>(
                    opts.platform, comp_options, repl_config, opts.user_profile,
                    std::make_unique<REPL::Wrapper>(opts.user_profile, repl_config, startup_file, nrepl_ok)
                );
                status = sootc::ReplStatus::OK;
            }
            
            std::string input = compiler->get_repl_input();
            if (!input.empty()) {
                std::lock_guard<std::mutex> lock(compiler_mutex);
                status = compiler->handle_repl_string(input);
            }
        }
        
        // Очистка
        if (nrepl_ok) {
            repl_server.shutdown_server();
            nrepl_thread.join();
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        lg::error("Fatal error: {}", e.what());
        return 1;
    }
}