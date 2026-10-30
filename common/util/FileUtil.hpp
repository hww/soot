// FileHub.h - чистые объявления
#pragma once

#include <cstdint>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>


#if defined(__linux__)
#include <unistd.h> // readlink
#elif defined(__APPLE__)
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

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


/// @brief Directory containing the running executable.
/// @details Used to find tool-owned resources (prelude, default
///          sidbase) that must work regardless of the working
///          directory. Platform-specific: /proc/self/exe on Linux,
///          GetModuleFileName on Windows, _NSGetExecutablePath on macOS.
/// @return Absolute path to the directory, or empty path on failure.
std::filesystem::path get_executable_dir();

fs::path get_absolute_path(const fs::path &path);
} // namespace file_util