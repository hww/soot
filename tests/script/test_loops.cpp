#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class LoopTest : public ::testing::Test {
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

// Тест while
TEST_F(LoopTest, WhileLoop) {
    eval("(define counter 0)");
    eval("(while (< counter 3) (set! counter (+ counter 1)))");
    Object obj = eval("counter");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);
}