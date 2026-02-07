#include "common/sooti/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace script;

class SystemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        env = interp.get_global_environment().as_env();
    }

    Object eval(const std::string &code) {
        return interp.eval_string(code, "test");
    }

    Interpreter        interp;
    EnvironmentObject *env;
};

// Тест file-exists?
TEST_F(SystemTest, FileExists) {
    Object obj = eval("(file-exists? \"nonexistent.txt\")");
    EXPECT_TRUE(obj.is_boolean());
    EXPECT_FALSE(obj.as_boolean());
}

// Тест current-directory
TEST_F(SystemTest, CurrentDirectory) {
    Object obj = eval("(get-path 'cwd)");
    EXPECT_TRUE(obj.is_string());
    // Не можем проверить точное значение, но можем проверить что это не пустая строка
    EXPECT_FALSE(obj.as_string()->empty());
}