#include "token.h"
#include "tokenizer.h"
#include "source-info.h"
#include <cctype>
#include <stdexcept>

Tokenizer::Tokenizer(const std::string& input, const std::string& filename)
    : source(input), pos(0), line(1), line_start_offset(0), 
      current_filename(filename) {}

void Tokenizer::skip_whitespace() {
    while (std::isspace(current())) {
        advance();
    }
}

void Tokenizer::skip_comment() {
    if (current() == ';') {
        while (current() != '\n' && current() != '\0') {
            advance();
        }
    }
}

Token Tokenizer::parse_symbol() {
    size_t start_pos = pos;
    while (current() != '\0' && !std::isspace(current()) &&
           current() != '(' && current() != ')' &&
           current() != '\'' && current() != '`' &&
           current() != ',' && current() != ';' &&
           current() != '"') {
        advance();
    }
    return {TokenType::SYMBOL, 
            std::string_view(source.data() + start_pos, pos - start_pos),
            {start_pos, line}};
}

Token Tokenizer::parse_number() {
    size_t start_pos = pos;
    bool has_dot = false;
    
    while (std::isdigit(current()) || current() == '.') {
        if (current() == '.') {
            if (has_dot) break;
            has_dot = true;
        }
        advance();
    }
    
    return {TokenType::NUMBER,
            std::string_view(source.data() + start_pos, pos - start_pos),
            {start_pos, line}};
}

Token Tokenizer::parse_string() {
    size_t start_pos = pos;
    size_t start_line = line;
    
    advance();  // Пропускаем открывающую кавычку
    
    while (current() != '"' && current() != '\0') {
        if (current() == '\\') {
            advance();  // Экранирование
        }
        advance();
    }
    
    if (current() == '"') {
        advance();  // Пропускаем закрывающую кавычку
        Token token = {TokenType::STRING, 
                      std::string_view(source.data() + start_pos + 1, pos - start_pos - 2),
                      {start_pos, start_line}};
        return token;
    } else {
        // СОЗДАЕМ токен ошибки и сразу сохраняем SourceInfo
        Token error_token = {TokenType::STRING, 
                           std::string_view(source.data() + start_pos, pos - start_pos),
                           {start_pos, start_line}};
        
        link_token_source(error_token, {start_pos, start_line});
        throw std::runtime_error("Unterminated string");
    }
}

Token Tokenizer::parse_boolean() {
    size_t start_pos = pos;
    advance();
    
    if (current() == 't' || current() == 'T') {
        advance();
        return {TokenType::BOOLEAN, std::string_view("#t"), {start_pos, line}};
    } else if (current() == 'f' || current() == 'F') {
        advance();
        return {TokenType::BOOLEAN, std::string_view("#f"), {start_pos, line}};
    }
    
    pos = start_pos;
    return parse_symbol();
}

Token Tokenizer::next_token() {
    skip_whitespace();
    skip_comment();
    skip_whitespace();
    
    SourcePos start = current_pos();
    
    if (current() == '\0') {
        Token token = {TokenType::EOF_TOKEN, std::string_view(""), start};
        // Сохраняем информацию об исходнике для токена
        g_source_db.link(&token, SourceInfo(current_filename, start.line, 
                                           compute_column(start), get_current_line()));
        return token;
    }
    
    Token token;
    switch(current()) {
        case '(': 
            advance();
            token = {TokenType::LPAREN, std::string_view("("), start};
            break;
        case ')': 
            advance();
            token = {TokenType::RPAREN, std::string_view(")"), start};
            break;
        case '\'': 
            advance();
            token = {TokenType::QUOTE, std::string_view("'"), start};
            break;
        case '`': 
            advance();
            token = {TokenType::BACKQUOTE, std::string_view("`"), start};
            break;
        case ',': 
            if (peek() == '@') {
                advance(); advance();
                token = {TokenType::COMMA_AT, std::string_view(",@"), start};
            } else {
                advance();
                token = {TokenType::COMMA, std::string_view(","), start};
            }
            break;
        case '"': 
            token = parse_string();
            break;
        case '#': 
            token = parse_boolean();
            break;
        default:
            if (std::isdigit(current()) || 
                (current() == '.' && std::isdigit(peek()))) {
                token = parse_number();
            } else {
                token = parse_symbol();
            }
            break;
    }
    // Сохраняем информацию об исходнике для каждого токена
    std::string current_line = get_current_line();
    g_source_db.link(&token, SourceInfo(current_filename, start.line, 
                                       compute_column(start), current_line));
    
    return token;
}

std::string Tokenizer::get_current_line_at_offset(size_t offset) const {
    // Находим начало и конец строки содержащей указанное смещение
    size_t line_start = source.rfind('\n', offset);
    if (line_start == std::string::npos) line_start = 0;
    else line_start++; // пропускаем \n
    
    size_t line_end = source.find('\n', offset);
    if (line_end == std::string::npos) line_end = source.length();
    
    return source.substr(line_start, line_end - line_start);
}

void Tokenizer::link_token_source(const Token& token, SourcePos start) {
    std::string current_line = get_current_line_at_offset(start.offset);
    g_source_db.link(&token, SourceInfo(current_filename, start.line, 
                                       compute_column(start), current_line));
}

// Вспомогательные методы для отладки
std::string Tokenizer::get_current_line() const {
    // Находим начало и конец текущей строки
    size_t line_start = source.rfind('\n', pos);
    if (line_start == std::string::npos) line_start = 0;
    else line_start++; // пропускаем \n
    
    size_t line_end = source.find('\n', pos);
    if (line_end == std::string::npos) line_end = source.length();
    
    return source.substr(line_start, line_end - line_start);
}

int Tokenizer::compute_column(SourcePos pos) const {
    // Находим начало строки и вычисляем колонку
    size_t line_start = source.rfind('\n', pos.offset);
    if (line_start == std::string::npos) line_start = 0;
    else line_start++;
    
    return pos.offset - line_start;
}