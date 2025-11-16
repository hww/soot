#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include "script/reader.h"
#include "script/object.h"
#include "script/interpreter.h"
#include "script/source_info.h"

using namespace script;

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