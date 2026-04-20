#include "common/sooti/Reader.hpp"
#include "common/sooti/Object.hpp"
#include "common/sootc/compiler/Compiler.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"
#include "file/BinaryFileInspector.hpp"
#include "fmt/color.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>

using namespace script;
using namespace sootc;
using namespace carbon;

namespace fs = std::filesystem;

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string content;
    file.seekg(0, std::ios::end);
    content.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    return content;
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " --target <dir> <input.soot> [options]\n";
    std::cerr << "Required:\n";
    std::cerr << "  --target <dir>   Target directory for compiled files\n";
    std::cerr << "  --source-root <dir>  Root directory to determine module namespace\n";
    std::cerr << "  <input.soot>     Source file to compile\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  -o <name>        Output base name (default: input file name)\n";
    std::cerr << "  --flat           Don't preserve directory structure (flat output)\n";
    std::cerr << "  -h, --help       Show this help\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << program_name << " --target build/modules math/add.soot\n";
    std::cerr << "\nNote: One .sot file produces one .bin and one .dci\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_file;
    std::string target_dir;
    std::string source_root;
    std::string output_name;
    bool flat_output = false;
    
    // Парсим аргументы
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--target" && i + 1 < argc) {
            target_dir = argv[++i];
        }
        else if (arg == "--source-root" && i + 1 < argc) {
            source_root = argv[++i];
        }
        else if (arg == "-o" && i + 1 < argc) {
            output_name = argv[++i];
        }
        else if (arg == "--flat") {
            flat_output = true;
        }
        else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg[0] != '-') {
            if (input_file.empty()) {
                input_file = arg;
            } else {
                std::cerr << "Error: Multiple input files specified: " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
        }
        else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Проверяем обязательные аргументы
    if (target_dir.empty()) {
        std::cerr << "Error: --target is required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        print_usage(argv[0]);
        return 1;
    }
    
    if (source_root.empty()) {
        std::cerr << "Error: --source-root is required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // Извлекаем информацию о входном файле
    fs::path input_path(input_file);
    
    if (!fs::exists(input_path)) {
        std::cerr << "Error: Input file does not exist: " << input_file << "\n";
        return 1;
    }
    
    std::string base_name = input_path.stem().string();
    std::string output_base = output_name.empty() ? base_name : output_name;
    
    // Получаем абсолютные пути
    fs::path absolute_source_root = fs::absolute(source_root);
    fs::path absolute_input = fs::absolute(input_path);
    fs::path absolute_target = fs::absolute(target_dir);
    
    // Вычисляем относительный путь от source_root до директории исходника
    fs::path relative_path;
    try {
        relative_path = fs::relative(absolute_input.parent_path(), absolute_source_root);
    } catch (const std::exception& e) {
        std::cerr << "Error: Input file is not under source root: " << source_root << "\n";
        return 1;
    }
    
    // Формируем имя модуля: relative_path + base_name
    std::string module_name;
    if (relative_path.empty() || relative_path == ".") {
        module_name = output_base;
    } else {
        module_name = (relative_path / output_base).string();
    }
    
    // Формируем выходную директорию
    fs::path output_dir;
    if (flat_output) {
        output_dir = absolute_target;
    } else {
        output_dir = absolute_target / relative_path;
    }
    
    // Создаем директорию
    try {
        fs::create_directories(output_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error: Cannot create target directory: " << output_dir.string() 
                  << " - " << e.what() << "\n";
        return 1;
    }
    
    lg::info("=== SOOT Compiler ===");
    lg::info("Input: {}", input_file);
    lg::info("Module Name: {}", module_name);
    lg::info("Target Dir: {}", output_dir.string());
    
    try {
        (void)Object::get_symbol_table();
        std::string source = read_file(input_file);
        
        Reader reader;
        Object forms = reader.read_from_string(source, false, input_file);
        
        lg::info("Parsed: {}", forms.print());
        
        TypeSystem& ts = TypeSystem::instance();
        // 1. Подготовка типов
        ts.add_builtin_types();

        // 2. Создаем компилятор (без Env!)
        Compiler compiler(ts);

        // 3. Запускаем компиляцию
        auto result = compiler.compile_file(forms, input_file);

        if (!result) {
            lg::error("Failed to build module: {}", result.error());
            return 1;
        }

        // 4. Сохраняем результат
        auto& binary = result.value();
        
        // TODO: Сохранение бинарника
        // binary->save(output_dir.string());
        
        BinaryFileInspector inspector(*binary, 2);
        inspector.inspect();
        lg::info("Module {} successfully compiled.", input_file);
        lg::info("=== Compilation complete ===");
        lg::info("Module: {}", module_name);
        
    } catch (const std::exception& e) {
        lg::error("Compilation failed: {}", e.what());
        return 1;
    }
    
    return 0;
}