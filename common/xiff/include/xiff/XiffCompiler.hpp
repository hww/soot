#pragma once

#include "common/sooti/Interpreter.hpp" // Твой sooti
#include "XiffInjector.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;


class XiffCompiler {
public:
    // Передаем ссылку на существующий интерпретатор
    explicit XiffCompiler(script::Interpreter& shared_interp);

    // 1. Загрузка библиотек (Преамбула)
    bool load_library(const std::string& lib_name);

    // 2. Сканирование ассемблерных файлов
    void scan_file(const fs::path& asm_path);

    // 3. Финализация и запись (Инъекция)
    // Вызывает Lisp-логику для получения контента и использует XiffInjector
    bool finalize_and_inject();

private:

    void eval_string(const std::string& line, const std::string& file);

    script::Interpreter& m_interp;
    XiffInjector m_injector;
    std::vector<fs::path> m_scanned_files;
};