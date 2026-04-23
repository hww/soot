#pragma once

#include "common/soot/Object.hpp"

namespace soot {

class Reader;

struct ErrorFrame {
    Object      form;    // Форма (контекст) этого уровня
    std::string message; // Пояснение: "Inside macro expansion" или "While assembling..."
    std::shared_ptr<EnvironmentObject> env;
    bool                               show_details;
};

class EvalException : public std::exception {
  public:
    Object                             form;    // Изначальный виновник
    std::string                        message; // Первичное сообщение ("Not a pair")
    std::shared_ptr<EnvironmentObject> env;
    bool                               already_printed = false;
    uint                               stack_counter;
    // Цепочка контекстов (от глубокого к верхнему)
    std::vector<ErrorFrame> trace;

    EvalException(Object form, std::string message,
                  std::shared_ptr<EnvironmentObject> env = nullptr)
        : form(form), message(std::move(message)), env(env), stack_counter(0) {}

    // добавить сообщение для стека
    void add_context(Object form, std::string message, std::shared_ptr<EnvironmentObject> env,
                     bool show_details = false) {
        trace.push_back({form, std::move(message), env, show_details});
    }
    // Базовое сообщение
    const char *what() const noexcept override {
        return message.c_str();
    }

    // Основной метод для получения красивого отчета
    std::string full_report(Reader &reader) const;

  private:
    std::string format_env_vars(const std::shared_ptr<EnvironmentObject> &env) const;
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
} // namespace soot