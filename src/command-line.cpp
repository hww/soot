#include "command-line.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

CommandLineParser::CommandLineParser(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        args_.push_back(argv[i]);
    }
}

CommandLineOptions CommandLineParser::parse() {
    CommandLineOptions options;
    bool mode_explicitly_set = false;
    
    while (has_next_arg()) {
        std::string arg = next_arg();
        
        if (arg == "-h" || arg == "--help") {
            options.help = true;
        }
        else if (arg == "-e" || arg == "--expression") {
            if (!options.expression.empty()) {
                throw std::runtime_error("Multiple expressions specified");
            }
            if (!options.filename.empty()) {
                throw std::runtime_error("Cannot specify both file and expression");
            }
            if (!has_next_arg()) {
                throw std::runtime_error("Expected expression after -e");
            }
            options.expression = next_arg();
            options.has_expression = true;
        }
        else if (arg == "--compile") {
            options.mode = CommandLineOptions::Mode::COMPILE;
            mode_explicitly_set = true;
        }
        else if (arg == "--run") {
            options.mode = CommandLineOptions::Mode::RUN;
            mode_explicitly_set = true;
        }
        else if (arg == "--eval") {
            options.mode = CommandLineOptions::Mode::EVAL;
            mode_explicitly_set = true;
        }
        else if (arg == "--tokens") {
            options.print.tokens = true;
            if (has_next_arg()) {
                std::string next = args_[current_arg_];
                if (!next.empty() && next[0] != '-') {
                    options.tokens_file = next_arg();
                }
            }
        }
        else if (arg == "--ast") {
            options.print.ast = true;
            if (has_next_arg()) {
                std::string next = args_[current_arg_];
                if (!next.empty() && next[0] != '-') {
                    options.ast_file = next_arg();
                }
            }
        }
        else if (arg == "--bytecode") {
            options.print.bytecode = true;
            if (has_next_arg()) {
                std::string next = args_[current_arg_];
                if (!next.empty() && next[0] != '-') {
                    options.bytecode_file = next_arg();
                }
            }
        }
        else if (arg == "--string-info") {
            options.print.string_info = true;
        }
        else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        }
        else {
            // Positional argument - filename
            if (!options.filename.empty()) {
                throw std::runtime_error("Multiple input files specified");
            }
            if (!options.expression.empty()) {
                throw std::runtime_error("Cannot specify both file and expression");
            }
            options.filename = arg;
        }
    }
    
    // УСТАНАВЛИВАЕМ РЕЖИМЫ ПО УМОЛЧАНИЮ ТОЛЬКО ЕСЛИ РЕЖИМ НЕ БЫЛ ЯВНО УКАЗАН
    if (!mode_explicitly_set) {
        if (options.has_expression) {
            options.mode = CommandLineOptions::Mode::EVAL;  // Выражения по умолчанию интерпретируются
        } else {
            options.mode = CommandLineOptions::Mode::RUN;   // Файлы по умолчанию выполняются
        }
    }
    
    // ЕСЛИ ЕСТЬ ОТЛАДОЧНЫЙ ВЫВОД, ПРИНУДИТЕЛЬНО ВКЛЮЧАЕМ COMPILE MODE
    // (но только если не указан явно EVAL режим)
    if ((options.print.tokens || options.print.ast || options.print.bytecode || options.print.string_info) &&
        options.mode != CommandLineOptions::Mode::EVAL) {
        options.mode = CommandLineOptions::Mode::COMPILE;
    }
    
    return options;
}

void CommandLineParser::print_help(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] [filename]\n"
              << "       " << program_name << " [options] -e \"expression\"\n\n"
              
              << "Options:\n"
              << "  -h, --help              Show this help message\n\n"
              
              << "Input modes (mutually exclusive):\n"
              << "  -e, --expression EXPR   Evaluate expression\n"
              << "  filename                 Read and execute file\n\n"
              
              << "Output control:\n"
              << "  --tokens [filename]     Print tokens to screen or file\n"
              << "  --ast [filename]        Print AST to screen or file\n"
              << "  --bytecode [filename]   Print bytecode to screen or file\n"
              << "  --string-info           Print string database info\n\n"
              
              << "Execution mode:\n"
              << "  --compile               Compile only (auto-set with debug output)\n"
              << "  --run                   Compile and execute (default for files)\n"
              << "  --eval                  Interpret only (default for expressions)\n\n"
              
              << "Examples:\n"
              << "  " << program_name << " program.scm           # Run file\n"
              << "  " << program_name << " -e \"(+ 1 2)\"         # Evaluate expression\n"
              << "  " << program_name << " --compile -e \"(* 3 4)\" # Compile expression only\n"
              << "  " << program_name << " --tokens file.scm     # Show tokens\n"
              << "  " << program_name << " --ast --bytecode -e EXPR # Show AST and bytecode\n";
}

std::string CommandLineParser::next_arg() {
    if (!has_next_arg()) {
        throw std::runtime_error("Expected argument after option");
    }
    return args_[current_arg_++];
}

bool CommandLineParser::has_next_arg() const {
    return current_arg_ < args_.size();
}