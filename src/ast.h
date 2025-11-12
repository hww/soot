#ifndef AST_H
#define AST_H

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "token.h"

enum class ObjectType : uint8_t {
    EMPTY_LIST,   // '()
    INTEGER,      // 42
    FLOAT,        // 3.14
    CHAR,         // #\A  
    SYMBOL,       // define, x, etc
    STRING,       // "hello"
    PAIR,         // (cons a b)
    ARRAY,        // #(1 2 3)
    LAMBDA,       // (lambda (x) x)
    MACRO,        // макросы
    BOOLEAN,      // #t #f
    PROGRAM,      // Последовательность выражений
};



struct ASTNode {
    ObjectType type;
    Token token;
    std::vector<std::unique_ptr<ASTNode>> children;
    
    // Простые конструкторы
    ASTNode(ObjectType t, const Token& tok) : type(t), token(tok) {}
    
    // Для пар (car . cdr)
    ASTNode(ObjectType t, const Token& tok, 
            std::unique_ptr<ASTNode> car, std::unique_ptr<ASTNode> cdr)
        : type(t), token(tok) {
        children.push_back(std::move(car));
        children.push_back(std::move(cdr));
    }
    
    // Для списков
    ASTNode(ObjectType t, const Token& tok, 
            std::vector<std::unique_ptr<ASTNode>>&& elements)
        : type(t), token(tok), children(std::move(elements)) {}
    
    std::string to_string(int indent = 0) const;
};

class Parser {
    std::vector<Token> tokens;
    size_t pos;
    
    const Token& current() const { 
        static Token eof_token{TokenType::EOF_TOKEN, "", {0, 0}};
        return pos < tokens.size() ? tokens[pos] : eof_token; 
    }
    
    void advance() { if (pos < tokens.size()) pos++; }
    bool match(TokenType type) const { return current().type == type; }
    bool at_end() const { return match(TokenType::EOF_TOKEN); }
    
    std::unique_ptr<ASTNode> parse_expression();
    std::unique_ptr<ASTNode> parse_list();
    std::unique_ptr<ASTNode> parse_quoted();
    std::unique_ptr<ASTNode> parse_atom();

public:
    Parser(std::vector<Token>&& t) : tokens(std::move(t)), pos(0) {}
    std::unique_ptr<ASTNode> parse_program(); 
};

#endif