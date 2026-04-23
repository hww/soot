#include "common/soot/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace soot;

class ConversionTest : public ::testing::Test {
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

// ���� number->string
TEST_F(ConversionTest, NumberToString) {
    Object obj = eval("(number->string 42)");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "42");

    obj = eval("(number->string 3.14)");
    EXPECT_TRUE(obj.is_string());
}

// ���� string->number
TEST_F(ConversionTest, StringToNumber) {
    Object obj = eval("(string->number \"42\")");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 42);

    obj = eval("(string->number \"3.14\")");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.14);
}

// ���� char->integer � integer->char
TEST_F(ConversionTest, CharIntegerConversions) {
    Object obj = eval("(char->integer #\\A)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 65);

    obj = eval("(integer->char 65)");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), 'A');
}