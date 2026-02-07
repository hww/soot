#include "common/sooti/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace script;

class QuasiquoteTest : public ::testing::Test {
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

// Тест quasiquote с unquote
TEST_F(QuasiquoteTest, QuasiquoteWithUnquote) {
    eval("(define x 5)");
    Object obj = eval("`(1 ,x 3)");
    EXPECT_TRUE(obj.is_pair());

    // Должен получиться список (1 5 3)
    EXPECT_EQ(obj.as_pair()->car.as_integer(), 1);
    EXPECT_EQ(obj.as_pair()->cdr.as_pair()->car.as_integer(), 5);
    EXPECT_EQ(obj.as_pair()->cdr.as_pair()->cdr.as_pair()->car.as_integer(), 3);
}

// Тест quasiquote с unquote-splicing
TEST_F(QuasiquoteTest, QuasiquoteWithUnquoteSplicing) {
    eval("(define lst '(2 3))");
    Object obj = eval("`(1 ,@lst 4)");
    EXPECT_TRUE(obj.is_pair());

    // Должен получиться список (1 2 3 4)
    EXPECT_EQ(obj.as_pair()->car.as_integer(), 1);
    EXPECT_EQ(obj.as_pair()->cdr.as_pair()->car.as_integer(), 2);
    EXPECT_EQ(obj.as_pair()->cdr.as_pair()->cdr.as_pair()->car.as_integer(), 3);
    EXPECT_EQ(obj.as_pair()->cdr.as_pair()->cdr.as_pair()->cdr.as_pair()->car.as_integer(), 4);
}