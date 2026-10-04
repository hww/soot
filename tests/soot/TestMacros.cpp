#include "common/soot/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace soot;

class MacroTest : public ::testing::Test {
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

// Тест простого макроса
TEST_F(MacroTest, SimpleMacro) {
    eval("(define double (macro (x) (list '+ x x)))");
    Object obj = eval("(double 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 10);
}

// Тест макроса с несколькими параметрами
TEST_F(MacroTest, MultiParamMacro) {
    eval("(define add-two (macro (x y) (list '+ x y)))");
    Object obj = eval("(add-two 3 4)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 7);
}