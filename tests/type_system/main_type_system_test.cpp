#include <gtest/gtest.h>

int main_(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Running TypeSystem Test Suite..." << std::endl;
    std::cout << "===============================" << std::endl;

    int result = RUN_ALL_TESTS();

    std::cout << "===============================" << std::endl;
    if (result == 0) {
        std::cout << "All tests passed!" << std::endl;
    }
    else {
        std::cout << "Some tests failed!" << std::endl;
    }

    return result;
}