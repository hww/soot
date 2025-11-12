#include "token.h"
#include <sstream>

std::string Token::to_string() const {
    std::ostringstream oss;
    oss << "Token{type: ";
    
    switch(type) {
        case TokenType::EOF_TOKEN: oss << "EOF"; break;
        case TokenType::SYMBOL: oss << "SYMBOL"; break;
        case TokenType::NUMBER: oss << "NUMBER"; break;
        case TokenType::STRING: oss << "STRING"; break;
        case TokenType::LPAREN: oss << "LPAREN"; break;
        case TokenType::RPAREN: oss << "RPAREN"; break;
        case TokenType::QUOTE: oss << "QUOTE"; break;
        case TokenType::BACKQUOTE: oss << "BACKQUOTE"; break;
        case TokenType::COMMA: oss << "COMMA"; break;
        case TokenType::COMMA_AT: oss << "COMMA_AT"; break;
        case TokenType::BOOLEAN: oss << "BOOLEAN"; break;
    }
    
    oss << ", text: '" << text << "', line: " << start.line;
    oss << ", offset: " << start.offset << "}";
    return oss.str();
}
