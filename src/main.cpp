#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "reader.h"
#include "object.h"
#include "interpreter.h"
#include "source-info.h"
#include "command-line.h"  // Используем то, что уже есть

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

#include <iostream>
#include "reader.h"
#include "interpreter.h"

int main(int argc, char* argv[]) {
    try {
        Interpreter interpreter;
        
        // Если есть аргументы командной строки, обработать их
        if (argc > 1) {
            // TODO: обработка файлов из командной строки
            std::cout << "Command line arguments not yet implemented\n";
        }
        
        interpreter.execute_repl();
    } 
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}