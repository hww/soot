#include <gtest/gtest.h>
#include "interpreter.h"

class QuasiquoteTest : public ::testing::Test {
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

// Тест quasiquote с unquote
TEST_F(QuasiquoteTest, QuasiquoteWithUnquote) {
    eval("(define x 5)");
    Object obj = eval("`(1 ,x 3)");
    EXPECT_TRUE(obj.is_pair());

    // Должен получиться список (1 5 3)
    EXPECT_EQ(obj.car().as_integer(), 1);
    EXPECT_EQ(obj.cdr().car().as_integer(), 5);
    EXPECT_EQ(obj.cdr().cdr().car().as_integer(), 3);
}

// Тест quasiquote с unquote-splicing
TEST_F(QuasiquoteTest, QuasiquoteWithUnquoteSplicing) {
    eval("(define lst '(2 3))");
    Object obj = eval("`(1 ,@lst 4)");
    EXPECT_TRUE(obj.is_pair());

    // Должен получиться список (1 2 3 4)
    EXPECT_EQ(obj.car().as_integer(), 1);
    EXPECT_EQ(obj.cdr().car().as_integer(), 2);
    EXPECT_EQ(obj.cdr().cdr().car().as_integer(), 3);
    EXPECT_EQ(obj.cdr().cdr().cdr().car().as_integer(), 4);
}