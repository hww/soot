#include "xiff/XiffInjector.hpp"
#include "common/util/FileUtil.hpp" // Используем твой FileHub/FileUtil
#include <sstream>

XiffInjector::Result XiffInjector::inject(const fs::path& target_file, 
                                         const std::string& section_id, 
                                         const std::string& content,
                                         const std::string& source_name) {
    try {
        if (!file_util::exists(target_file)) {
            return {false, "[ERR] [XiffInjector] Target file does not exist: " + target_file.string()};
        }

        std::string file_content = file_util::read_text(target_file);
        std::string prefix = get_comment_prefix(target_file);

        // Формируем теги
        std::string start_tag = prefix + " xiff:begin " + section_id;
        std::string end_tag = prefix + " xiff:end " + section_id;

        size_t start_pos = file_content.find(start_tag);
        size_t end_pos = file_content.find(end_tag);

        if (start_pos == std::string::npos || end_pos == std::string::npos) {
            return {false, "[ERR] [XiffInjector] Required tags not found for section: " + start_tag + " and  " + end_tag};
        }

        // Вычисляем позицию после первой строки (тега begin)
        size_t content_start = file_content.find('\n', start_pos);
        if (content_start == std::string::npos) content_start = start_pos + start_tag.length();
        else content_start++; // Переходим на следующую строку после тега

        // Формируем блок вставки
        std::stringstream ss;
        if (!source_name.empty()) {
            ss << prefix << "[XiffInjector] Generated from: " << source_name << "\n";
        }
        ss << content;
        
        // Добавляем перевод строки, если его нет в конце контента
        if (!content.empty() && content.back() != '\n') {
            ss << "\n";
        }

        // Заменяем всё между тегами (не трогая сами теги)
        std::string new_file_content = file_content.substr(0, content_start);
        new_file_content += ss.str();
        new_file_content += file_content.substr(end_pos);

        file_util::write_text(target_file, new_file_content);
        
        return {true, "[XiffInjector] Successfully injected into " + section_id};

    } catch (const std::exception& e) {
        return {false, std::string("[ERR] [XiffInjector] ") + e.what()};
    }
}

std::string XiffInjector::get_comment_prefix(const fs::path& file) {
    std::string ext = file.extension().string();
    if (ext == ".asm" || ext == ".s" || ext == ".inc") {
        return ";";
    }
    // По умолчанию C-style ( .h, .cpp, .c, .sot )
    return "//";
}