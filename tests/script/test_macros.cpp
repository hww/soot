#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class MacroTest : public ::testing::Test {
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