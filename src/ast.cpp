#include "ast.h"
#include <sstream>
#include <stdexcept>

// Обновляем to_string для PROGRAM:
std::string ASTNode::to_string(int indent) const {
    std::string indentation(indent, ' ');
    std::ostringstream oss;
    
    oss << indentation;
    
    switch(type) {
        case ObjectType::PROGRAM:
            oss << "PROGRAM [\n";
            for (const auto& expr : children) {
                oss << expr->to_string(indent + 2) << "\n";
            }
            oss << indentation << "]";
            break;
            
        case ObjectType::EMPTY_LIST: 
            oss << "()";
            break;
            
        case ObjectType::PAIR:
            if (children.size() == 2) {
                oss << "(";
                const ASTNode* current = this;
                while (current->type == ObjectType::PAIR && current->children.size() == 2) {
                    oss << current->children[0]->to_string(0);
                    if (current->children[1]->type == ObjectType::EMPTY_LIST) {
                        break;
                    } else if (current->children[1]->type == ObjectType::PAIR) {
                        oss << " ";
                        current = current->children[1].get();
                    } else {
                        oss << " . " << current->children[1]->to_string(0);
                        break;
                    }
                }
                oss << ")";
            } else {
                oss << "PAIR[...]";
            }
            break;
            
        case ObjectType::INTEGER: 
        case ObjectType::FLOAT: 
        case ObjectType::SYMBOL: 
        case ObjectType::STRING: 
        case ObjectType::BOOLEAN:
            oss << "'" << std::string(token.text) << "'";
            break;
            
        default:
            oss << "?" << static_cast<int>(type);
            break;
    }
    
    return oss.str();
}

std::unique_ptr<ASTNode> Parser::parse_atom() {
    Token token = current();
    advance();
    
    switch(token.type) {
        case TokenType::SYMBOL:
            return std::make_unique<ASTNode>(ObjectType::SYMBOL, token);
            
        case TokenType::NUMBER:
            // Пока все числа считаем INTEGER, потом добавим FLOAT
            return std::make_unique<ASTNode>(ObjectType::INTEGER, token);
            
        case TokenType::STRING:
            return std::make_unique<ASTNode>(ObjectType::STRING, token);
            
        case TokenType::BOOLEAN:
            return std::make_unique<ASTNode>(ObjectType::BOOLEAN, token);
            
        default:
            throw std::runtime_error("Unexpected token in atom: " + token.to_string());
    }
}

std::unique_ptr<ASTNode> Parser::parse_quoted() {
    Token quote_token = current();
    advance();
    
    // 'expr → (quote expr)
    auto quoted_expr = parse_expression();
    
    // Создаем (quote expr) как PAIR
    auto quote_symbol = std::make_unique<ASTNode>(
        ObjectType::SYMBOL, 
        Token{TokenType::SYMBOL, "quote", quote_token.start}
    );
    
    std::vector<std::unique_ptr<ASTNode>> pair_children;
    pair_children.push_back(std::move(quote_symbol));
    pair_children.push_back(std::move(quoted_expr));
    
    return std::make_unique<ASTNode>(
        ObjectType::PAIR, quote_token, std::move(pair_children)
    );
}

std::unique_ptr<ASTNode> Parser::parse_list() {
    Token lparen = current();
    advance();  // consume '('
    
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    while (!match(TokenType::RPAREN) && !at_end()) {
        elements.push_back(parse_expression());
    }
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')'");
    }
    advance();  // consume ')'
    
    // Пустой список → EMPTY_LIST
    if (elements.empty()) {
        return std::make_unique<ASTNode>(ObjectType::EMPTY_LIST, lparen);
    }
    
    // Строим вложенные пары: (a b c) → (PAIR a (PAIR b (PAIR c EMPTY_LIST)))
    std::unique_ptr<ASTNode> list = std::make_unique<ASTNode>(
        ObjectType::EMPTY_LIST, Token{TokenType::RPAREN, ")", lparen.start}
    );
    
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        std::vector<std::unique_ptr<ASTNode>> pair_children;
        pair_children.push_back(std::move(*it));
        pair_children.push_back(std::move(list));
        
        list = std::make_unique<ASTNode>(
            ObjectType::PAIR, lparen, std::move(pair_children)
        );
    }
    
    return list;
}

std::unique_ptr<ASTNode> Parser::parse_expression() {
    if (at_end()) {
        throw std::runtime_error("Unexpected EOF");
    }
    
    switch(current().type) {
        case TokenType::LPAREN:
            return parse_list();
            
        case TokenType::QUOTE:
        case TokenType::BACKQUOTE:
            return parse_quoted();
            
        case TokenType::COMMA:
        case TokenType::COMMA_AT:
            // Пока просто символы, потом добавим UNQUOTE
            return parse_atom();
            
        default:
            return parse_atom();
    }
}

std::unique_ptr<ASTNode> Parser::parse_program() {
    std::vector<std::unique_ptr<ASTNode>> expressions;
    
    while (!at_end()) {
        expressions.push_back(parse_expression());
    }
    
    // Создаем узел PROGRAM со всеми выражениями
    if (expressions.empty()) {
        return std::make_unique<ASTNode>(ObjectType::EMPTY_LIST, 
                                        Token{TokenType::EOF_TOKEN, "", {0, 0}});
    }
    
    return std::make_unique<ASTNode>(ObjectType::PROGRAM,
                                    Token{TokenType::EOF_TOKEN, "program", {0, 0}},
                                    std::move(expressions));
}


