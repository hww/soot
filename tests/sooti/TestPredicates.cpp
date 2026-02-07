#include "common/sooti/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace script;

class PredicatesTest : public ::testing::Test {
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

// Тест type predicates
TEST_F(PredicatesTest, TypePredicates) {
    Object obj = eval("(number? 5)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(number? 3.14)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(string? \"hello\")");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(symbol? 'x)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(pair? (cons 1 2))");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(null? ())");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(boolean? #t)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(char? #\\a)");
    EXPECT_TRUE(obj.as_boolean());
}

// Тест сравнений
TEST_F(PredicatesTest, ComparisonPredicates) {
    Object obj = eval("(= 5 5)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(= 5 6)");
    EXPECT_FALSE(obj.as_boolean());

    obj = eval("(< 1 2)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(> 3 2)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(<= 2 2)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(>= 3 3)");
    EXPECT_TRUE(obj.as_boolean());
}

// Тест eq? и eqv?
TEST_F(PredicatesTest, EqualityPredicates) {
    Object obj = eval("(eq? 5 5)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(eq? 'a 'a)");
    EXPECT_TRUE(obj.as_boolean());
}