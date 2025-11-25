/*!
 * @file assert.h
 * Custom ASSERT macro
 */

#pragma once

#ifndef NO_ASSERT

#include <string_view>
#include <string_view>
#include <stdexcept>

class AssertionException : public std::runtime_error {
public:
    AssertionException(const std::string& msg) : std::runtime_error(msg) {}
};


[[noreturn]] void private_assert_failed(const char* expr,
                                        const char* file,
                                        int line,
                                        const char* function,
                                        const char* msg = "");

[[noreturn]] void private_assert_failed(const char* expr,
                                        const char* file,
                                        int line,
                                        const char* function,
                                        const std::string_view& msg);

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define ASSERT(EX) \
  (void)((EX) || (private_assert_failed(#EX, __FILE__, __LINE__, __PRETTY_FUNCTION__), 0))

#define ASSERT_NOT_REACHED() \
  (void)((private_assert_failed("not reached", __FILE__, __LINE__, __PRETTY_FUNCTION__), 0))

#define ASSERT_MSG(EXPR, STR) \
  (void)((EXPR) || (private_assert_failed(#EXPR, __FILE__, __LINE__, __PRETTY_FUNCTION__, STR), 0))

#define ASSERT_FORMAT(EXPR, FORMAT, ...) \
  ASSERT_MSG(EXPR, fmt::format(FORMAT, __VA_ARGS__))

#define ASSERT_NOT_REACHED_MSG(STR) \
  (void)((private_assert_failed("not reached", __FILE__, __LINE__, __PRETTY_FUNCTION__, STR), 0))

#define ASSERT_EQ_IMM(EXPR, EXPECTED) \
  ASSERT_MSG((EXPR) == (EXPECTED), fmt::format("result was {}, expected {}", (EXPR), (EXPECTED)))
#else

#define ASSERT(EX) ((void)0)
#define ASSERT_MSG(EXPR, STR) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define ASSERT_NOT_REACHED_MSG(STR) ((void)0)
#define ASSERT_EQ_IMM(EXPR, EXPECTED) ((void)0)

#endif
