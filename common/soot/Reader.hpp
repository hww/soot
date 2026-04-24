#pragma once

#include "common/soot/Object.hpp"
#include "TextDb.hpp"
#include "repl/repl_wrapper.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace soot {

class Interpreter;

struct TextStream {
    explicit TextStream(std::shared_ptr<SourceText> ptr) {
        text = std::move(ptr);
    }

    std::shared_ptr<SourceText> text;
    int                         seek = 0;
    int                         line_count = 0;
    bool                        has_bom; // Файл содержит UTF8 BOM

    char peek() {
        ASSERT(seek < text->get_size());
        return text->get_text()[seek];
    }

    char peek(int ahead) {
        ASSERT(seek + ahead < text->get_size());
        return text->get_text()[seek + ahead];
    }

    char read() {
        ASSERT(seek < text->get_size());
        char c = text->get_text()[seek++];
        if (c == '\n')
            line_count++;
        return c;
    }

    bool text_remains() {
        return seek < text->get_size();
    }
    bool text_remains(int ahead) {
        return seek + ahead < text->get_size();
    }
    void seek_past_whitespace_and_comments();
    bool read_utf8_encoding(bool throw_on_error);
};

struct Token {
    std::string                 text;
    std::shared_ptr<SourceText> source_text;
    int                         source_offset;
    int                         source_line;
};

struct ReaderEvent {
    enum Type {
        FORM_READ,    // Прочитано целое выражение (топ-левел)
        MACRO_REQUEST // Встречен функциональный макрос внутри списка
    } type;

    Object form;   // Лямбда макроса ИЛИ прочитанная форма
    Object reader; // Поток, чтобы макрос мог дочитать данные
    Object token;  // Сам токен макроса (например, "#")
};
// Тип коллбэка для инкрементального выполнения
using EvalCallback = std::function<Object(const ReaderEvent &)>;

class Reader {
    struct ReaderMacro {
        std::string shortcut;
        // Call lambda if defined
        Object lambda;
        // Replate to string if there are no lambda
        std::string replacement;
        // Wrap to extression (replacement nex)
        bool list;
    };

    struct MacroInContext {
        const ReaderMacro          *macro;
        std::shared_ptr<SourceText> src;
        int                         offset;
    };

  public:
    Reader();

    // ТОЧНО как у них:
    Object read_single_form(TextStream &ts, EvalCallback eval_callback = nullptr);
    Object read_from_string(const std::string &str, bool add_top_level = true,
                            const std::optional<std::string> &string_name = std::nullopt,
                            EvalCallback                      eval_callback = nullptr);
    Object read_from_file(const std::vector<std::string> &file_path, bool check_encoding = true,
                          bool add_top_level = true, EvalCallback eval_callback = nullptr);
    Object read_one(TextStream &ts);
    // REPL метод (если нужен):
    std::optional<Object> read_from_stdin(const std::string& prompt, REPL::Wrapper& repl);

    TextDb &get_db() {
        return m_db;
    }

    // Проверка завершения
    bool is_expression_complete(const std::string &code);

    const ReaderMacro *find_reader_macro(const std::string &shortcut) const;
    void add_reader_macro(const std::string &shortcut, std::string replacement, bool list = true);
    void add_reader_macro(const std::string &shortcut, Object lambda, bool list = true);
    void remove_reader_macro(const std::string &shortcut);
    void throw_reader_error(TextStream &here, const std::string &err, int seek_offset = 0);

    Object read_list(TextStream &ts, bool expect_close_paren = true, std::string terminator = ")",
                     EvalCallback eval_callback = nullptr);
    SymbolTable& symbol_table() { return m_symbols; }
  private:
    // Внутренние методы как у них:
    Object internal_read(std::shared_ptr<SourceText> text, bool check_encoding,
                         bool add_top_level = true, EvalCallback eval_callback = nullptr);

    Token get_next_token(TextStream &stream);
    bool  read_object(Token &tok, TextStream &ts, Object &obj);

    // Парсеры чисел:
    bool try_token_as_integer(const Token &tok, Object &obj);
    bool try_token_as_hex(const Token &tok, Object &obj);
    bool try_token_as_binary(const Token &tok, Object &obj);
    bool try_token_as_float(const Token &tok, Object &obj);
    bool try_token_as_char(const Token &tok, Object &obj);
    bool try_token_as_symbol(const Token &tok, Object &obj);
    bool read_string(TextStream &stream, Object &obj);
    bool read_array(TextStream &stream, Object &obj);

    bool is_expression_complete_impl(TextStream &ts);

    SymbolTable                                  m_symbols; // как у них - symbolTable, не m_symbols
    TextDb                                       m_db;      // как у них - db, не m_db
    bool                                         m_valid_symbols_chars[256];
    std::unordered_map<std::string, ReaderMacro> m_reader_macros;
};

} // namespace soot
