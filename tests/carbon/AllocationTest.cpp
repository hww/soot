#include <gtest/gtest.h>

#include "carbon/Carbon.hpp"
#include "carbon/gc/GC.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/NativeAllocators.hpp"
#include "carbon/vm/NativeFunc.hpp"

namespace carbon::testing {

    TEST(Allocation, AllocateVector) {
        gc::initialize(16, 256);
        NativeAllocators::initialize_builtins();

        auto          &reg = NativeFunctionRegistry::get_instance();
        NativeFunction alloc = reg.find_function_by_name("allocate");
        ASSERT_NE(alloc, nullptr);

        Variant args[2] = {Variant("heap", RuntimeType::Pointer),
                           Variant(static_cast<i64>(SID("vector")))};
        Variant result = alloc(2, args);

        ASSERT_TRUE(result.is_ptr());
        ASSERT_NE(result.get_ptr(), nullptr);

        // Block is zero-initialized.
        const float *fields = static_cast<const float *>(result.get_ptr());
        EXPECT_FLOAT_EQ(fields[0], 0.0f);
        EXPECT_FLOAT_EQ(fields[1], 0.0f);
        EXPECT_FLOAT_EQ(fields[2], 0.0f);
        EXPECT_FLOAT_EQ(fields[3], 0.0f);

        gc::shutdown();
    }

    TEST(Allocation, AllocateI64) {
        gc::initialize(16, 256);
        NativeAllocators::initialize_builtins();

        auto          &reg = NativeFunctionRegistry::get_instance();
        NativeFunction alloc = reg.find_function_by_name("allocate-silent");

        Variant args[2] = {Variant("heap", RuntimeType::Pointer),
                           Variant(static_cast<i64>(SID("i64")))};
        Variant result = alloc(2, args);

        ASSERT_TRUE(result.is_ptr());
        ASSERT_NE(result.get_ptr(), nullptr);

        EXPECT_EQ(*static_cast<i64 *>(result.get_ptr()), 0);

        gc::shutdown();
    }

    TEST(Allocation, UnknownTypeReturnsNull) {
        gc::initialize(16, 256);
        NativeAllocators::initialize_builtins();

        auto          &reg = NativeFunctionRegistry::get_instance();
        NativeFunction alloc = reg.find_function_by_name("allocate-silent");

        Variant args[2] = {Variant("heap", RuntimeType::Pointer),
                           Variant(static_cast<i64>(SID("not-a-type")))};
        Variant result = alloc(2, args);

        EXPECT_TRUE(result.is_ptr());
        EXPECT_EQ(result.get_ptr(), nullptr);

        gc::shutdown();
    }

    TEST(Allocation, MissingArgsReturnsNull) {
        gc::initialize(16, 256);
        NativeAllocators::initialize_builtins();

        auto          &reg = NativeFunctionRegistry::get_instance();
        NativeFunction alloc = reg.find_function_by_name("allocate-silent");
        ASSERT_NE(alloc, nullptr);

        // 0 arguments
        Variant result0 = alloc(0, nullptr);
        EXPECT_TRUE(result0.is_ptr());
        EXPECT_EQ(result0.get_ptr(), nullptr);

        // 1 argument
        Variant args1[1] = {Variant("heap", RuntimeType::Pointer)};
        Variant result1 = alloc(1, args1);
        EXPECT_TRUE(result1.is_ptr());
        EXPECT_EQ(result1.get_ptr(), nullptr);

        gc::shutdown();
    }
} // namespace carbon::testing