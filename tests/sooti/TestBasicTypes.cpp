#include <gtest/gtest.h>
#include "common/sooti/Interpreter.hpp"

using namespace script;

class BasicTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        env = std::make_shared<EnvironmentObject>();
    }

    Object eval(const std::string& code) {
        Object obj = interp.get_reader().read_from_string(code, "test");
        return interp.eval_form(obj, env);
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

    obj = eval("#\\\\n");
    EXPECT_TRUE(obj.is_char());
    EXPECT_EQ(obj.as_char(), '\n');
}

// Тест 5: Пустой список
TEST_F(BasicTypesTest, EmptyList) {
    Object obj = eval("()");
    EXPECT_TRUE(obj.is_null());
}

// Тест 6: Строки
TEST_F(BasicTypesTest, StringLiterals) {
    Object obj = eval("\"hello\"");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "hello");

    obj = eval("\"test with spaces\"");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "test with spaces");
}

// Тест 7: Символы (идентификаторы)
TEST_F(BasicTypesTest, SymbolLiterals) {
    // Символы как литералы (без вычисления) - используем reader из interpreter

    Object obj1 = interp.get_reader().read_from_string("x", "test");
    auto& expressions1 = obj1.as_pair()->cdr;  // (x)
    auto& symbol_obj1 = expressions1.as_pair()->car;  // x
    EXPECT_TRUE(symbol_obj1.is_symbol());
    EXPECT_STREQ(symbol_obj1.as_symbol().name_ptr, "x");  // ← используем STREQ для C-строк

    Object obj2 = interp.get_reader().read_from_string("variable-name", "test");
    auto& expressions2 = obj2.as_pair()->cdr;  // (variable-name)
    auto& symbol_obj2 = expressions2.as_pair()->car;  // variable-name
    EXPECT_TRUE(symbol_obj2.is_symbol());
    EXPECT_STREQ(symbol_obj2.as_symbol().name_ptr, "variable-name");  // ← STREQ

    Object obj3 = interp.get_reader().read_from_string("+", "test");
    auto& expressions3 = obj3.as_pair()->cdr;  // (+)
    auto& symbol_obj3 = expressions3.as_pair()->car;  // +
    EXPECT_TRUE(symbol_obj3.is_symbol());
    EXPECT_STREQ(symbol_obj3.as_symbol().name_ptr, "+");  // ← STREQ
}