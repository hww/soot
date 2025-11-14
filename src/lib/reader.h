#pragma once

#include "object.h"
#include "source-info.h"
#include <memory>
#include <vector>
#include <optional>
#include <fstream>
#include <cstdlib>

// Токен для парсинга
struct Token {
    std::string text;
    std::shared_ptr<SourceText> source;
    int offset;
    int line;
};

// Поток токенов (аналог TextStream)
class TokenStream {
    


public:
    explicit TokenStream(std::shared_ptr<SourceText> source);
    
    char peek();
    char peek(int ahead);
    char read();
    bool has_more() const;
    bool has_more(int ahead) const;
    void skip_whitespace_and_comments();
    
    int get_current_offset() const { return m_position; }
    int get_current_line() const { return m_line; }

    std::shared_ptr<SourceText> get_source() const { return m_source; }

private:
    std::shared_ptr<SourceText> m_source;
    int m_position = 0;
    int m_line = 1;
};

// Главный Reader (аналог goos::Reader)
class Reader {
    friend class TokenStream; 

public:
    Reader();
    
    Object read_from_string(const std::string& code, const std::string& source_name = "string");
    Object read_from_file(const std::string& filename);
    
    SymbolTable& get_symbol_table() { return m_symbols; }
    SourceManager& get_source_manager() { return m_sources; }

    bool is_input_complete(const std::string& code);

private:
    Object read_impl(std::shared_ptr<SourceText> source);
    Object read(TokenStream& tokens);
    Object read_list(TokenStream& tokens);
    Object read_atom(const Token& token);
    Token next_token(TokenStream& tokens);
    
    // Парсеры чисел как в OpenGOAL
    bool try_parse_integer(const Token& token, Object& result);
    bool try_parse_float(const Token& token, Object& result);
    bool try_parse_hex(const Token& token, Object& result);
    bool try_parse_binary(const Token& token, Object& result);
    bool try_parse_boolean(const Token& token, Object& result);
    bool try_parse_char(const Token& token, Object& result);
    
    void link_object(const Object& obj, const Token& token);

    void throw_reader_error(TokenStream& tokens, const std::string& error);
    void throw_reader_error(const Token& token, const std::string& error);    

    SymbolTable m_symbols;
    SourceManager m_sources;
};