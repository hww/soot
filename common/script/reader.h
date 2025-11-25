#pragma once

#include "object.h"
#include "text_db.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace script {

    struct TextStream {
        explicit TextStream(std::shared_ptr<SourceText> ptr) {
            text = std::move(ptr);
        }

        std::shared_ptr<SourceText> text;
        int seek = 0;
        int line_count = 0;

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

        bool text_remains() { return seek < text->get_size(); }
        bool text_remains(int ahead) { return seek + ahead < text->get_size(); }
        void seek_past_whitespace_and_comments();
        void read_utf8_encoding(bool throw_on_error);
    };

    struct Token {
        std::string text;
        std::shared_ptr<SourceText> source_text;
        int source_offset;
        int source_line;
    };

    class Reader {
    public:
        Reader();

        // ТОЧНО как у них:
        Object read_from_string(const std::string& str,
            bool add_top_level = true,
            const std::optional<std::string>& string_name = std::nullopt);

        Object read_from_file(const std::vector<std::string>& file_path, bool check_encoding = true, bool add_top_level = true);

        // REPL метод (если нужен):
        //std::optional<Object> read_from_stdin(const std::string& prompt, REPL::Wrapper& repl);

        SymbolTable& get_symbol_table() { return symbolTable; }
        TextDb& get_db() { return db; }

        // Проверка завершения
        bool is_expression_complete(const std::string& code);
    private:
        // Внутренние методы как у них:
        Object internal_read(std::shared_ptr<SourceText> text,
            bool check_encoding,
            bool add_top_level = true);

        Object read_list(TextStream& ts, bool expect_close_paren = true);
        Token get_next_token(TextStream& stream);
        bool read_object(Token& tok, TextStream& ts, Object& obj);

        // Парсеры чисел:
        bool try_token_as_integer(const Token& tok, Object& obj);
        bool try_token_as_hex(const Token& tok, Object& obj);
        bool try_token_as_binary(const Token& tok, Object& obj);
        bool try_token_as_float(const Token& tok, Object& obj);
        bool try_token_as_char(const Token& tok, Object& obj);
        bool try_token_as_symbol(const Token& tok, Object& obj);
        bool read_string(TextStream& stream, Object& obj);
        bool read_array(TextStream& stream, Object& obj);

        void add_reader_macro(const std::string& shortcut, std::string replacement);
        void throw_reader_error(TextStream& here, const std::string& err, int seek_offset = 0);

        bool is_expression_complete_impl(TextStream& ts);

        SymbolTable symbolTable;  // как у них - symbolTable, не m_symbols
        TextDb db;                // как у них - db, не m_db
        bool m_valid_symbols_chars[256];
        std::unordered_map<std::string, std::string> m_reader_macros;
    };


    // =================== List Builder ===================

    struct ListBuilder {
        Object head;
        std::shared_ptr<PairObject> prev_tail;
        std::shared_ptr<PairObject> tail;
        int size = 0;

        ListBuilder() { head = Object::make_empty_list(); }

        void push_back(Object&& o) {
            size++;
            if (!tail) {
                tail = std::make_shared<PairObject>(o, Object{});
                head.type = ObjectType::PAIR;
                head.heap_obj = tail;
            }
            else {
                auto next = std::make_shared<PairObject>(o, Object{});
                tail->cdr.type = ObjectType::PAIR;
                tail->cdr.heap_obj = next;
                prev_tail = std::move(tail);
                tail = std::move(next);
            }
        }

        Object pop_back() {
            auto obj = tail->car;
            tail = std::move(prev_tail);
            return obj;
        }

        void finalize() {
            if (tail) {
                tail->cdr = Object::make_empty_list();
            }
            else {
                head = Object::make_empty_list();
            }
        }
    };

} // namespace script