// FileHub.cpp - все реализации
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include <fstream>
#include <regex>
#include <CommonTypes.hpp>

namespace file_util {

// ==================== ОСНОВНЫЕ УТИЛИТЫ ====================

std::string read_text(const fs::path &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void write_text(const fs::path &path, const std::string &content) {
    create_dirs_for_file(path);
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + path.string());
    }
    file << content;
}

std::vector<uint8_t> read_binary(const fs::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    auto                 size = file.tellg();
    std::vector<uint8_t> buffer(size);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), size);
    return buffer;
}

void write_binary(const fs::path &path, const std::vector<uint8_t> &data, size_t size) {
    create_dirs_for_file(path);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + path.string());
    }
    file.write(reinterpret_cast<const char *>(data.data()), size);
}

// ==================== РАБОТА С ПУТЯМИ ====================

std::string get_filename(const fs::path &path) {
    return path.filename().string();
}

std::string get_stem(const fs::path &path) {
    return path.stem().string();
}

std::string get_extension(const fs::path &path) {
    return path.extension().string();
}

fs::path join_paths(const fs::path &a, const fs::path &b) {
    return a / b;
}

// =================== ОПЕРАЦИИ С ПУТЯМИ  ====================
std::string convert_to_unix_path_separators(const std::string& path) {
#ifdef _WIN32
  std::string copy = path;
  std::replace(copy.begin(), copy.end(), '\\', '/');
  return copy;
#else
  return path;
#endif
}

// ==================== РАБОТА С ДИРЕКТОРИЯМИ ====================

bool create_dirs(const fs::path &path) {
    std::error_code ec;
    return fs::create_directories(path, ec);
}

bool create_dirs_for_file(const fs::path &file_path) {
    return file_path.has_parent_path() ? create_dirs(file_path.parent_path()) : true;
}

std::vector<fs::path> list_files(const fs::path &dir) {
    std::vector<fs::path> files;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
    }
    return files;
}

std::vector<fs::path> list_dirs(const fs::path &dir) {
    std::vector<fs::path> dirs;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                dirs.push_back(entry.path());
            }
        }
    }
    return dirs;
}

// ==================== ПОИСК ФАЙЛОВ ====================

std::vector<fs::path> find_by_extension(const fs::path &dir, const std::string &ext) {
    std::vector<fs::path> files;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ext) {
                files.push_back(entry.path());
            }
        }
    }
    return files;
}

std::vector<fs::path> find_by_extension_recursive(const fs::path &dir, const std::string &ext) {
    std::vector<fs::path> files;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto &entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ext) {
                files.push_back(entry.path());
            }
        }
    }
    return files;
}

std::vector<fs::path> find_by_pattern(const fs::path &dir, const std::regex &pattern) {
    std::vector<fs::path> files;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() &&
                std::regex_search(entry.path().filename().string(), pattern)) {
                files.push_back(entry.path());
            }
        }
    }
    return files;
}

// ==================== ПРОВЕРКИ И ИНФОРМАЦИЯ ====================

uintmax_t get_size(const fs::path &path) {
    return fs::file_size(path);
}

// ==================== ОПЕРАЦИИ С ФАЙЛАМИ ====================

void copy_file(const fs::path &from, const fs::path &to) {
    create_dirs_for_file(to);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing);
}

void move_file(const fs::path &from, const fs::path &to) {
    create_dirs_for_file(to);
    fs::rename(from, to);
}

bool remove_file(const fs::path &path) {
    return fs::remove(path);
}

uintmax_t remove_dir(const fs::path &path) {
    return fs::remove_all(path);
}

std::filesystem::path get_executable_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD   len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return std::filesystem::path(buf).parent_path();
#elif defined(__linux__)
    char    buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return {};
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return {};
    return std::filesystem::path(buf.data()).parent_path();
#else
    return {};
#endif
}

fs::path get_absolute_path(const fs::path &path) {
    if (!fs::exists(path)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }
    return fs::absolute(path).lexically_normal();
}
} // namespace file_util