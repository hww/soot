#include <gtest/gtest.h>
#include "interpreter.h"

class SystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        env = std::make_shared<EnvironmentObject>();
    }

    Object eval(const std::string& code) {
        Object obj = interp.get_reader().read_from_string(code, "test");
        return interp.eval_with_rewind(obj, env);
    }

    Interpreter interp;
    std::shared_ptr<EnvironmentObject> env;
};

// Тест file-exists?
TEST_F(SystemTest, FileExists) {
    Object obj = eval("(file-exists? \"nonexistent.txt\")");
    EXPECT_TRUE(obj.is_boolean());
    EXPECT_FALSE(obj.as_boolean());
}

// Тест current-directory
TEST_F(SystemTest, CurrentDirectory) {
    Object obj = eval("(current-directory)");
    EXPECT_TRUE(obj.is_string());
    // Не можем проверить точное значение, но можем проверить что это не пустая строка
    EXPECT_FALSE(obj.as_string().empty());
}