#include "token.h"
#include "tokenizer.h"
#include <cctype>
#include <stdexcept>

Tokenizer::Tokenizer(const std::string& input) 
    : source(input), pos(0), line(1), line_start_offset(0) {}

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
    advance();  // Пропускаем открывающую кавычку
    
    while (current() != '"' && current() != '\0') {
        if (current() == '\\') {
            advance();
        }
        advance();
    }
    
    if (current() == '"') {
        advance();
    } else {
        throw std::runtime_error("Unterminated string");
    }
    
    return {TokenType::STRING,
            std::string_view(source.data() + start_pos + 1, pos - start_pos - 2),
            {start_pos, line}};
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
        return {TokenType::EOF_TOKEN, std::string_view(""), start};
    }
    
    switch(current()) {
        case '(': 
            advance();
            return {TokenType::LPAREN, std::string_view("("), start};
        case ')': 
            advance();
            return {TokenType::RPAREN, std::string_view(")"), start};
        case '\'': 
            advance();
            return {TokenType::QUOTE, std::string_view("'"), start};
        case '`': 
            advance();
            return {TokenType::BACKQUOTE, std::string_view("`"), start};
        case ',': 
            if (peek() == '@') {
                advance(); advance();
                return {TokenType::COMMA_AT, std::string_view(",@"), start};
            } else {
                advance();
                return {TokenType::COMMA, std::string_view(","), start};
            }
        case '"': 
            return parse_string();
        case '#': 
            return parse_boolean();
        default:
            if (std::isdigit(current()) || 
                (current() == '.' && std::isdigit(peek()))) {
                return parse_number();
            } else {
                return parse_symbol();
            }
    }
}