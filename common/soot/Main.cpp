#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include "common/soot/Reader.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/Interpreter.hpp"
#include "common/soot/TextDb.hpp"
#include "repl/config.h"

using namespace soot;

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
    (void)argv;
    try {
        REPL::Wrapper repl(SootPlatform::Default);
        Interpreter interpreter;
        
        // Если есть аргументы командной строки, обработать их
        if (argc > 1) {
            // TODO: обработка файлов из командной строки
            std::cout << "Command line arguments not yet implemented\n";
        }
        
        interpreter.execute_repl(repl);
    } 
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}