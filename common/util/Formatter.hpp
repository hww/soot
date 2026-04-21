#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>

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

    // 1. Для простых строк без форматирования (самый быстрый путь)
    std::string format(std::string_view msg) const {
        return indent() + std::string(msg);
    }

    // 2. Универсальный шаблон для форматирования. 
    // Используем vformat, чтобы избежать проблем с дедукцией типов вложенных шаблонов fmt.
    template<typename... Args>
    std::string format(std::string_view fmt_str, Args... args) const { 
        // Убрали &&, теперь аргументы передаются по значению или копируются.
        // Для тяжелых объектов это чуть медленнее, но для примитивов (int, float, sid64) 
        // это убирает все проблемы со ссылками на члены структур.
        return indent() + fmt::vformat(fmt_str, fmt::make_format_args(args...));
    }

    // 2. Универсальный шаблон для форматирования. 
    // Используем vformat, чтобы избежать проблем с дедукцией типов вложенных шаблонов fmt.
    template<typename... Args>
    std::string format_print(std::string_view fmt_str, Args... args) const { 
        // Убрали &&, теперь аргументы передаются по значению или копируются.
        // Для тяжелых объектов это чуть медленнее, но для примитивов (int, float, sid64) 
        // это убирает все проблемы со ссылками на члены структур.
        auto result = indent() + fmt::vformat(fmt_str, fmt::make_format_args(args...));
        fmt::print("{}", result);
        return result;
    }

    // 3. Методы print
    template<typename... Args>
    void print(std::string_view fmt_str, Args&&... args) const {
        fmt::print("{}", indent());
        fmt::vprint(fmt_str, fmt::make_format_args(args...));
    }

    // Для вывода заранее отформатированной строки
    void print_runtime(std::string_view msg) const {
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