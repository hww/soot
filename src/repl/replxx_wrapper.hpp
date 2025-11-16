// src/repl/replxx_wrapper.hpp
#pragma once
#include "replxx.hxx"
#include <string>

class ReplxxWrapper {
private:
    replxx::Replxx rx_;

public:
    ReplxxWrapper() {
        setup_replxx();
    }

    std::string read_input(const std::string& prompt) {
        auto result = rx_.input(prompt);
        return result ? std::string(result) : "";
    }

    void add_to_history(const std::string& command) {
        if (!command.empty()) {
            rx_.history_add(command);
        }
    }

    void load_history(const std::string& history_file) {
        rx_.history_load(history_file);
    }

    void save_history(const std::string& history_file) {
        rx_.history_save(history_file);
    }

private:
    void setup_replxx() {
        // Настройка ReplXX
        rx_.set_max_history_size(1000);
        rx_.set_word_break_characters(" \t");
        rx_.set_complete_on_empty(true);
        rx_.set_indent_multiline(false);
        rx_.set_beep_on_ambiguous_completion(false);
    }
};