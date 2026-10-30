#pragma once

#include <ctime>
#include <string>

#ifdef __linux__
#include <sys/time.h>
#endif

#include "fmt/color.h"
#include "fmt/format.h"

namespace lg {

#ifdef __linux__
    struct LogTime {
        timeval tv;
    };
#else
    struct LogTime {
        time_t tim;
    };
#endif

    /**
     * @brief Log severity levels.
     *
     * @details Levels are ordered: a higher value includes everything
     *          below it. `trace` and `debug` are for compiler
     *          development; `info` and `warn` are for normal use;
     *          `error` is for recoverable failures; `die` logs and
     *          aborts.
     *
     *          `off` and `off_unless_die` are internal states used by
     *          tools that must be silent. They are NOT user-selectable
     *          levels and are not exposed on the command line.
     */
    enum class level {
        trace = 0,
        debug = 1,
        info = 2,
        warn = 3,
        error = 4,
        die = 5,

        /// @brief Suppress all output.
        off = 6,

        /// @brief Suppress everything except `die`.
        off_unless_die = 7
    };

    std::string log_level_to_string(level l);
    level       string_to_log_level(std::string s);

    namespace internal {
        void log_message(level log_level, LogTime &now, const char *message);
    } // namespace internal

    /// @brief Open a log file as a SECOND destination.
    ///
    /// @details Console output (stderr) is unaffected: it is always
    ///          on, filtered by set_console_level(). This call only
    ///          adds a file next to it. It never enables or disables
    ///          console logging.
    void set_file(const std::string &filename, bool should_rotate = true, bool append = false);

    /// @brief Set the minimum level written to stderr.
    ///
    /// @details Renamed from set_stdout_level: diagnostics go to
    ///          stderr, not stdout, so the old name was misleading.
    ///          The deprecated alias below is kept for one release.
    void set_console_level(level log_level);

    /// @brief Deprecated. Use set_console_level().
    [[deprecated("use set_console_level()")]] void set_stdout_level(level log_level);

    /// @brief Set the minimum level written to the log file (if any).
    void set_file_level(level log_level);

    void disable_ansi_colors();

    /// @brief Enable platform-specific console features.
    ///
    /// @details On Windows this turns on VT processing so that ANSI
    ///          color codes are interpreted. On other platforms it is
    ///          a no-op. It does NOT install any log sinks: console
    ///          output is always on.
    void initialize();

    void finish();

    template <typename... Args>
    void log(level log_level, const std::string &format, Args &&...args) {
        LogTime now;
#ifdef __linux__
        gettimeofday(&now.tv, nullptr);
#else
        now.tim = time(nullptr);
#endif
        std::string formatted_message =
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
        internal::log_message(log_level, now, formatted_message.c_str());
    }

    template <typename... Args> void trace(const std::string &f, Args &&...a) {
        log(level::trace, f, std::forward<Args>(a)...);
    }
    template <typename... Args> void debug(const std::string &f, Args &&...a) {
        log(level::debug, f, std::forward<Args>(a)...);
    }
    template <typename... Args> void info(const std::string &f, Args &&...a) {
        log(level::info, f, std::forward<Args>(a)...);
    }
    template <typename... Args> void warn(const std::string &f, Args &&...a) {
        log(level::warn, f, std::forward<Args>(a)...);
    }
    template <typename... Args> void error(const std::string &f, Args &&...a) {
        log(level::error, f, std::forward<Args>(a)...);
    }
    template <typename... Args> void die(const std::string &f, Args &&...a) {
        log(level::die, f, std::forward<Args>(a)...);
    }

    void log_print(const char *message);

    template <typename... Args> void print(const std::string &format, Args &&...args) {
        std::string formatted_message =
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
        log_print(formatted_message.c_str());
    }
    template <typename... Args>
    void print(const fmt::text_style &ts, const std::string &format, Args &&...args) {
        std::string formatted_message = fmt::vformat(ts, format, fmt::make_format_args(args...));
        log_print(formatted_message.c_str());
    }
} // namespace lg