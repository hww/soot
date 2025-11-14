#include <gtest/gtest.h>
#include "interpreter.h"
#include <cmath>

class MathTest : public ::testing::Test {
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

// Тест abs
TEST_F(MathTest, Abs) {
    Object obj = eval("(abs 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);

    obj = eval("(abs -5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);

    obj = eval("(abs -3.14)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.14);
}

// Тест max и min
TEST_F(MathTest, MaxMin) {
    Object obj = eval("(max 1 3 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(min 1 3 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);
}

// Тест expt
TEST_F(MathTest, Expt) {
    Object obj = eval("(expt 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 8);

    obj = eval("(expt 4 0.5)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 2.0);
}

// Тест sqrt
TEST_F(MathTest, Sqrt) {
    Object obj = eval("(sqrt 4)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 2.0);

    obj = eval("(sqrt 2.25)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 1.5);
}