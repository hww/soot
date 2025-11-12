#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "tokenizer.h"
#include "ast.h"


std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    try {
        std::string source;
        
        if (argc > 1) {
            source = read_file(argv[1]);
        } else {
            source = "(define x 42)\n(print \"Hello\")\n'(1 2 3)";
        }
        
        // Токенизация
        Tokenizer tokenizer(source);
        std::vector<Token> tokens;
        
        while (true) {
            Token token = tokenizer.next_token();
            if (token.type == TokenType::EOF_TOKEN) break;
            tokens.push_back(token);
        }
        
        // Парсинг AST
        Parser parser(std::move(tokens));
        auto ast = parser.parse_program();
        
        std::cout << "=== TOKENS ===" << std::endl;
        for (const auto& token : tokens) {
            std::cout << token.to_string() << std::endl;
        }
        
        std::cout << "\n=== AST ===" << std::endl;
        std::cout << ast->to_string() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}