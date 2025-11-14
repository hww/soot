#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct CommandLineOptions {
    // Input
    std::string filename;
    std::string expression;
    
    // Output targets
    struct {
        bool tokens = false;
        bool ast = false;
        bool bytecode = false;
        bool string_info = false;
    } print;
    
    // Output files (empty = stdout)
    std::string tokens_file;
    std::string ast_file;
    std::string bytecode_file;
    
    // Execution mode
    enum class Mode { COMPILE, RUN, EVAL };
    Mode mode = Mode::COMPILE;
    
    // Flags
    bool help = false;
    bool has_expression = false;
    
    // Debug method
    std::string mode_to_string() const {
        switch(mode) {
            case Mode::COMPILE: return "COMPILE";
            case Mode::RUN: return "RUN"; 
            case Mode::EVAL: return "EVAL";
            default: return "UNKNOWN";
        }
    }
};

class CommandLineParser {
public:
    CommandLineParser(int argc, char* argv[]);
    
    CommandLineOptions parse();
    void print_help(const std::string& program_name);
    
private:
    std::vector<std::string> args_;
    size_t current_arg_ = 0;
    
    std::string next_arg();
    bool has_next_arg() const;
};