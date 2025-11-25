// FileHub.cpp - âńĺ đĺŕëčçŕöčč
#include "file_util.h"
#include <fstream>
#include <regex>

namespace file_util {

    // ==================== ÎŃÍÎÂÍŰĹ ÓŇČËČŇŰ ====================

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

    // ==================== ĐŔÁÎŇŔ Ń ĎÓŇßĚČ ====================

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

    // ==================== ĐŔÁÎŇŔ Ń ÄČĐĹĘŇÎĐČßĚČ ====================

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

    // ==================== ĎÎČŃĘ ÔŔÉËÎÂ ====================

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

    // ==================== ĎĐÎÂĹĐĘČ Č ČÍÔÎĐĚŔÖČß ====================

    uintmax_t get_size(const fs::path& path) {
        return fs::file_size(path);
    }

    // ==================== ÎĎĹĐŔÖČČ Ń ÔŔÉËŔĚČ ====================

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

    // ========== ÎĎĹĐŔÖČČ Ń ÔŔÉËŔĚČ ÄËß ÝŇÎĂÎ ĎĐÎĹĘŇŔ ============

    fs::path get_project_dir()
    {
        return fs::current_path();
    }

    std::string get_file_path(const std::vector<std::string>& input) {
        // TODO - clean this behaviour up, it causes unexpected behaviour when working with files
        // the project path should be explicitly provided by whatever if needed
        // TEMP HACK
        // - if the provided path is absolute, don't add the project path
        if (input.size() == 1 && fs::path(input.at(0)).is_absolute()) {
            return input.at(0);
        }

        auto current_path = get_project_dir();
        for (auto& str : input) {
            current_path /= str;
        }

        return current_path.string();
    }

} // namespace filehub