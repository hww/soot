#pragma once

#include "common/sooti/Object.hpp"

namespace script {

struct ErrorFrame {
    Object      form; // Форма (контекст) этого уровня
    std::string note; // Пояснение: "Inside macro expansion" или "While assembling..."
    std::shared_ptr<EnvironmentObject> env;
};

class EvalException : public std::exception {
  public:
    Object      primary_form; // Изначальный виновник
    std::string original_msg; // Первичное сообщение ("Not a pair")
    uint        stack_counter;
    // Цепочка контекстов (от глубокого к верхнему)
    std::vector<ErrorFrame> trace;

    bool already_printed = false;

    EvalException(Object f, std::string m)
        : primary_form(f), original_msg(std::move(m)), stack_counter(0) {
        // Сразу добавляем первый кадр
        trace.push_back({f, "Origin"});
    }

    // Метод для добавления "этажа" информации
    void add_context(Object f, std::string note) {
        trace.push_back({f, std::move(note)});
    }

    const char *what() const noexcept override {
        return original_msg.c_str(); // Для совместимости
    }
};

class ExitException : public std::exception {
  public:
    int         exit_code;
    std::string message; // Храним строку здесь

    explicit ExitException(int code = 0)
        : exit_code(code), message(fmt::format("Exit with code {}", code)) {}

    const char *what() const noexcept override {
        return message.c_str(); // Теперь это безопасно
    }
};
} // namespace script