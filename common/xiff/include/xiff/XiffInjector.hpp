#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class XiffInjector {
public:
    struct Result {
        bool success;
        std::string message;
    };

    XiffInjector() = default;

    /**
     * Вставляет контент в файл между тегами soot:begin и soot:end.
     * @param target_file Путь к файлу (например, interface.h)
     * @param section_id Идентификатор секции
     * @param content Новый текст для вставки
     * @param source_name Имя источника для пометки "Generated from..."
     */
    Result inject(const fs::path& target_file, 
                  const std::string& section_id, 
                  const std::string& content,
                  const std::string& source_name = "");

private:
    // Определяет стиль комментария на основе расширения файла
    std::string get_comment_prefix(const fs::path& file);
};