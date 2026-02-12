#pragma once

#include "common/sooti/Object.hpp"

namespace script {
class EvalException : public std::exception {
  public:
    Object                             form;    // Тот самый объект (Pair или LexToken)
    std::string                        message; // Текст ошибки
    bool                               already_printed = false; // Не печай второй раз
    bool                               error_header_required = true;
    bool                               detailed_error_required = true;
    int                                stack_counter;
    std::shared_ptr<EnvironmentObject> env;

    EvalException(Object f, std::string m) : form(f), message(std::move(m)), stack_counter(0) {}

    // Чтобы соответствовать стандарту std::exception
    const char *what() const noexcept override {
        return message.c_str();
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