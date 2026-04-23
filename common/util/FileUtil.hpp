// FileHub.h - чистые объявления
#pragma once

#include <cstdint>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace file_util {

namespace fs = std::filesystem;

// ==================== ОСНОВНЫЕ УТИЛИТЫ ====================
std::string          read_text(const fs::path &path);
void                 write_text(const fs::path &path, const std::string &content);
std::vector<uint8_t> read_binary(const fs::path &path, size_t size);
void                 write_binary(const fs::path &path, const std::vector<uint8_t> &data);

// ==================== РАБОТА С ПУТЯМИ ====================
std::string get_filename(const fs::path &path);
std::string get_stem(const fs::path &path);
std::string get_extension(const fs::path &path);
fs::path    join_paths(const fs::path &a, const fs::path &b);
// fs::path make_path(std::string path) { return fs::path(path); }

// ==================== РАБОТА С ДИРЕКТОРИЯМИ ====================
bool                  create_dirs(const fs::path &path);
bool                  create_dirs_for_file(const fs::path &file_path);
std::vector<fs::path> list_files(const fs::path &dir);
std::vector<fs::path> list_dirs(const fs::path &dir);

// ==================== ПОИСК ФАЙЛОВ ====================
std::vector<fs::path> find_by_extension(const fs::path &dir, const std::string &ext);
std::vector<fs::path> find_by_extension_recursive(const fs::path &dir, const std::string &ext);
std::vector<fs::path> find_by_pattern(const fs::path &dir, const std::regex &pattern);

// ==================== ПРОВЕРКИ И ИНФОРМАЦИЯ ====================
// Простые обёртки - можно inline
inline bool exists(const fs::path &path) {
    return fs::exists(path);
}
inline bool is_file(const fs::path &path) {
    return fs::is_regular_file(path);
}
inline bool is_dir(const fs::path &path) {
    return fs::is_directory(path);
}

uintmax_t get_size(const fs::path &path);

// ==================== ОПЕРАЦИИ С ФАЙЛАМИ ====================
void      copy_file(const fs::path &from, const fs::path &to);
void      move_file(const fs::path &from, const fs::path &to);
bool      remove_file(const fs::path &path);
uintmax_t remove_dir(const fs::path &path);

// =================== ОПЕРАЦИИ С ПУТЯМИ  ====================
std::string convert_to_unix_path_separators(const std::string& path);

// ========== ОПЕРАЦИИ С ФАЙЛАМИ ДЛЯ ЭТОГО ПРОЕКТА ============
enum class PathType {
    CWD,    // Текущая рабочая директория
    EXE,    // Директория с бинарником
    HOME,   // Домашняя папка пользователя
    CONFIG, // ~/.config/soot/
    CACHE,  // ~/.cache/soot/
    SHARE,  // /usr/local/share/soot/ или эквивалент
    PROJECT // Корень проекта (определяется автоматически или через set)
};

void     set_project_path(const fs::path &path);
fs::path get_path(PathType type);

// Ищет файл в приоритете: Project -> Config -> Share
fs::path find_config_file(const std::string &filename);

// Логика автопоиска корня (ищет файл-маркер вверх по дереву)
fs::path detect_project_root(fs::path start_from = fs::current_path());

// Логика генерации пути к файлу
std::string get_file_path(const std::vector<std::string> &input);

fs::path get_absolute_path(const fs::path &path);
} // namespace file_util