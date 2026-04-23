#include "common/soot/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace soot;

class VectorTest : public ::testing::Test {
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

// Тест создания векторов
TEST_F(VectorTest, VectorCreation) {
    Object obj = eval("(make-array 1 2 3)");
    EXPECT_TRUE(obj.is_vector());

    auto elements = *obj.as_array();
    EXPECT_EQ(elements.size(), 3);
    EXPECT_EQ(elements[0].as_integer(), 1);
    EXPECT_EQ(elements[1].as_integer(), 2);
    EXPECT_EQ(elements[2].as_integer(), 3);
}

// Тест vector-ref
TEST_F(VectorTest, VectorRef) {
    Object obj = eval("(array-ref (make-array 1 2 3) 0)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(array-ref (make-array 1 2 3) 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);
}

// Тест vector-set!
TEST_F(VectorTest, VectorSet) {
    eval("(define v (make-array 1 2 3))");
    eval("(array-set! v 1 99)");
    Object obj = eval("(array-ref v 1)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 99);
}

// Тест vector-length
TEST_F(VectorTest, VectorLength) {
    Object obj = eval("(array-length (make-array 1 2 3))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(array-length (make-array))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 0);
}