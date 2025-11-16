#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class ListTest : public ::testing::Test {
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

// Тест создания пар
TEST_F(ListTest, PairCreation) {
    Object obj = eval("(cons 1 2)");
    EXPECT_TRUE(obj.is_pair());
    EXPECT_TRUE(obj.car().is_integer());
    EXPECT_EQ(obj.car().as_integer(), 1);
    EXPECT_TRUE(obj.cdr().is_integer());
    EXPECT_EQ(obj.cdr().as_integer(), 2);
}

// Тест car и cdr
TEST_F(ListTest, CarCdrOperations) {
    Object obj = eval("(car (cons 1 2))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(cdr (cons 1 2))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2);
}

// Тест списков
TEST_F(ListTest, ListOperations) {
    Object obj = eval("(list 1 2 3)");
    EXPECT_TRUE(obj.is_pair());

    // Проверяем структуру списка (1 2 3)
    Object first = obj.car();
    EXPECT_TRUE(first.is_integer());
    EXPECT_EQ(first.as_integer(), 1);

    Object second = obj.cdr().car();
    EXPECT_TRUE(second.is_integer());
    EXPECT_EQ(second.as_integer(), 2);

    Object third = obj.cdr().cdr().car();
    EXPECT_TRUE(third.is_integer());
    EXPECT_EQ(third.as_integer(), 3);

    Object end = obj.cdr().cdr().cdr();
    EXPECT_TRUE(end.is_empty_list());
}

// Тест length
TEST_F(ListTest, ListLength) {
    Object obj = eval("(length (list 1 2 3))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(length (list))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 0);
}

// Тест append
TEST_F(ListTest, ListAppend) {
    Object obj = eval("(append (list 1 2) (list 3 4))");
    EXPECT_TRUE(obj.is_pair());

    // Должен получиться список (1 2 3 4)
    EXPECT_EQ(obj.car().as_integer(), 1);
    EXPECT_EQ(obj.cdr().car().as_integer(), 2);
    EXPECT_EQ(obj.cdr().cdr().car().as_integer(), 3);
    EXPECT_EQ(obj.cdr().cdr().cdr().car().as_integer(), 4);
}