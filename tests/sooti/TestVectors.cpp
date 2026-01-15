#include <gtest/gtest.h>
#include "common/sooti/Interpreter.hpp"

using namespace script;

class VectorTest : public ::testing::Test {
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

// Тест создания векторов
TEST_F(VectorTest, VectorCreation) {
    Object obj = eval("(vector 1 2 3)");
    EXPECT_TRUE(obj.is_vector());

    auto elements = *obj.as_array();
    EXPECT_EQ(elements.size(), 3);
    EXPECT_EQ(elements[0].as_integer(), 1);
    EXPECT_EQ(elements[1].as_integer(), 2);
    EXPECT_EQ(elements[2].as_integer(), 3);
}

// Тест vector-ref
TEST_F(VectorTest, VectorRef) {
    Object obj = eval("(vector-ref (vector 1 2 3) 0)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);

    obj = eval("(vector-ref (vector 1 2 3) 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);
}

// Тест vector-set!
TEST_F(VectorTest, VectorSet) {
    eval("(define v (vector 1 2 3))");
    eval("(vector-set! v 1 99)");
    Object obj = eval("(vector-ref v 1)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 99);
}

// Тест vector-length
TEST_F(VectorTest, VectorLength) {
    Object obj = eval("(vector-length (vector 1 2 3))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(vector-length (vector))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 0);
}