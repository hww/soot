#include <iostream>
#include <fstream>
#include <sstream>
#include "tokenizer.h"


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
            // Тестовый код по умолчанию
            source = "(define x 42)\n(print \"Hello\")\n'symbol";
        }
        
        Tokenizer tokenizer(source);
        
        while (true) {
            Token token = tokenizer.next_token();
            std::cout << token.to_string() << std::endl;
            
            if (token.type == TokenType::EOF_TOKEN) {
                break;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
