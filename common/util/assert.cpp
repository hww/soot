#include "assert.h"

#ifndef NO_ASSERT

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "util/log.h"

#ifdef ASSET_WITH_TERMINATE

void private_assert_failed(const char* expr,
    const char* file,
    int line,
    const char* function,
    const char* msg) {
    if (!msg || msg[0] == '\0') {
        std::string log = fmt::format("Assertion failed: '{}'\n\tSource: {}:{}\n\tFunction: {}\n", expr,
            file, line, function);
        lg::die("{}", log);  // ← ЗАМЕНИТЬ die на fatal
    }
    else {
        std::string log =
            fmt::format("Assertion failed: '{}'\n\tMessage: {}\n\tSource: {}:{}\n\tFunction: {}\n",
                expr, msg, file, line, function);
        lg::die("{}", log);  // ← ЗАМЕНИТЬ die на fatal
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

#else // ifdef ASSET_WITH_TERMINATE

[[noreturn]] void private_assert_failed(const char* expr,
    const char* file,
    int line,
    const char* function,
    const char* msg) {
    std::string message = fmt::format("Assertion failed: '{}'\n\tMessage: {}\n\tSource: {}:{}\n\tFunction: {}",
        expr, msg, file, line, function);

    lg::error("{}", message);
    throw AssertionException(message);
}

[[noreturn]] void private_assert_failed(const char* expr,
    const char* file,
    int line,
    const char* function,
    const std::string_view& msg) {
    private_assert_failed(expr, file, line, function, std::string(msg).c_str());
}

#endif // ifdef ASSET_WITH_TERMINATE

#endif