#include <gtest/gtest.h>
#include "common/sooti/Interpreter.hpp"

using namespace script;

class SpecialFormsTest : public ::testing::Test {
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

// Тест quote
TEST_F(SpecialFormsTest, Quote) {
    Object obj = eval("(quote (1 2 3))");
    EXPECT_TRUE(obj.is_pair());

    obj = eval("'symbol");
    EXPECT_TRUE(obj.is_symbol());

    obj = eval("'(1 2 3)");
    EXPECT_TRUE(obj.is_pair());
}

// Тест define и set!
TEST_F(SpecialFormsTest, DefineAndSet) {
    eval("(define x 5)");
    Object obj = eval("x");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);

    eval("(set! x 10)");
    obj = eval("x");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 10);
}

// Тест if
TEST_F(SpecialFormsTest, IfExpression) {
    Object obj = eval("(if #t 1 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(if #f 1 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2);

    obj = eval("(if #t 1)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(if #f 1)");
    EXPECT_TRUE(obj.is_empty_list());
}

// Тест lambda
TEST_F(SpecialFormsTest, Lambda) {
    eval("(define add (lambda (x y) (+ x y)))");
    Object obj = eval("(add 3 4)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 7);
}

// Тест let
TEST_F(SpecialFormsTest, Let) {
    Object obj = eval("(let ((x 5) (y 3)) (+ x y))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 8);
}

// Тест begin
TEST_F(SpecialFormsTest, Begin) {
    Object obj = eval("(begin 1 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    eval("(define x 0)");
    eval("(begin (set! x 1) (set! x 2))");
    obj = eval("x");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2);
}