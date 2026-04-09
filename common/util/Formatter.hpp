#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <string>

namespace util {

class Formatter {
public:
    static Formatter& instance() {
        static Formatter s_instance;
        return s_instance;
    }

    int get_column() const { return m_column; }
    void set_column(int col) { m_column = col; }
    void inc_column(int delta) { m_column += delta; }
    void reset_column() { m_column = 0; }

    std::string indent() const {
        return std::string(m_column, ' ');
    }

    // Для compile-time строк (шаблон)
    template<typename... Args>
    void print(fmt::format_string<Args...> format, Args&&... args) const {
        fmt::print("{}{}", indent(), fmt::format(format, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void print_no_newline(fmt::format_string<Args...> format, Args&&... args) const {
        fmt::print("{}{}", indent(), fmt::format(format, std::forward<Args>(args)...));
    }

    template<typename... Args>
    std::string format(fmt::format_string<Args...> format, Args&&... args) const {
        return fmt::format("{}{}", indent(), fmt::format(format, std::forward<Args>(args)...));
    }

    // Для runtime-строк (если очень нужно)
    void print_runtime(const std::string& msg) const {
        fmt::print("{}{}", indent(), msg);
    }

    void print_runtime(const char* msg) const {
        fmt::print("{}{}", indent(), msg);
    }

    class Block {
    public:
        Block(Formatter& f, int indent_delta = 2) : m_formatter(f), m_delta(indent_delta) {
            m_formatter.inc_column(m_delta);
        }
        ~Block() {
            m_formatter.inc_column(-m_delta);
        }
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
    private:
        Formatter& m_formatter;
        int m_delta;
    };

private:
    Formatter() = default;
    int m_column = 0;
};

} // namespace util