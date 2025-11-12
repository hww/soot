#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <string_view>

enum class TokenType {
    EOF_TOKEN,
    SYMBOL,
    NUMBER,
    STRING,
    LPAREN,     // (
    RPAREN,     // )
    QUOTE,      // '
    BACKQUOTE,  // `
    COMMA,      // ,
    COMMA_AT,   // ,@
    BOOLEAN     // #t #f
};

struct SourcePos {
    size_t offset;
    size_t line;
};

struct Token {
    TokenType type;
    std::string_view text;
    SourcePos start;
    
    // Для отладки - реализация прямо в header (inline)
    std::string to_string() const {
        const char* type_str = "";
        switch(type) {
            case TokenType::EOF_TOKEN: type_str = "EOF"; break;
            case TokenType::SYMBOL: type_str = "SYMBOL"; break;
            case TokenType::NUMBER: type_str = "NUMBER"; break;
            case TokenType::STRING: type_str = "STRING"; break;
            case TokenType::LPAREN: type_str = "LPAREN"; break;
            case TokenType::RPAREN: type_str = "RPAREN"; break;
            case TokenType::QUOTE: type_str = "QUOTE"; break;
            case TokenType::BACKQUOTE: type_str = "BACKQUOTE"; break;
            case TokenType::COMMA: type_str = "COMMA"; break;
            case TokenType::COMMA_AT: type_str = "COMMA_AT"; break;
            case TokenType::BOOLEAN: type_str = "BOOLEAN"; break;
        }
        
        return std::string("Token{type: ") + type_str + 
               ", text: '" + std::string(text) + 
               "', line: " + std::to_string(start.line) + 
               ", offset: " + std::to_string(start.offset) + "}";
    }
    
    bool is(const char* str) const { return text == str; }
};

#endif