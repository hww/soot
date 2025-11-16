#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class BasicTypesTest : public ::testing::Test {
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

// Тест 1: Целые числа
TEST_F(BasicTypesTest, IntegerLiterals) {
    Object obj = eval("42");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 42);

    obj = eval("-123");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), -123);
}

// Тест 2: Вещественные числа
TEST_F(BasicTypesTest, FloatLiterals) {
    Object obj = eval("3.14");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.14);

    obj = eval("-2.718");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), -2.718);
}

// Тест 3: Булевы значения
TEST_F(BasicTypesTest, BooleanLiterals) {
    Object obj_true = eval("#t");
    Object obj_false = eval("#f");

    EXPECT_TRUE(obj_true.is_boolean());
    EXPECT_TRUE(obj_false.is_boolean());
    EXPECT_TRUE(obj_true.as_boolean());
    EXPECT_FALSE(obj_false.as_boolean());
}

// Тест 4: Символы
TEST_F(BasicTypesTest, CharLiterals) {
    Object obj = eval("#\\a");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), 'a');

    obj = eval("#\\newline");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), '\n');
}

// Тест 5: Пустой список
TEST_F(BasicTypesTest, EmptyList) {
    Object obj = eval("()");
    EXPECT_TRUE(obj.is_empty_list());
}

// Тест 6: Строки
TEST_F(BasicTypesTest, StringLiterals) {
    Object obj = eval("\"hello\"");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string(), "hello");

    obj = eval("\"test with spaces\"");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string(), "test with spaces");
}

// Тест 7: Символы (идентификаторы)
TEST_F(BasicTypesTest, SymbolLiterals) {
    // Символы как литералы (без вычисления) - используем reader из interpreter
    Object obj = interp.get_reader().read_from_string("x", "test");
    EXPECT_TRUE(obj.is_symbol());
    EXPECT_EQ(obj.as_symbol(), "x");

    obj = interp.get_reader().read_from_string("variable-name", "test");
    EXPECT_TRUE(obj.is_symbol());
    EXPECT_EQ(obj.as_symbol(), "variable-name");

    obj = interp.get_reader().read_from_string("+", "test");
    EXPECT_TRUE(obj.is_symbol());
    EXPECT_EQ(obj.as_symbol(), "+");
}