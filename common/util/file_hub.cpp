#include "FileHub.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#endif

namespace filehub {

    namespace {

        // Вспомогательная функция для получения переменных окружения
        std::string get_env_var(const std::string& name) {
#ifdef _WIN32
            char* buffer = nullptr;
            size_t size = 0;
            if (_dupenv_s(&buffer, &size, name.c_str()) == 0 && buffer != nullptr) {
                std::string result(buffer);
                free(buffer);
                return result;
            }
            return "";
#else
            const char* value = std::getenv(name.c_str());
            return value ? value : "";
#endif
        }

        // Вспомогательная функция для создания временной метки
        std::string current_timestamp() {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
            ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
            return ss.str();
        }

    } // namespace

    // ==================== РЕАЛИЗАЦИЯ КРОССПЛАТФОРМЕННЫХ ПУТЕЙ ====================

    fs::path get_user_home_dir() {
#ifdef _WIN32
        char* homedir = nullptr;
        size_t len = 0;
        if (_dupenv_s(&homedir, &len, "USERPROFILE") == 0 && homedir != nullptr) {
            fs::path result(homedir);
            free(homedir);
            return result;
        }
#else
        const char* homedir = std::getenv("HOME");
        if (homedir != nullptr) {
            return fs::path(homedir);
        }

        // Fallback для Unix систем
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr) {
            return fs::path(pw->pw_dir);
        }
#endif

        throw std::runtime_error("Cannot determine user home directory");
    }

    fs::path get_user_config_dir(const std::string& app_name) {
        fs::path config_path;

#ifdef _WIN32
        char* appdata = nullptr;
        size_t len = 0;
        if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
            config_path = fs::path(appdata);
            free(appdata);
        }
#elif defined(__APPLE__)
        config_path = get_user_home_dir() / "Library" / "Application Support";
#else
        // Linux и другие Unix-системы
        std::string xdg_config = get_env_var("XDG_CONFIG_HOME");
        if (!xdg_config.empty()) {
            config_path = fs::path(xdg_config);
        }
        else {
            config_path = get_user_home_dir() / ".config";
        }
#endif

        if (!app_name.empty()) {
            config_path /= app_name;
        }

        return config_path;
    }

    fs::path get_user_data_dir(const std::string& app_name) {
        fs::path data_path;

#ifdef _WIN32
        char* local_appdata = nullptr;
        size_t len = 0;
        if (_dupenv_s(&local_appdata, &len, "LOCALAPPDATA") == 0 && local_appdata != nullptr) {
            data_path = fs::path(local_appdata);
            free(local_appdata);
        }
        else {
            // Fallback
            data_path = get_user_config_dir();
        }
#elif defined(__APPLE__)
        data_path = get_user_home_dir() / "Library" / "Application Support";
#else
        // Linux и другие Unix-системы
        std::string xdg_data = get_env_var("XDG_DATA_HOME");
        if (!xdg_data.empty()) {
            data_path = fs::path(xdg_data);
        }
        else {
            data_path = get_user_home_dir() / ".local" / "share";
        }
#endif

        if (!app_name.empty()) {
            data_path /= app_name;
        }

        return data_path;
    }

    fs::path get_current_executable_path() {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(nullptr, path, MAX_PATH) != 0) {
            return fs::path(path);
        }
#elif defined(__APPLE__)
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return fs::path(path);
        }
#else
        // Linux
        char path[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        if (count != -1) {
            return fs::path(std::string(path, count));
        }
#endif

        throw std::runtime_error("Cannot determine current executable path");
    }

    // ==================== РЕАЛИЗАЦИЯ ПРАКТИЧНЫХ УТИЛИТ ====================

    bool ensure_directory_for_file(const fs::path& file_path) {
        if (file_path.has_parent_path()) {
            std::error_code ec;
            return fs::create_directories(file_path.parent_path(), ec);
        }
        return true;
    }

    std::string read_file_text(const fs::path& path) {
        if (!fs::exists(path)) {
            throw std::runtime_error("File does not exist: " + path.string());
        }

        if (!fs::is_regular_file(path)) {
            throw std::runtime_error("Path is not a regular file: " + path.string());
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for reading: " + path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void write_file_text(const fs::path& path, const std::string& content) {
        ensure_directory_for_file(path);

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + path.string());
        }

        file << content;

        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + path.string());
        }
    }

    std::vector<uint8_t> read_file_binary(const fs::path& path) {
        if (!fs::exists(path)) {
            throw std::runtime_error("File does not exist: " + path.string());
        }

        if (!fs::is_regular_file(path)) {
            throw std::runtime_error("Path is not a regular file: " + path.string());
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for reading: " + path.string());
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Error reading file: " + path.string());
        }

        return buffer;
    }

    void write_file_binary(const fs::path& path, const void* data, size_t size) {
        if (size == 0) {
            // Создаём пустой файл
            ensure_directory_for_file(path);
            std::offile file(path, std::ios::binary);
            return;
        }

        ensure_directory_for_file(path);

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + path.string());
        }

        file.write(static_cast<const char*>(data), size);

        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + path.string());
        }
    }

    // ==================== РЕАЛИЗАЦИЯ ПОИСКА И ФИЛЬТРАЦИИ ====================

    std::vector<fs::path> find_files_matching(const fs::path& dir, const std::regex& pattern) {
        std::vector<fs::path> result;

        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            return result;
        }

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, pattern)) {
                    result.push_back(entry.path());
                }
            }
        }

        return result;
    }

    std::vector<fs::path> find_files_by_extension(const fs::path& dir,
        const std::string& extension,
        bool case_sensitive) {
        std::string pattern_str = ".*\\" + extension + "$";
        std::regex pattern;

        if (case_sensitive) {
            pattern = std::regex(pattern_str);
        }
        else {
            pattern = std::regex(pattern_str, std::regex_constants::icase);
        }

        return find_files_matching(dir, pattern);
    }

    std::vector<fs::path> find_files_recursively(const fs::path& base_dir, const std::regex& pattern) {
        std::vector<fs::path> result;

        if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) {
            return result;
        }

        for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, pattern)) {
                    result.push_back(entry.path());
                }
            }
        }

        return result;
    }

    // ==================== РЕАЛИЗАЦИЯ АНАЛИЗА И УТИЛИТ ====================

    std::string detect_line_endings(const std::string& content) {
        size_t lf_count = 0;
        size_t crlf_count = 0;

        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') {
                if (i > 0 && content[i - 1] == '\r') {
                    crlf_count++;
                }
                else {
                    lf_count++;
                }
            }
        }

        return (crlf_count > lf_count) ? "\r\n" : "\n";
    }

    fs::path make_timestamped_path(const fs::path& directory,
        const std::string& prefix,
        const std::string& extension) {
        std::string filename = current_timestamp();

        if (!prefix.empty()) {
            filename = prefix + "_" + filename;
        }

        if (!extension.empty()) {
            if (extension[0] != '.') {
                filename += ".";
            }
            filename += extension;
        }

        return directory / filename;
    }

    std::string get_file_size_human_readable(const fs::path& path, bool use_si_units) {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return "N/A";
        }

        uintmax_t size = fs::file_size(path);
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        const int unit_base = use_si_units ? 1000 : 1024;

        double size_d = static_cast<double>(size);
        int unit_index = 0;

        while (size_d >= unit_base && unit_index < 4) {
            size_d /= unit_base;
            unit_index++;
        }

        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << size_d << " " << units[unit_index];
        return ss.str();
    }

    bool is_path_inside_directory(const fs::path& parent, const fs::path& child) {
        auto parent_normal = fs::absolute(parent).lexically_normal();
        auto child_normal = fs::absolute(child).lexically_normal();

        auto relative = child_normal.lexically_relative(parent_normal);
        return !relative.empty() && relative != ".." &&
            relative.native().rfind("..", 0) != 0;
    }

    // ==================== РЕАЛИЗАЦИЯ БЕЗОПАСНЫХ ОПЕРАЦИЙ ====================

    void copy_file_safe(const fs::path& source_path,
        const fs::path& destination_path,
        bool overwrite) {
        if (!fs::exists(source_path)) {
            throw std::runtime_error("Source file does not exist: " + source_path.string());
        }

        if (!fs::is_regular_file(source_path)) {
            throw std::runtime_error("Source path is not a regular file: " + source_path.string());
        }

        if (fs::exists(destination_path)) {
            if (!overwrite) {
                throw std::runtime_error("Destination file already exists: " + destination_path.string());
            }
            if (!fs::is_regular_file(destination_path)) {
                throw std::runtime_error("Destination path is not a regular file: " + destination_path.string());
            }
        }

        ensure_directory_for_file(destination_path);

        std::error_code ec;
        fs::copy_file(source_path, destination_path,
            overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none, ec);

        if (ec) {
            throw std::runtime_error("Failed to copy file: " + ec.message());
        }
    }

    void move_file_safe(const fs::path& source_path, const fs::path& destination_path) {
        if (!fs::exists(source_path)) {
            throw std::runtime_error("Source file does not exist: " + source_path.string());
        }

        if (!fs::is_regular_file(source_path)) {
            throw std::runtime_error("Source path is not a regular file: " + source_path.string());
        }

        ensure_directory_for_file(destination_path);

        std::error_code ec;
        fs::rename(source_path, destination_path, ec);

        if (ec) {
            // Попробовать скопировать и удалить, если rename не сработал (например, между разными разделами)
            copy_file_safe(source_path, destination_path, true);
            fs::remove(source_path, ec); // Игнорируем ошибку удаления
        }
    }

} // namespace filehub