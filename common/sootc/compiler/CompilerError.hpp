#pragma once

#include "fmt/format.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace sootc {

    // -------------------------------------------------------------------------------------------
    // 
    // СОГЛАШЕНИЕ
    //
    // 1. Внутренние ошибки(баг компилятора, неверное состояние) → CompilerError.
    // 
    // 2. Пользовательские ошибки(неизвестный символ, не тот тип) → throw_compiler_error(form, ...) 
    // в Compiler, как у нас уже есть.
    // 
    // 3. Инварианты в горячем коде(assert, unreachable) — оставляем как std::logic_error или assert,
    // они не для пользователя и не для отладки компилятора,
    // а для отлова программистских ошибок в C++.
    // -------------------------------------------------------------------------------------------
    // 
    // Единый формат для внутренних ошибок компилятора (баги в самом компиляторе,
    // не ошибки пользователя).
    //
    // Использование:
    //   throw CompilerError("FileNode::generate")
    //       .where("repl file '<repl>'")
    //       .expected("at least one FunctionNode")
    //       .got("1 child: ConstNode");
    //
    class CompilerError : public std::runtime_error {
    public:
        explicit CompilerError(std::string where)
            : std::runtime_error(""), m_where(std::move(where)) {}

        CompilerError &where(std::string s) {
            m_details.emplace_back("where", std::move(s));
            return *this;
        }
        CompilerError &expected(std::string s) {
            m_details.emplace_back("expected", std::move(s));
            return *this;
        }
        CompilerError &got(std::string s) {
            m_details.emplace_back("got", std::move(s));
            return *this;
        }
        CompilerError &note(std::string s) {
            m_details.emplace_back("note", std::move(s));
            return *this;
        }

        const char *what() const noexcept override {
            build_message();
            return m_message.c_str();
        }

    private:
        void build_message() const {
            if (!m_message.empty()) return;
            m_message = m_where;
            for (const auto &[k, v] : m_details) {
                m_message += "\n  ";
                m_message += k;
                m_message += ':';
                for (size_t i = k.size() + 1; i < 10; ++i) m_message += ' ';
                m_message += v;
            }
        }

        std::string                                      m_where;
        std::vector<std::pair<std::string, std::string>> m_details;
        mutable std::string                              m_message;
    };

} // namespace sootc