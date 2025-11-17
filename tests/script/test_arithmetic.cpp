#include <gtest/gtest.h>
#include "interpreter.h"

using namespace script;

class ArithmeticTest : public ::testing::Test {
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

// Тест сложения
TEST_F(ArithmeticTest, Addition) {
    Object obj = eval("(+ 1 2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3);

    obj = eval("(+ 1 2 3 4)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 10);

    obj = eval("(+ 1.5 2.5)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 4.0);
}

// Тест вычитания
TEST_F(ArithmeticTest, Subtraction) {
    Object obj = eval("(- 5 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2);

    obj = eval("(- 10 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 5);

    obj = eval("(- 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), -5);
}

// Тест умножения
TEST_F(ArithmeticTest, Multiplication) {
    Object obj = eval("(* 2 3)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 6);

    obj = eval("(* 2 3 4)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 24);
}

// Тест смешанных типов - теперь первый аргумент определяет тип
TEST_F(ArithmeticTest, MixedTypesFollowFirstArg) {
    // Первый аргумент integer → ВСЕ конвертируется в integer
    Object obj = eval("(+ 1 2.5)");
    EXPECT_TRUE(obj.is_integer());  // Теперь ожидаем integer!
    EXPECT_EQ(obj.as_integer(), 3); // 2.5 → 2 (отбрасывание дробной части)

    // Первый аргумент float → ВСЕ конвертируется в float  
    obj = eval("(+ 1.0 2)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.0);

    obj = eval("(+ 1.5 2)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.5);
}

// Тест деления (всегда float в OpenGOAL)
TEST_F(ArithmeticTest, Division) {
    Object obj = eval("(/ 6 2)");
    EXPECT_TRUE(obj.is_float());  // Всегда float для деления
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.0);

    obj = eval("(/ 5 2)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 2.5);
}

TEST_F(ArithmeticTest, FloatToIntegerTruncation) {
    // Проверяем что float аргументы обрезаются до integer при integer операциях
    Object obj = eval("(+ 1 2.9)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3); // 2.9 → 2

    obj = eval("(* 2 1.9)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2); // 1.9 → 1, затем 2 * 1 = 2
}

// Тест смешанных операций
TEST_F(ArithmeticTest, MixedOperations) {
    Object obj = eval("(+ (* 2 3) (- 5 1))");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 10);
}

// Тест смешанных типов (целые + вещественные)
TEST_F(ArithmeticTest, MixedTypes) {
    // Первый аргумент integer → ВСЕ конвертируется в integer (отбрасывание дробной части)
    Object obj = eval("(+ 1 2.5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3); // 2.5 → 2, затем 1 + 2 = 3

    obj = eval("(- 5 2.5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 3); // 2.5 → 2, затем 5 - 2 = 3

    obj = eval("(* 2 1.5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 2); // 1.5 → 1, затем 2 * 1 = 2

    // Первый аргумент float → ВСЕ конвертируется в float
    obj = eval("(+ 1.5 2)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.5);

    obj = eval("(- 5.0 2)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 3.0);

    obj = eval("(* 2.0 3)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), 6.0);
}

// Тест унарных операций
TEST_F(ArithmeticTest, UnaryOperations) {
    Object obj = eval("(- 5)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), -5);

    obj = eval("(- 5.5)");
    EXPECT_TRUE(obj.is_float());
    EXPECT_DOUBLE_EQ(obj.as_float(), -5.5);

    obj = eval("(+)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 0);

    obj = eval("(*)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 1);
}