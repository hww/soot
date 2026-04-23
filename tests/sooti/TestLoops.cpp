#include "common/soot/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace soot;

class LoopTest : public ::testing::Test {
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

// Тест while
TEST_F(LoopTest, WhileLoop) {
    eval("(define counter 0)");
    eval("(while (< counter 3) (set! counter (+ counter 1)))");
    Object obj = eval("counter");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);
}