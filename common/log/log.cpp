#include "log.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lg {

    // Определения статических переменных
    std::mutex Logger::mutex_;
    FILE* Logger::file_ = nullptr;
    Level Logger::stdoutLevel_ = Level::INFO;
    Level Logger::fileLevel_ = Level::DEBUG;
    bool Logger::colorsEnabled_ = true;
    bool Logger::initialized_ = false;

    void Logger::initialize() {
        std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(hStdOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hStdOut, mode);
        }
#endif

        initialized_ = true;
    }

    std::string Logger::formatTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string Logger::levelToString(Level level) {
        static const char* names[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
        return names[static_cast<int>(level)];
    }

    void Logger::writeToFile(const std::string& message) {
        if (file_) {
            fmt::print(file_, "{}\n", message);
            fflush(file_);
        }
    }

    void Logger::writeToConsole(const std::string& message, Level level) {
        static const fmt::color colors[] = {
            fmt::color::gray, fmt::color::cyan, fmt::color::green,
            fmt::color::yellow, fmt::color::red, fmt::color::purple
        };

        if (colorsEnabled_) {
            fmt::print(fg(colors[static_cast<int>(level)]), "{}\n", message);
        }
        else {
            fmt::print("{}\n", message);
        }
        fflush(stdout);
    }

    void Logger::setFile(const std::string& filename, bool append) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_) {
            fclose(file_);
            file_ = nullptr;
        }
        file_ = fopen(filename.c_str(), append ? "a" : "w");
        if (!file_) {
            // Если не удалось открыть файл, пишем в stderr
            fmt::print(stderr, "Failed to open log file: {}\n", filename);
        }
    }

    void Logger::setStdoutLevel(Level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        stdoutLevel_ = level;
    }

    void Logger::setFileLevel(Level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        fileLevel_ = level;
    }

    void Logger::enableColors(bool enable) {
        std::lock_guard<std::mutex> lock(mutex_);
        colorsEnabled_ = enable;
    }

    void Logger::shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_) {
            fflush(file_);
            fclose(file_);
            file_ = nullptr;
        }
        initialized_ = false;
    }

} // namespace lg