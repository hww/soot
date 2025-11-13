#include "parser.h"
#include "source-info.h"
#include "errors.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include "parser.h"
#include "source-info.h"
#include "errors.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

// Вспомогательные методы
bool Parser::is_hex_char(char c) const {
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

bool Parser::is_float_start(char c) const {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}

bool Parser::is_decimal_start(char c) const {
    return (c >= '0' && c <= '9') || c == '-' || c == '+';
}

bool Parser::str_contains(std::string_view str, char ch) const {
    return str.find(ch) != std::string_view::npos;
}

// Парсер бинарных чисел: #b1010
bool Parser::try_parse_binary(const Token& token, int64_t& result) const {
    if (token.text.size() < 3 || token.text[0] != '#' || token.text[1] != 'b') {
        return false;
    }
    
    // Проверяем что все символы - 0 или 1
    for (size_t i = 2; i < token.text.size(); i++) {
        char c = token.text[i];
        if (c != '0' && c != '1') {
            return false;
        }
    }
    
    // Парсим бинарное число
    uint64_t value = 0;
    for (size_t i = 2; i < token.text.size(); i++) {
        if (value & (0x8000000000000000)) {
            throw ParseError(SourceInfo(), "Binary constant overflow: " + std::string(token.text));
        }
        
        value <<= 1;
        if (token.text[i] == '1') {
            value |= 1;
        }
    }
    
    result = static_cast<int64_t>(value);
    return true;
}

bool Parser::try_parse_hex(const Token& token, int64_t& result) const {
    if (token.text.size() < 3 || token.text[0] != '#' || token.text[1] != 'x') {
        return false;
    }
    
    // Проверяем hex символы
    for (size_t i = 2; i < token.text.size(); i++) {
        if (!is_hex_char(token.text[i])) {
            return false;
        }
    }
    
    try {
        std::string hex_str(token.text.substr(2));
        size_t end_pos = 0;
        uint64_t value = std::stoull(hex_str, &end_pos, 16);
        
        if (end_pos != hex_str.length()) {
            return false;
        }
        
        result = static_cast<int64_t>(value);
        return true;
    } catch (const std::exception& e) {
        throw ParseError(SourceInfo(), "Invalid hexadecimal constant: " + std::string(token.text));
    }
}

// Парсеры значений
bool Parser::try_parse_integer(const Token& token, int64_t& result) const {
    if (token.type != TokenType::WORD) return false;
    
    const std::string text(token.text);
    if (text.empty()) return false;
    
    // Проверяем что все символы - цифры или знак
    for (size_t i = 0; i < text.length(); i++) {
        if (i == 0 && (text[i] == '+' || text[i] == '-')) continue;
        if (!std::isdigit(text[i])) return false;
    }
    
    try {
        result = std::stoll(text);
        return true;
    } catch (...) {
        return false;
    }
}

bool Parser::try_parse_float(const Token& token, double& result) const {
    if (token.type != TokenType::WORD) return false;
    
    const std::string text(token.text);
    if (text.empty()) return false;
    
    // Проверяем формат: цифры, одна точка, опционально знак
    bool has_dot = false;
    for (size_t i = 0; i < text.length(); i++) {
        if (i == 0 && (text[i] == '+' || text[i] == '-')) continue;
        if (text[i] == '.') {
            if (has_dot) return false; // Две точки
            has_dot = true;
        } else if (!std::isdigit(text[i])) {
            return false;
        }
    }
    
    // Должна быть хотя бы одна цифра до или после точки
    if (!has_dot) return false;
    
    try {
        result = std::stod(text);
        return true;
    } catch (...) {
        return false;
    }
}

// Парсер чисел в научной нотации: 1e10, 2.5e-3, 1d5
bool Parser::try_parse_scientific(const Token& token, double& result) const {
    if (token.text.empty()) return false;
    
    std::string text(token.text);
    
    // Заменяем 'd' на 'e' для совместимости с Scheme
    for (char& c : text) {
        if (c == 'd' || c == 'D') {
            c = 'e';
        }
    }
    
    // Проверяем общий формат: [знак][цифры][.цифры]e[знак]цифры
    size_t e_pos = text.find('e');
    if (e_pos == std::string::npos) {
        return false;
    }
    
    // Проверяем часть до 'e'
    std::string mantissa = text.substr(0, e_pos);
    if (mantissa.empty()) return false;
    
    size_t start = (mantissa[0] == '-' || mantissa[0] == '+') ? 1 : 0;
    bool has_dot = false;
    
    for (size_t i = start; i < mantissa.size(); i++) {
        char c = mantissa[i];
        if (c == '.') {
            if (has_dot) return false;
            has_dot = true;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    
    // Проверяем часть после 'e'
    std::string exponent = text.substr(e_pos + 1);
    if (exponent.empty()) return false;
    
    start = (exponent[0] == '-' || exponent[0] == '+') ? 1 : 0;
    if (start == 1 && exponent.size() == 1) return false;
    
    for (size_t i = start; i < exponent.size(); i++) {
        if (exponent[i] < '0' || exponent[i] > '9') {
            return false;
        }
    }
    
    try {
        size_t end_pos = 0;
        double value = std::stod(text, &end_pos);
        
        if (end_pos != text.length()) {
            return false;
        }
        
        result = value;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

// Обновляем порядок парсинга в parse_atom
std::unique_ptr<ASTNode> Parser::parse_atom() {
    if (at_end()) {
        throw ParseError(SourceInfo(), "Unexpected end of input while parsing atom");
    }
    
    Token token = current();
    advance();
    
    // Пробуем парсить как разные типы в порядке приоритета
    int64_t int_value;
    double float_value;
    bool bool_value;
    
    std::unique_ptr<ASTNode> node;

    SourceInfo info(
        "expression",  // filename 
        token.start.line, 
        token.start.offset,  // используем offset как column
        std::string(token.text)  // line_text
    );
    
    // РЕГИСТРИРУЕМ AST узел
    g_source_db.link(node.get(), info);
    

    // 1. Булевы значения
    if (try_parse_boolean(token, bool_value)) {
        node = std::make_unique<ASTNode>(ObjectType::BOOLEAN, token);
    }
    // 2. Бинарные числа
    else if (try_parse_binary(token, int_value)) {
        node = std::make_unique<ASTNode>(ObjectType::INTEGER, token);
    }
    // 3. Шестнадцатеричные числа  
    else if (try_parse_hex(token, int_value)) {
        node = std::make_unique<ASTNode>(ObjectType::INTEGER, token);
    }
    // 4. Целые числа
    else if (try_parse_integer(token, int_value)) {
        node = std::make_unique<ASTNode>(ObjectType::INTEGER, token);
    }
    // 5. Числа в научной нотации (должно быть перед обычными float)
    else if (try_parse_scientific(token, float_value)) {
        node = std::make_unique<ASTNode>(ObjectType::FLOAT, token);
    }
    // 6. Числа с плавающей точкой
    else if (try_parse_float(token, float_value)) {
        node = std::make_unique<ASTNode>(ObjectType::FLOAT, token);
    }
    // 7. Строки
    else if (token.type == TokenType::STRING) {
        node = std::make_unique<ASTNode>(ObjectType::STRING, token);
    }
    // 8. По умолчанию - символ
    else {
        node = std::make_unique<ASTNode>(ObjectType::SYMBOL, token);
    }
    return node;
}

bool Parser::try_parse_boolean(const Token& token, bool& result) const {
    if (token.type != TokenType::WORD) return false;
    
    if (token.text == "#t" || token.text == "#T") {
        result = true;
        return true;
    } else if (token.text == "#f" || token.text == "#F") {
        result = false;
        return true;
    }
    
    return false;
}

std::string Parser::parse_string_value(const Token& token) const {
    if (token.type != TokenType::STRING) {
        throw ParseError(SourceInfo(), "Expected string token");
    }
    return std::string(token.text);
}

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
                oss << "PAIR(";
                oss << children[0]->to_string(0);
                oss << ", ";
                oss << children[1]->to_string(0);
                oss << ")";
            } else {
                oss << "PAIR[invalid:" << children.size() << " children]";
            }
            break;
            
        case ObjectType::INTEGER: 
            oss << "INTEGER:" << std::string(token.text);
            break;
        case ObjectType::FLOAT: 
            oss << "FLOAT:" << std::string(token.text);
            break;
        case ObjectType::SYMBOL: 
            oss << "SYMBOL:" << std::string(token.text);
            break;
        case ObjectType::STRING: 
            oss << "STRING:\"" << std::string(token.text) << "\"";
            break;
        case ObjectType::BOOLEAN:
            oss << "BOOLEAN:" << std::string(token.text);
            break;
            
        default:
            oss << "UNKNOWN:" << static_cast<int>(type);
            break;
    }
    
    return oss.str();
}

std::unique_ptr<ASTNode> Parser::parse_quoted() {
    if (at_end()) {
        SourceInfo info = g_source_db.get_info(&current());
        throw ParseError(info, "Unexpected end of input after quote");
    }
    
    Token quote_token = current();
    advance();
    
    // 'expr → (quote expr)
    auto quoted_expr = parse_expression();
    
    // Создаем (quote expr) как PAIR
    auto quote_symbol = std::make_unique<ASTNode>(
        ObjectType::SYMBOL, 
        Token{TokenType::WORD, "quote", quote_token.start}
    );
    
    std::vector<std::unique_ptr<ASTNode>> pair_children;
    pair_children.push_back(std::move(quote_symbol));
    pair_children.push_back(std::move(quoted_expr));
    
    return std::make_unique<ASTNode>(
        ObjectType::PAIR, quote_token, std::move(pair_children)
    );
}

std::unique_ptr<ASTNode> Parser::parse_list() {
    if (at_end()) {
        SourceInfo info = g_source_db.get_info(&current());
        throw ParseError(info, "Unexpected end of input while parsing list");
    }
    
    Token lparen = current();
    advance();  // consume '('
    
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    while (!match(TokenType::RPAREN) && !at_end()) {
        elements.push_back(parse_expression());
    }
    
    if (!match(TokenType::RPAREN)) {
        SourceInfo info = g_source_db.get_info(&current());
        throw ParseError(info, "Expected ')' but found: " + current().to_string());
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
        throw ParseError(SourceInfo(), "Unexpected end of input while parsing expression");
    }
    
    try {
        switch(current().type) {
            case TokenType::LPAREN:
                return parse_list();
                
            case TokenType::QUOTE:
            case TokenType::BACKQUOTE:
                return parse_quoted();
                
            case TokenType::COMMA:
            case TokenType::COMMA_AT:
                // ИСПРАВЛЕНИЕ: теперь это WORD токены, парсим как атомы
                return parse_atom();
                
            default:
                return parse_atom();
        }
    }
    catch (const ParseError&) {
        throw;
    }
    catch (const std::exception& e) {
        throw ParseError(SourceInfo(), std::string("Unexpected parsing error: ") + e.what());
    }
}

std::unique_ptr<ASTNode> Parser::parse_program() {
    std::vector<std::unique_ptr<ASTNode>> expressions;
    
    try {
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
    catch (const ParseError&) {
        throw;
    }
    catch (const std::exception& e) {
        SourceInfo info = g_source_db.get_info(nullptr);
        throw ParseError(info, std::string("Unexpected program parsing error: ") + e.what());
    }
}