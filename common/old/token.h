#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <string_view>
#include <sstream>  // ← ДОБАВИТЬ для std::ostringstream

enum class TokenType {
    EOF_TOKEN,
    WORD,        // Символы, числа, булевы - всё здесь!
    STRING,      // Только строки в кавычках
    LPAREN,      // (
    RPAREN,      // )
    QUOTE,       // '
    BACKQUOTE,   // `
    COMMA,       // ,
    COMMA_AT     // ,@
};

struct SourcePos {
    size_t offset;
    size_t line;
};

struct Token {
    TokenType type;
    std::string_view text;
    SourcePos start;
    
    // Реализуем метод прямо здесь (inline)
    std::string to_string() const {
        std::ostringstream oss;
        oss << "Token{type: ";
        
        switch(type) {
            case TokenType::EOF_TOKEN: oss << "EOF"; break;
            case TokenType::WORD: oss << "WORD"; break;
            case TokenType::STRING: oss << "STRING"; break;
            case TokenType::LPAREN: oss << "LPAREN"; break;
            case TokenType::RPAREN: oss << "RPAREN"; break;
            case TokenType::QUOTE: oss << "QUOTE"; break;
            case TokenType::BACKQUOTE: oss << "BACKQUOTE"; break;
            case TokenType::COMMA: oss << "COMMA"; break;
            case TokenType::COMMA_AT: oss << "COMMA_AT"; break;
        }
        
        oss << ", text: '" << text << "', line: " << start.line;
        oss << ", offset: " << start.offset << "}";
        return oss.str();
    }
    
    bool is(const char* str) const { return text == str; }
};

#endif