#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Running Z80 Lisp Test Suite..." << std::endl;
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