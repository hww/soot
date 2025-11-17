#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class LogicalTest : public ::testing::Test {
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

// Тест and
TEST_F(LogicalTest, And) {
    Object obj = eval("(and #t #t)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(and #t #f)");
    EXPECT_FALSE(obj.as_boolean());

    obj = eval("(and 1 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(and #t 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);
}

// Тест or
TEST_F(LogicalTest, Or) {
    Object obj = eval("(or #f #t)");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(or #f #f)");
    EXPECT_FALSE(obj.as_boolean());

    obj = eval("(or 1 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(or #f 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);
}

// Тест not (через if)
TEST_F(LogicalTest, NotViaIf) {
    Object obj = eval("(if #f #t #f)");
    EXPECT_FALSE(obj.as_boolean());

    obj = eval("(if #t #t #f)");
    EXPECT_TRUE(obj.as_boolean());
}

// Тест cond
TEST_F(LogicalTest, Cond) {
    Object obj = eval("(cond (#t 1) (#t 2))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(cond (#f 1) (#t 2))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2);

    obj = eval("(cond (#f 1) (#f 2) (else 3))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);
}