#pragma once

#include "gtest/gtest.h"
#include "common/util/Log.hpp"

namespace vm::test {

    // ============================================================================
    // Exception Testing Macros
    // ============================================================================

#define EXPECT_ASSERT(expression) \
    do { \
        bool caught = false; \
        try { \
            expression; \
        } catch (const std::exception& e) { \
            caught = true; \
            lg::debug("Caught expected exception: {}", e.what()); \
        } catch (...) { \
            caught = true; \
            lg::debug("Caught expected unknown exception"); \
        } \
        EXPECT_TRUE(caught) << "Expected assertion for: " << #expression; \
    } while (0)

#define EXPECT_NO_ASSERT(expression) \
    do { \
        bool caught = false; \
        std::string error_msg; \
        try { \
            expression; \
        } catch (const std::exception& e) { \
            caught = true; \
            error_msg = e.what(); \
        } catch (...) { \
            caught = true; \
            error_msg = "Unknown exception"; \
        } \
        EXPECT_FALSE(caught) << "Unexpected assertion for: " << #expression \
                            << "\nError: " << error_msg; \
    } while (0)

// ============================================================================
// Utility Functions for Testing
// ============================================================================

    template<typename T>
    void print_test_info(const std::string& test_name, const T& value) {
        lg::debug("Test {}: {}", test_name, value);
    }

    inline void setup_test_environment() {
        lg::set_stdout_level(lg::level::debug);
    }

} // namespace vm::test