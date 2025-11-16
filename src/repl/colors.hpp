#pragma once
#include <string>

namespace colors {
    // Простые ANSI цвета
    inline std::string reset() { return "\033[0m"; }
    inline std::string red(const std::string& text) { return "\033[31m" + text + reset(); }
    inline std::string green(const std::string& text) { return "\033[32m" + text + reset(); }
    inline std::string yellow(const std::string& text) { return "\033[33m" + text + reset(); }
    inline std::string blue(const std::string& text) { return "\033[34m" + text + reset(); }
    inline std::string magenta(const std::string& text) { return "\033[35m" + text + reset(); }
    inline std::string cyan(const std::string& text) { return "\033[36m" + text + reset(); }
    
    // Стили
    inline std::string bold(const std::string& text) { return "\033[1m" + text + reset(); }
    inline std::string underline(const std::string& text) { return "\033[4m" + text + reset(); }
}