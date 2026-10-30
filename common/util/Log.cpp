#include "util/Log.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

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
        bool  initialized = false;
        FILE *fp = nullptr;

        // Console output is always on. The default matches the value
        // advertised in --help and in the options table: "warn".
        level console_level = level::warn;

        // File output is opt-in (set_file). When a file is opened,
        // its filter defaults to the console level, so a single
        // `--log debug` affects both destinations. The caller may
        // override with set_file_level().
        level file_level = level::warn;

        bool       disable_colors = false;
        std::mutex mutex;
    };

    Logger gLogger;

    namespace internal {

        const char      *log_level_names[] = {"trace", "debug", "info", "warn",
                                              "error", "die",   "off",  "off_unless_die"};
        const fmt::color log_colors[] = {fmt::color::gray,   fmt::color::cyan, fmt::color::green,
                                         fmt::color::yellow, fmt::color::red,  fmt::color::purple};

        
        std::string log_level_to_string(level l) {
            return std::string(log_level_names[static_cast<int>(l)]);
        }

        level string_to_log_level(std::string s) {
            for (int i = 0; i < 8; ++i) {
                if (s == log_level_names[i]) return static_cast<level>(i);
            }
            return level::info;
        }

        std::string format_time(LogTime &now) {
#ifdef __linux__
            char   date_time_buffer[128];
            time_t now_seconds = now.tv.tv_sec;
            auto   now_milliseconds = now.tv.tv_usec / 1000;
            strftime(date_time_buffer, 128, "%H:%M:%S", localtime(&now_seconds));
            return fmt::format("{}:{:03d}", date_time_buffer, now_milliseconds);
#else
            char date_time_buffer[128];
            strftime(date_time_buffer, 128, "%H:%M:%S", localtime(&now.tim));
            return std::string(date_time_buffer);
#endif
        }

        void log_message(level log_level, LogTime &now, const char *message) {
            std::lock_guard<std::mutex> lock(gLogger.mutex);

            auto time_string = format_time(now);

            // ---- File (optional second destination) ----
            if (gLogger.fp && log_level >= gLogger.file_level) {
                std::string file_string =
                    fmt::format("{} [{}] {}\n", time_string,
                                log_level_names[static_cast<int>(log_level)], message);
                fwrite(file_string.c_str(), file_string.length(), 1, gLogger.fp);
                fflush(gLogger.fp);
            }

            // ---- Console (always on, stderr) ----
            //
            // Diagnostics go to stderr, not stdout. This keeps the
            // real program output separate from log lines, and makes
            // them visible in IDE output panes that show stderr.
            if (log_level >= gLogger.console_level) {
                fmt::print(stderr, "{} [", time_string);
                if (gLogger.disable_colors) {
                    fmt::print(stderr, "{}", log_level_names[static_cast<int>(log_level)]);
                } else {
                    fmt::print(stderr, fg(log_colors[static_cast<int>(log_level)]), "{}",
                               log_level_names[static_cast<int>(log_level)]);
                }
                fmt::print(stderr, "] {}\n", message);
                fflush(stderr);
            }

            if (log_level == level::die) {
                if (gLogger.fp) fflush(gLogger.fp);
                fflush(stderr);
                std::abort();
            }
        }

    } // namespace internal

    void log_print(const char *message) {
        // Direct, non-leveled output. Kept as-is: callers use it for
        // things that are not diagnostics (REPL prompts, banners).
        std::lock_guard<std::mutex> lock(gLogger.mutex);

        if (gLogger.fp) {
            std::string msg(message);
            fwrite(msg.c_str(), msg.length(), 1, gLogger.fp);
            fflush(gLogger.fp);
        }

        if (gLogger.console_level != level::off && gLogger.console_level != level::off_unless_die) {
            fmt::print(stderr, "{}", message);
            fflush(stderr);
        }
    }

    void set_file(const std::string &filename, bool should_rotate, bool append) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);

        if (gLogger.fp) {
            fclose(gLogger.fp);
            gLogger.fp = nullptr;
        }

        std::string final_filename = filename;

        if (should_rotate) {
            time_t now = time(nullptr);
            char   time_buffer[128];
            strftime(time_buffer, sizeof(time_buffer), "%Y%m%d_%H%M%S", localtime(&now));
            final_filename = filename + "." + time_buffer + ".log";

            try {
                fs::path log_dir = fs::path(filename).parent_path();
                if (log_dir.empty()) log_dir = ".";

                std::string           base_name = fs::path(filename).stem().string();
                std::vector<fs::path> log_files;

                for (const auto &entry : fs::directory_iterator(log_dir)) {
                    if (entry.is_regular_file()) {
                        std::string stem = entry.path().stem().string();
                        if (stem.find(base_name) == 0) { log_files.push_back(entry.path()); }
                    }
                }

                std::sort(log_files.begin(), log_files.end(),
                          [](const fs::path &a, const fs::path &b) {
                              return fs::last_write_time(a) > fs::last_write_time(b);
                          });

                for (size_t i = 10; i < log_files.size(); i++) { fs::remove(log_files[i]); }
            } catch (...) {
                // Best-effort rotation. Never fail the program because
                // an old log file could not be deleted.
            }
        } else {
            if (final_filename.find(".log") == std::string::npos) { final_filename += ".log"; }
        }

        gLogger.fp = fopen(final_filename.c_str(), append ? "a" : "w");

        // When a file is opened, mirror the console level unless the
        // caller already chose a different file level.
        if (gLogger.fp) { gLogger.file_level = gLogger.console_level; }
    }

    void set_console_level(level log_level) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.console_level = log_level;
    }

    void set_stdout_level(level log_level) {
        // Deprecated alias. Remove after one release.
        set_console_level(log_level);
    }

    void set_file_level(level log_level) {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.file_level = log_level;
    }

    void disable_ansi_colors() {
        std::lock_guard<std::mutex> lock(gLogger.mutex);
        gLogger.disable_colors = true;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(gLogger.mutex);

#ifdef _WIN32
        HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD  mode;
        if (GetConsoleMode(hStdErr, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hStdErr, mode);
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

} // namespace lg