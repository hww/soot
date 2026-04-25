#pragma once

#include <fmt/format.h>
#include <fmt/core.h>
#include <string>
#include <string_view>

namespace util {

// ==================== Базовый интерфейс ====================

class IFormatter {
public:
    virtual ~IFormatter() = default;
    
    virtual int get_column() const = 0;
    virtual void set_column(int col) = 0;
    virtual void inc_column(int delta) = 0;
    virtual void reset_column() = 0;
    
    virtual std::string indent() const = 0;
    
    // format - возвращает строку (не печатает)
    virtual std::string format(std::string_view msg) const = 0;
    
    template<typename... Args>
    std::string format(std::string_view fmt_str, Args... args) const {
        return indent() + fmt::vformat(fmt_str, fmt::make_format_args(args...));
    }
    
    template<typename... Args>
    std::string format_inline(std::string_view fmt_str, Args... args) const {
        return fmt::vformat(fmt_str, fmt::make_format_args(args...));
    }
    
    // print - печатает/накапливает
    template<typename... Args>
    void print(std::string_view fmt_str, Args... args) {
        do_print(indent() + fmt::vformat(fmt_str, fmt::make_format_args(args...)));
    }
    
    template<typename... Args>
    void print_inline(std::string_view fmt_str, Args... args) {
        do_print(fmt::vformat(fmt_str, fmt::make_format_args(args...)));
    }
    
    void print_raw(std::string_view msg) {
        do_print(std::string(msg));
    }
    
    template<typename... Args>
    void println(std::string_view fmt_str, Args... args) {
        print(fmt_str, args...);
        print_raw("\n");
    }
    
    // RAII блок
    class Block {
    public:
        Block(IFormatter& f, int delta = 2) : m_formatter(f), m_delta(delta) {
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
    
protected:
    virtual void do_print(const std::string& msg) = 0;
};

// ==================== Печать в консоль ====================

class OutputFormatter : public IFormatter {
public:
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
    
protected:
    void do_print(const std::string& msg) override {
        fmt::print("{}", msg);
    }
    
private:
    int m_column = 0;
};

// ==================== Накопление в строку ====================

class StringBuilderFormatter : public IFormatter {
public:
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
    
    std::string get_result() const { return m_buffer; }
    void clear() { m_buffer.clear(); reset_column(); }
    
protected:
    void do_print(const std::string& msg) override {
        m_buffer += msg;
    }
    
private:
    std::string m_buffer;
    int m_column = 0;
};

} // namespace util