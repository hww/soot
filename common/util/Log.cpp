#include "common/util/Log.hpp"
#include <cstdio>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace lg {

    struct Logger {
        bool initialized = false;
        FILE* fp = nullptr;
        level stdout_log_level = level::info;
        level file_log_level = level::debug;
        bool disable_colors = false;
        std::mutex mutex;
    };

    Logger gLogger;

    namespace internal {

        const char* log_level_names[] = { "trace", "debug", "info", "warn", "error", "die", "off", "off_unless_die" };
        const fmt::color log_colors[] = {
            fmt::color::gray, fmt::color::cyan, fmt::color::green,
            fmt::color::yellow, fmt::color::red, fmt::color::purple
        };

        std::string format_time(LogTime& now) {
#ifdef __linux__
            char date_time_buffer[128];
            time_t now_seconds = now.tv.tv_sec;
            auto now_milliseconds = now.tv.tv_usec / 1000;
            strftime(date_time_buffer, 128, "%H:%M:%S", localtime(&now_seconds));
            return fmt::format("{}:{:03d}", date_time_buffer, now_milliseconds);
#else
            char date_time_buffer[128];
            strftime(date_time_buffer, 128, "%H:%M:%S", localtime(&now.tim));
            return std::string(date_time_buffer);
#endif
        }

        void log_message(level log_level, LogTime& now, const char* message) {
            std::lock_guard<std::mutex> lock(gLogger.mutex);

            auto time_string = format_time(now);

            if (gLogger.fp && log_level >= gLogger.file_log_level) {
                std::string file_string = fmt::format("{} [{}] {}\n", time_string,
                    log_level_names[static_cast<int>(log_level)], message);
                fwrite(file_string.c_str(), file_string.length(), 1, gLogger.fp);
                fflush(gLogger.fp);
            }

            if (log_level >= gLogger.stdout_log_level) {
                fmt::print("{} [", time_string);
                if (gLogger.disable_colors) {
                    fmt::print("{}", log_level_names[static_cast<int>(log_level)]);
                }
                else {
                    fmt::print(fg(log_colors[static_cast<int>(log_level)]), "{}",
                        log_level_names[static_cast<int>(log_level)]);
                }
                fmt::print("] {}\n", message);
                fflush(stdout);
            }

            if (log_level == level::die) {
                if (gLogger.fp) fflush(gLogger.fp);
                fflush(stdout);
                std::abort();
            }
        }

    }  // namespace internal

    void log_print(const char* message) {
        // Мы всегда немедленно флашим вывод, так как без уровня логирования
        // это может быть что угодно - от фатальной ошибки до отладочного сообщения
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        
        if (gLogger.fp) {
            // Логируем в файл
            std::string msg(message);
            fwrite(msg.c_str(), msg.length(), 1, gLogger.fp);
            fflush(gLogger.fp);
        }

        // Исправляем проверку: off_unless_die - это специальный уровень,
        // который выключает всё кроме die
        if (gLogger.stdout_log_level != level::off && 
            gLogger.stdout_log_level != level::off_unless_die) {
            fmt::print("{}", message);
            fflush(stdout);
            fflush(stderr);
        }
    }

    // Упрощенная ротация логов - максимум 10 файлов
    void set_file(const std::string& filename, bool should_rotate, bool append) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);

        if (gLogger.fp) {
            fclose(gLogger.fp);
            gLogger.fp = nullptr;
        }

        std::string final_filename = filename;

        if (should_rotate) {
            // Добавляем timestamp к имени файла
            time_t now = time(nullptr);
            char time_buffer[128];
            strftime(time_buffer, sizeof(time_buffer), "%Y%m%d_%H%M%S", localtime(&now));
            final_filename = filename + "." + time_buffer + ".log";

            // Удаляем старые логи (оставляем только 10 последних)
            try {
                fs::path log_dir = fs::path(filename).parent_path();
                if (log_dir.empty()) log_dir = ".";

                std::string base_name = fs::path(filename).stem().string();
                std::vector<fs::path> log_files;

                // Ищем все файлы логов
                for (const auto& entry : fs::directory_iterator(log_dir)) {
                    if (entry.is_regular_file()) {
                        std::string stem = entry.path().stem().string();
                        if (stem.find(base_name) == 0) { // начинается с base_name
                            log_files.push_back(entry.path());
                        }
                    }
                }

                // Сортируем по времени изменения (новые первыми)
                std::sort(log_files.begin(), log_files.end(),
                    [](const fs::path& a, const fs::path& b) {
                        return fs::last_write_time(a) > fs::last_write_time(b);
                    });

                // Удаляем старые (оставляем 10 файлов)
                for (size_t i = 10; i < log_files.size(); i++) {
                    fs::remove(log_files[i]);
                }

            }
            catch (...) {
                // Игнорируем ошибки при ротации
            }
        }
        else {
            if (final_filename.find(".log") == std::string::npos) {
                final_filename += ".log";
            }
        }

        gLogger.fp = fopen(final_filename.c_str(), append ? "a" : "w");
    }

    void set_stdout_level(level log_level) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.stdout_log_level = log_level;
    }

    void set_file_level(level log_level) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.file_log_level = log_level;
    }

    void disable_ansi_colors() {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.disable_colors = true;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(gLogger.mutex);

#ifdef _WIN32
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(hStdOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hStdOut, mode);
        }
#endif

        gLogger.initialized = true;
    }

    void finish() {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        if (gLogger.fp) {
            fclose(gLogger.fp);
            gLogger.fp = nullptr;
        }
    }

}  // namespace lg
