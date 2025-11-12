#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "token.h"
#include <string>

class Tokenizer {
    std::string source;
    size_t pos;
    size_t line;
    size_t line_start_offset;
    
    char current() const { return pos < source.length() ? source[pos] : '\0'; }
    char peek() const { return (pos + 1) < source.length() ? source[pos + 1] : '\0'; }
    void advance() { 
        if (current() == '\n') {
            line++;
            line_start_offset = pos + 1;
        }
        pos++; 
    }
    
    void skip_whitespace();
    void skip_comment();
    
    Token parse_symbol();
    Token parse_number();
    Token parse_string();
    Token parse_boolean();
    
    SourcePos current_pos() const { 
        return {pos, line};
    }
    
public:
    Tokenizer(const std::string& input);
    Token next_token();
};

#endif
