// FileHub.cpp - все реализации
#include "common/util/FileUtil.hpp"
#include <fstream>
#include <regex>
#include "common/util/Log.hpp"

namespace file_util {

    // ==================== ОСНОВНЫЕ УТИЛИТЫ ====================

    std::string read_text(const fs::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path.string());
        }
        return std::string((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
    }

    void write_text(const fs::path& path, const std::string& content) {
        create_dirs_for_file(path);
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot create file: " + path.string());
        }
        file << content;
    }

    std::vector<uint8_t> read_binary(const fs::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path.string());
        }
        auto size = file.tellg();
        std::vector<uint8_t> buffer(size);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    void write_binary(const fs::path& path, const std::vector<uint8_t>& data) {
        create_dirs_for_file(path);
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot create file: " + path.string());
        }
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    // ==================== РАБОТА С ПУТЯМИ ====================

    std::string get_filename(const fs::path& path) {
        return path.filename().string();
    }

    std::string get_stem(const fs::path& path) {
        return path.stem().string();
    }

    std::string get_extension(const fs::path& path) {
        return path.extension().string();
    }

    fs::path join_paths(const fs::path& a, const fs::path& b) {
        return a / b;
    }

    // ==================== РАБОТА С ДИРЕКТОРИЯМИ ====================

    bool create_dirs(const fs::path& path) {
        std::error_code ec;
        return fs::create_directories(path, ec);
    }

    bool create_dirs_for_file(const fs::path& file_path) {
        return file_path.has_parent_path() ? create_dirs(file_path.parent_path()) : true;
    }

    std::vector<fs::path> list_files(const fs::path& dir) {
        std::vector<fs::path> files;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }
        }
        return files;
    }

    std::vector<fs::path> list_dirs(const fs::path& dir) {
        std::vector<fs::path> dirs;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    dirs.push_back(entry.path());
                }
            }
        }
        return dirs;
    }

    // ==================== ПОИСК ФАЙЛОВ ====================

    std::vector<fs::path> find_by_extension(const fs::path& dir, const std::string& ext) {
        std::vector<fs::path> files;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ext) {
                    files.push_back(entry.path());
                }
            }
        }
        return files;
    }

    std::vector<fs::path> find_by_extension_recursive(const fs::path& dir, const std::string& ext) {
        std::vector<fs::path> files;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ext) {
                    files.push_back(entry.path());
                }
            }
        }
        return files;
    }

    std::vector<fs::path> find_by_pattern(const fs::path& dir, const std::regex& pattern) {
        std::vector<fs::path> files;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && std::regex_search(entry.path().filename().string(), pattern)) {
                    files.push_back(entry.path());
                }
            }
        }
        return files;
    }

    // ==================== ПРОВЕРКИ И ИНФОРМАЦИЯ ====================

    uintmax_t get_size(const fs::path& path) {
        return fs::file_size(path);
    }

    // ==================== ОПЕРАЦИИ С ФАЙЛАМИ ====================

    void copy_file(const fs::path& from, const fs::path& to) {
        create_dirs_for_file(to);
        fs::copy_file(from, to, fs::copy_options::overwrite_existing);
    }

    void move_file(const fs::path& from, const fs::path& to) {
        create_dirs_for_file(to);
        fs::rename(from, to);
    }

    bool remove_file(const fs::path& path) {
        return fs::remove(path);
    }

    uintmax_t remove_dir(const fs::path& path) {
        return fs::remove_all(path);
    }

    // ========== ОПЕРАЦИИ С ФАЙЛАМИ ДЛЯ ЭТОГО ПРОЕКТА ============

    static fs::path g_project_path = "";

    void set_project_path(const fs::path& path) {
        if (fs::exists(path) && fs::is_directory(path)) {
            g_project_path = fs::absolute(path);
        }
    }

    fs::path detect_project_root(fs::path start_from) {
        fs::path current = fs::absolute(start_from);
        
        // Защита от бесконечного цикла
        const size_t MAX_ITERATIONS = 100;
        size_t iterations = 0;
        
        while (current.has_parent_path() && iterations < MAX_ITERATIONS) {
            // Маркеры корня проекта
            if (fs::exists(current / "project.sot") || 
                fs::exists(current / ".soot-root") ||
                fs::exists(current / ".git")) {
                return current;
            }
            
            fs::path parent = current.parent_path();
            
            // Если достигли корневой директории (например, "/" или "C:\")
            if (parent == current) {
                break;
            }
            
            current = parent;
            iterations++;
        }
        
        // Fallback с логированием
        lg::debug("Project root not found, falling back to: {}", fs::current_path().string());
        return fs::current_path();
    }

    fs::path get_path(PathType type) {
        switch (type) {
            case PathType::CWD: return fs::current_path();
            case PathType::PROJECT: 
                return g_project_path.empty() ? (g_project_path = detect_project_root()) : g_project_path;
            
            case PathType::HOME: {
                const char* home = std::getenv("HOME");
                return home ? fs::path(home) : fs::current_path();
            }
            
            case PathType::CONFIG: {
                const char* xdg = std::getenv("XDG_CONFIG_HOME");
                if (xdg) return fs::path(xdg) / "soot";
                return get_path(PathType::HOME) / ".config" / "soot";
            }

            case PathType::CACHE: {
                const char* xdg = std::getenv("XDG_CACHE_HOME");
                if (xdg) return fs::path(xdg) / "soot";
                return get_path(PathType::HOME) / ".cache" / "soot";
            }

            case PathType::SHARE:
                // В будущем здесь можно добавить логику проверки /usr/share vs /usr/local/share
                return "/usr/local/share/soot";

            default: return fs::current_path();
        }
    }

    fs::path find_config_file(const std::string& filename) {
        // 1. Проверяем проект
        fs::path p = get_path(PathType::PROJECT) / filename;
        if (fs::exists(p)) return p;

        // 2. Проверяем конфиг пользователя
        p = get_path(PathType::CONFIG) / filename;
        if (fs::exists(p)) return p;

        // 3. Проверяем системную папку
        p = get_path(PathType::SHARE) / filename;
        if (fs::exists(p)) return p;

        return ""; // Не нашли
    }

    std::string get_file_path(const std::vector<std::string>& input) {
        // TODO - clean this behaviour up, it causes unexpected behaviour when working with files
        // the project path should be explicitly provided by whatever if needed
        // TEMP HACK
        // - if the provided path is absolute, don't add the project path
        if (input.size() == 1 && fs::path(input.at(0)).is_absolute()) {
            return input.at(0);
        }

        auto current_path = get_path(PathType::PROJECT);
        for (auto& str : input) {
            current_path /= str;
        }

        return current_path.string();
    }
} // namespace filehub