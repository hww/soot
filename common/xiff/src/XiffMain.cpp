
#include <iostream>
#include <filesystem>
#include <fstream>
#include "fmt/format.h"
#include "fmt/color.h"
#include "common/util/Log.hpp"
#include "common/util/FileUtil.hpp"
#include "common/script/Export.hpp"
#include "xiff/XiffCompiler.hpp"

namespace fs = std::filesystem;




void print_xiff_usage() {
    fmt::print(fg(fmt::color::cyan), "XIFF (eXternal Interface Function Fabric) - SOOT Edition\n\n");
    fmt::print("Usage:\n");
    fmt::print("  xiff [options] <asm_files...>\n\n");
    fmt::print("Options:\n");
    fmt::print("  -l, --lib <file>       {} Load custom SOOT library for parsing\n", "→");
    fmt::print("  -p, --project          {} Location of project files\n", "→");
    fmt::print("  -v, --verbose          {} Show debug info during scan\n", "→");
    fmt::print("  -h, --help             {} Show this help\n", "→");
    fmt::print("\nExample:\n");
    fmt::print("  xiff -o interface.h src/*.asm\n");
}



int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_xiff_usage();
        return 0;
    }
    

    std::string project_path;
    std::vector<std::string> libs;
    std::vector<std::string> sources;

    bool verbose = false;

    // 1. Парсинг аргументов
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_xiff_usage();
            return 0;
        } else if (arg == "--project" || arg == "-p") {
            if (i + 1 < argc) { project_path = argv[++i]; file_util::set_project_path(project_path); }
        } else if (arg == "--lib" || arg == "-l") {
            if (i + 1 < argc) libs.push_back(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg.starts_with("-")) {
            fmt::print(fg(fmt::color::red), "Unknown option: {}\n", arg);
            return 1;
        } else {
            sources.push_back(arg);
        }
    }

    try {
        // 2. Инициализируем SOOT Runtime (без интерактивного режима)
        script::Interpreter sooti("Xiff"); // Создаем ядро


        XiffCompiler compiler(sooti);

        // 1. Преамбула
        for (const auto& lib : libs) {
            compiler.load_library(lib);
        }

        // 2. Процессинг
        for (const auto& src : sources) {
            compiler.scan_file(src);
        }

        // 3. Завершение
        compiler.finalize_and_inject();

    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "XIFF Fatal Error: {}\n", e.what());
        return 1;
    }

    return 0;
}