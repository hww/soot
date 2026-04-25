#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>

namespace util {
#include <string>
#include <string_view>
#include <fmt/core.h>
#include <fmt/format.h>

// ==================== Базовый интерфейс ====================

class IFormatter {
public:
    virtual ~IFormatter() = default;
    
    virtual int get_column() const = 0;
    virtual void set_column(int col) = 0;
    virtual void inc_column(int delta) = 0;
    virtual void reset_column() = 0;
    
    virtual std::string indent() const = 0;
    
    // Форматирование с отступом
    virtual std::string format(std::string_view msg) const = 0;
    
    template<typename... Args>
    std::string format(std::string_view fmt_str, Args... args) const {
        return indent() + fmt::vformat(fmt_str, fmt::make_format_args(args...));
    }
    
    template<typename... Args>
    std::string format_inline(std::string_view fmt_str, Args... args) const {
        return fmt::vformat(fmt_str, fmt::make_format_args(args...));
    }

    // Основные методы вывода/накопления
    virtual void output(std::string_view msg) = 0;
    
    template<typename... Args>
    void output(std::string_view fmt_str, Args&&... args) {
        output(indent());
        std::string formatted = fmt::vformat(fmt_str, fmt::make_format_args(args...));
        output(formatted);
    }
    
    template<typename... Args>
    void output_inline(std::string_view fmt_str, Args&&... args) {
        std::string formatted = fmt::vformat(fmt_str, fmt::make_format_args(args...));
        output(formatted);
    }

    virtual void output_runtime(std::string_view msg) = 0;
    
    // RAII блок для изменения отступа
    class Block {
    public:
        Block(IFormatter& f, int indent_delta = 2) : m_formatter(f), m_delta(indent_delta) {
            m_formatter.inc_column(m_delta);
        }
        ~Block() {
            m_formatter.inc_column(-m_delta);
        }
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
    private:
        IFormatter& m_formatter;
        int m_delta;
    };
};

// ==================== Класс для печати ====================

class OutputFormatter : public IFormatter {
public:
    OutputFormatter() {}

    int get_column() const override { return m_column; }
    void set_column(int col) override { m_column = col; }
    void inc_column(int delta) override { m_column += delta; }
    void reset_column() override { m_column = 0; }
    
    std::string indent() const override {
        return std::string(m_column, ' ');
    }
    
    std::string format(std::string_view msg) const override {
        return indent() + std::string(msg);
    }
    
    // Вывод на печать
    void output(std::string_view msg) override {
        fmt::print("{}", msg);
    }
    
    void output_runtime(std::string_view msg) override {
        fmt::print("{}{}", indent(), msg);
    }
    
    // Дополнительный метод для печати с автоматическим переводом строки
    void output_line(std::string_view msg) {
        output(msg);
        output("\n");
    }
    
    template<typename... Args>
    void output_line(std::string_view fmt_str, Args&&... args) {
        output(fmt_str, std::forward<Args>(args)...);
        output("\n");
    }
    
private:
    int m_column = 0;
};

// ==================== Класс для накопления в строке ====================

class StringBuilderFormatter : public IFormatter {
public:
    StringBuilderFormatter() {}

    int get_column() const override { return m_column; }
    void set_column(int col) override { m_column = col; }
    void inc_column(int delta) override { m_column += delta; }
    void reset_column() override { m_column = 0; }
    
    std::string indent() const override {
        return std::string(m_column, ' ');
    }
    
    std::string format(std::string_view msg) const override {
        return indent() + std::string(msg);
    }
    
    // Накопление в буфер
    void output(std::string_view msg) override {
        m_buffer.append(msg);
    }
    
    void output_runtime(std::string_view msg) override {
        m_buffer.append(indent());
        m_buffer.append(msg);
    }
    
    // Дополнительный метод для добавления с переводом строки
    void output_line(std::string_view msg) {
        output(msg);
        output("\n");
    }
    
    template<typename... Args>
    void output_line(std::string_view fmt_str, Args&&... args) {
        output(fmt_str, std::forward<Args>(args)...);
        output("\n");
    }
    
    // Получение результата
    std::string get_result() const {
        return m_buffer;
    }
    
    void clear() {
        m_buffer.clear();
        reset_column();
    }
    
private:
    std::string m_buffer;
    int m_column = 0;
};

} // namespace util