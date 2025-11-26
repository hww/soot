#include <gtest/gtest.h>
#include "common/util/Log.hpp"
#include <chrono>
#include <vector>

#include "runtime/Export.hpp"

using namespace runtime;
using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;


class NativeFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        NativeFunctionRegistry::get_instance().initialize_builtins();
    }

    void TearDown() override {
        // Очищаем реестр между тестами
        // Можно добавить метод clear() в реестр если нужно
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(NativeFunctionsTest, RegistryBasicOperations) {
    auto& registry = NativeFunctionRegistry::get_instance();

    // Тест регистрации и поиска
    auto test_func = [](u32 argc, const Variant* argv) -> Variant {
        return Variant(42);
        };

    registry.register_function("test_func", test_func);

    NativeFunction found = registry.find_function("test_func");
    EXPECT_NE(found, nullptr);

    // Тест вызова
    Variant result = found(0, nullptr);
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(NativeFunctionsTest, BuiltinFunctionsRegistered) {
    auto& registry = NativeFunctionRegistry::get_instance();

    EXPECT_NE(registry.find_function("print"), nullptr);
    EXPECT_NE(registry.find_function("println"), nullptr);
    EXPECT_NE(registry.find_function("add"), nullptr);
    EXPECT_NE(registry.find_function("sub"), nullptr);
    EXPECT_NE(registry.find_function("mul"), nullptr);
    EXPECT_NE(registry.find_function("div"), nullptr);
    EXPECT_NE(registry.find_function("abs"), nullptr);
    EXPECT_NE(registry.find_function("sqrt"), nullptr);
}

// ============================================================================
// SC_ARG Macro Tests
// ============================================================================

TEST_F(NativeFunctionsTest, SC_ARGSafeAccess) {
    // Тестируем безопасный доступ к аргументам
    Variant argv[] = {
        Variant(100),
        Variant(3.14f),
        Variant(SID("test_string"))
    };

    u32 argc = 3;

    // Корректный доступ к существующим аргументам
    EXPECT_EQ(SC_ARG(0, s32, -1), 100);
    EXPECT_FLOAT_EQ(SC_ARG(1, float, -1.0f), 3.14f);
    EXPECT_EQ(SC_ARG(2, StringId, SID("default")), SID("test_string"));

    // Безопасный доступ к несуществующим аргументам
    EXPECT_EQ(SC_ARG(5, s32, 999), 999);
    EXPECT_FLOAT_EQ(SC_ARG(10, float, 2.71f), 2.71f);
    EXPECT_EQ(SC_ARG(15, StringId, SID("fallback")), SID("fallback"));
}

// ============================================================================
// Arithmetic Functions Tests
// ============================================================================

TEST_F(NativeFunctionsTest, AddFunction) {
    auto add_func = NativeFunctionRegistry::get_instance().find_function("add");
    ASSERT_NE(add_func, nullptr);

    // Integer addition
    Variant int_args[] = { Variant(5), Variant(3) };
    Variant result = add_func(2, int_args);
    EXPECT_EQ(result.to_int(), 8);

    // Float addition
    Variant float_args[] = { Variant(2.5f), Variant(1.5f) };
    result = add_func(2, float_args);
    EXPECT_FLOAT_EQ(result.to_float(), 4.0f);

    // Mixed types (should promote to float)
    Variant mixed_args[] = { Variant(2), Variant(1.5f) };
    result = add_func(2, mixed_args);
    EXPECT_FLOAT_EQ(result.to_float(), 3.5f);

    // Not enough arguments
    result = add_func(0, nullptr);
    EXPECT_EQ(result.to_int(), 0);
}

TEST_F(NativeFunctionsTest, SubtractFunction) {
    auto sub_func = NativeFunctionRegistry::get_instance().find_function("sub");
    ASSERT_NE(sub_func, nullptr);

    Variant args[] = { Variant(10), Variant(3) };
    Variant result = sub_func(2, args);
    EXPECT_EQ(result.to_int(), 7);
}

TEST_F(NativeFunctionsTest, MultiplyFunction) {
    auto mul_func = NativeFunctionRegistry::get_instance().find_function("mul");
    ASSERT_NE(mul_func, nullptr);

    Variant args[] = { Variant(4), Variant(5) };
    Variant result = mul_func(2, args);
    EXPECT_EQ(result.to_int(), 20);
}

TEST_F(NativeFunctionsTest, DivideFunction) {
    auto div_func = NativeFunctionRegistry::get_instance().find_function("div");
    ASSERT_NE(div_func, nullptr);

    // Normal division
    Variant args[] = { Variant(15), Variant(3) };
    Variant result = div_func(2, args);
    EXPECT_EQ(result.to_int(), 5);

    // Float division
    Variant float_args[] = { Variant(10.0f), Variant(4.0f) };
    result = div_func(2, float_args);
    EXPECT_FLOAT_EQ(result.to_float(), 2.5f);

    // Division by zero (should handle safely)
    Variant zero_args[] = { Variant(10), Variant(0) };
    result = div_func(2, zero_args);
    EXPECT_EQ(result.to_int(), 0); // Should return default value
}

// ============================================================================
// Math Functions Tests
// ============================================================================

TEST_F(NativeFunctionsTest, AbsFunction) {
    auto abs_func = NativeFunctionRegistry::get_instance().find_function("abs");
    ASSERT_NE(abs_func, nullptr);

    // Positive integer
    Variant pos_args[] = { Variant(5) };
    Variant result = abs_func(1, pos_args);
    EXPECT_EQ(result.to_int(), 5);

    // Negative integer
    Variant neg_args[] = { Variant(-5) };
    result = abs_func(1, neg_args);
    EXPECT_EQ(result.to_int(), 5);

    // Float absolute value
    Variant float_args[] = { Variant(-3.14f) };
    result = abs_func(1, float_args);
    EXPECT_FLOAT_EQ(result.to_float(), 3.14f);
}

TEST_F(NativeFunctionsTest, SqrtFunction) {
    auto sqrt_func = NativeFunctionRegistry::get_instance().find_function("sqrt");
    ASSERT_NE(sqrt_func, nullptr);

    // Normal square root
    Variant args[] = { Variant(16.0f) };
    Variant result = sqrt_func(1, args);
    EXPECT_FLOAT_EQ(result.to_float(), 4.0f);

    // Square root of zero
    Variant zero_args[] = { Variant(0.0f) };
    result = sqrt_func(1, zero_args);
    EXPECT_FLOAT_EQ(result.to_float(), 0.0f);

    // Negative number (should handle safely)
    Variant neg_args[] = { Variant(-4.0f) };
    result = sqrt_func(1, neg_args);
    EXPECT_FLOAT_EQ(result.to_float(), 0.0f); // Should return default value
}

// ============================================================================
// Type Conversion Tests
// ============================================================================

TEST_F(NativeFunctionsTest, TypeConversionFunctions) {
    // Test to_int
    auto to_int_func = NativeFunctionRegistry::get_instance().find_function("to_int");
    ASSERT_NE(to_int_func, nullptr);

    Variant float_arg[] = { Variant(3.99f) };
    Variant result = to_int_func(1, float_arg);
    EXPECT_EQ(result.to_int(), 3); // Truncation

    // Test to_float
    auto to_float_func = NativeFunctionRegistry::get_instance().find_function("to_float");
    ASSERT_NE(to_float_func, nullptr);

    Variant int_arg[] = { Variant(5) };
    result = to_float_func(1, int_arg);
    EXPECT_FLOAT_EQ(result.to_float(), 5.0f);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================
/*
TEST_F(NativeFunctionsTest, EdgeCases) {
    auto add_func = NativeFunctionRegistry::get_instance().find_function("add");
    ASSERT_NE(add_func, nullptr);

    // Single argument
    Variant single_arg[] = { Variant(10) };
    Variant result = add_func(1, single_arg);
    EXPECT_EQ(result.to_int(), 10); // Should return the argument itself

    // No arguments
    result = add_func(0, nullptr);
    EXPECT_EQ(result.to_int(), 0);

    // Many arguments (only first two should be used)
    Variant many_args[] = { Variant(1), Variant(2), Variant(3), Variant(4) };
    result = add_func(4, many_args);
    EXPECT_EQ(result.to_int(), 3); // 1 + 2
}
*/
TEST_F(NativeFunctionsTest, FunctionNotFound) {
    auto& registry = NativeFunctionRegistry::get_instance();

    NativeFunction not_found = registry.find_function("non_existent_function");
    EXPECT_EQ(not_found, nullptr);

    not_found = registry.find_function("unknown_function_12345");
    EXPECT_EQ(not_found, nullptr);
}

// ============================================================================
// Custom Native Function Test
// ============================================================================

Variant custom_concat_function(u32 argc, const Variant* argv) {
    std::string result;
    for (u32 i = 0; i < argc; i++) {
        result += argv[i].to_string();
        if (i < argc - 1) result += " ";
    }
    return Variant(result);
}

TEST_F(NativeFunctionsTest, CustomFunctionRegistration) {
    auto& registry = NativeFunctionRegistry::get_instance();

    // Регистрируем кастомную функцию
    registry.register_function("concat", custom_concat_function);

    // Тестируем её
    NativeFunction concat_func = registry.find_function("concat");
    ASSERT_NE(concat_func, nullptr);

    Variant args[] = {
        Variant("Hello"),
        Variant("World"),
        Variant(123)
    };

    Variant result = concat_func(3, args);
    auto resultstr = result.to_string();
    EXPECT_EQ(resultstr, "Hello World 123");
}

// ============================================================================
// Performance Test (опционально)
// ============================================================================

TEST_F(NativeFunctionsTest, PerformanceTest) {
    auto add_func = NativeFunctionRegistry::get_instance().find_function("add");
    ASSERT_NE(add_func, nullptr);

    Variant args[] = { Variant(1), Variant(2) };

    const int iterations = 100000;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        Variant result = add_func(2, args);
        EXPECT_EQ(result.to_int(), 3);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    // Проверяем что производительность адекватная
    // (это скорее sanity check, чем настоящий бенчмарк)
    EXPECT_LT(duration.count(), iterations * 1000); // Менее 1ms per call в среднем

    lg::info("Performance test: {} calls in {} microseconds",
        iterations, duration.count());
}

// ============================================================================
// StringId Function Tests
// ============================================================================

Variant sid_test_function(u32 argc, const Variant* argv) {
    StringId name = SC_ARG(0, StringId, SID("default"));
    s32 count = SC_ARG(1, s32, 1);

    // Простая логика для теста
    return Variant(count * 10);
}

TEST_F(NativeFunctionsTest, StringIdArguments) {
    auto& registry = NativeFunctionRegistry::get_instance();
    registry.register_function("sid_test", sid_test_function);

    auto func = registry.find_function("sid_test");
    ASSERT_NE(func, nullptr);

    // Тест с StringId аргументом
    Variant args[] = {
        Variant(SID("test_object")),
        Variant(5)
    };

    Variant result = func(2, args);
    EXPECT_EQ(result.to_int(), 50);

    // Тест с недостаточным количеством аргументов
    Variant minimal_args[] = { Variant(SID("test")) };
    result = func(1, minimal_args);
    EXPECT_EQ(result.to_int(), 10); // count = 1 по умолчанию
}