#pragma once

#include "common/sooti/Object.hpp"

namespace script {
class EvalException : public std::exception {
  public:
    Object form;                  // Тот самый объект (Pair или LexToken)
    std::string message;          // Текст ошибки
    bool already_printed = false; // Не печай второй раз
    bool error_header_required = true;
    bool detailed_error_required = true;

    EvalException(Object f, std::string m) : form(f), message(std::move(m)) {}

    // Чтобы соответствовать стандарту std::exception
    const char *what() const noexcept override {
        return message.c_str();
    }
};

} // namespace script