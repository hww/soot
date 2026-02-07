#include "common/sooti/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace script;

class StringTest : public ::testing::Test {
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

// Тест string-length
TEST_F(StringTest, StringLength) {
    Object obj = eval("(string-length \"hello\")");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);

    obj = eval("(string-length \"\")");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 0);
}

// Тест string-ref
TEST_F(StringTest, StringRef) {
    Object obj = eval("(string-ref \"hello\" 0)");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), 'h');

    obj = eval("(string-ref \"hello\" 4)");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), 'o');
}

// Тест string-append
TEST_F(StringTest, StringAppend) {
    Object obj = eval("(string-append \"hello\" \" \" \"world\")");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "hello world");
}

// Тест substring
TEST_F(StringTest, Substring) {
    Object obj = eval("(string-substr \"hello world\" 0 5)");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "hello");

    obj = eval("(string-substr \"hello world\" 6 11)");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "world");
}

// Тест преобразований string <-> symbol
TEST_F(StringTest, StringSymbolConversions) {
    Object obj = eval("(string->symbol \"hello\")");
    EXPECT_TRUE(obj.is_symbol());

    obj = eval("(symbol->string 'world)");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "world");
}