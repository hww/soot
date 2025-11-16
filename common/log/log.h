#pragma once

#include <string>
#include <chrono>
#include <mutex>
#include <cstdio>
#include "fmt/format.h"
#include "fmt/color.h"

namespace lg {

    enum class Level {
        TRACE, DEBUG, INFO, WARN, ERROR, FATAL
    };

    class Logger {
    public:
        static void initialize();
        static void shutdown();

        static void setFile(const std::string& filename, bool append = false);
        static void setStdoutLevel(Level level);
        static void setFileLevel(Level level);
        static void enableColors(bool enable = true);

        // Основные функции с fmt - ОПРЕДЕЛЕНИЯ В ЗАГОЛОВОЧНОМ ФАЙЛЕ
        template<typename... Args>
        static void log(Level level, fmt::format_string<Args...> format, Args&&... args) {
            if (!initialized_) return;

            std::lock_guard<std::mutex> lock(mutex_);

            auto message = fmt::format(format, std::forward<Args>(args)...);
            auto timestamp = formatTime();
            auto levelStr = levelToString(level);
            auto fullMessage = fmt::format("{} [{}] {}", timestamp, levelStr, message);

            // Запись в файл
            if (level >= fileLevel_ && file_) {
                writeToFile(fullMessage);
            }

            // Запись в консоль
            if (level >= stdoutLevel_) {
                writeToConsole(fullMessage, level);
            }

            if (level == Level::FATAL) {
                shutdown();
                std::abort();
            }
        }

        template<typename... Args>
        static void trace(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::TRACE, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::DEBUG, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::INFO, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::WARN, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::ERROR, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void fatal(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::FATAL, format, std::forward<Args>(args)...);
        }

        // Для обратной совместимости
        template<typename... Args>
        static void die(fmt::format_string<Args...> format, Args&&... args) {
            log(Level::FATAL, format, std::forward<Args>(args)...);
        }

    private:
        static std::string formatTime();
        static std::string levelToString(Level level);
        static void writeToFile(const std::string& message);
        static void writeToConsole(const std::string& message, Level level);

        static std::mutex mutex_;
        static FILE* file_;
        static Level stdoutLevel_;
        static Level fileLevel_;
        static bool colorsEnabled_;
        static bool initialized_;
    };

} // namespace lg