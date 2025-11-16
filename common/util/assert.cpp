#include "assert.h"

#ifndef NO_ASSERT

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "log/log.h"

using namespace lg;

void private_assert_failed(const char* expr,
    const char* file,
    int line,
    const char* function,
    const char* msg) {
    if (!msg || msg[0] == '\0') {
        std::string log = fmt::format("Assertion failed: '{}'\n\tSource: {}:{}\n\tFunction: {}\n", expr,
            file, line, function);
        Logger::fatal("{}", log);  // ← ЗАМЕНИТЬ die на fatal
    }
    else {
        std::string log =
            fmt::format("Assertion failed: '{}'\n\tMessage: {}\n\tSource: {}:{}\n\tFunction: {}\n",
                expr, msg, file, line, function);
        Logger::fatal("{}", log);  // ← ЗАМЕНИТЬ die на fatal
    }
    abort();
}

void private_assert_failed(const char* expr,
    const char* file,
    int line,
    const char* function,
    const std::string_view& msg) {
    if (msg.empty()) {
        private_assert_failed(expr, file, line, function);
    }
    else {
        private_assert_failed(expr, file, line, function, msg.data());
    }
}

#endif