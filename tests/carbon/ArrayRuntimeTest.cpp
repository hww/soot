#include <gtest/gtest.h>

#include "carbon/gc/GC.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/NativeArray.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeString.hpp"

namespace carbon::testing {

    namespace {

        Variant make_i32_array() {
            Variant kind("i32", RuntimeType::Pointer);
            return native_make_array(1, &kind);
        }

    } // namespace

    TEST(ArrayRuntime, MakeEmpty) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();

        Variant a = make_i32_array();
        ASSERT_TRUE(a.is_ptr());
        ASSERT_NE(a.get_ptr(), nullptr);

        Variant size = native_array_size(1, &a);
        EXPECT_EQ(size.get_i64(), 0);

        Variant cap = native_array_cap(1, &a);
        EXPECT_EQ(cap.get_i64(), 0);

        gc::shutdown();
    }

    TEST(ArrayRuntime, PushOne) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();

        Variant a = make_i32_array();
        Variant v(static_cast<i64>(42));
        Variant args[2] = {a, v};
        native_array_push(2, args);

        Variant size = native_array_size(1, &a);
        EXPECT_EQ(size.get_i64(), 1);

        Variant cap = native_array_cap(1, &a);
        EXPECT_GT(cap.get_i64(), 0);

        gc::shutdown();
    }

    TEST(ArrayRuntime, PushMany) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();

        Variant a = make_i32_array();
        for (i32 i = 0; i < 100; ++i) {
            Variant v(static_cast<i64>(i));
            Variant args[2] = {a, v};
            native_array_push(2, args);
        }

        Variant size = native_array_size(1, &a);
        EXPECT_EQ(size.get_i64(), 100);

        Variant data = native_array_data(1, &a);
        ASSERT_TRUE(data.is_ptr());
        const i32 *elems = static_cast<const i32 *>(data.get_ptr());
        for (i32 i = 0; i < 100; ++i) { EXPECT_EQ(elems[i], i); }

        gc::shutdown();
    }

    TEST(ArrayRuntime, Clear) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();

        Variant a = make_i32_array();
        for (i32 i = 0; i < 10; ++i) {
            Variant v(static_cast<i64>(i));
            Variant args[2] = {a, v};
            native_array_push(2, args);
        }

        native_array_clear(1, &a);
        Variant size = native_array_size(1, &a);
        EXPECT_EQ(size.get_i64(), 0);

        Variant cap = native_array_cap(1, &a);
        EXPECT_GT(cap.get_i64(), 0);

        gc::shutdown();
    }

    TEST(ArrayRuntime, DifferentElementKinds) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();

        Variant kind_i32("i32", RuntimeType::Pointer);
        Variant a_i32 = native_make_array(1, &kind_i32);
        EXPECT_TRUE(a_i32.is_ptr());

        Variant kind_f32("f32", RuntimeType::Pointer);
        Variant a_f32 = native_make_array(1, &kind_f32);
        EXPECT_TRUE(a_f32.is_ptr());

        Variant kind_ptr("ptr", RuntimeType::Pointer);
        Variant a_ptr = native_make_array(1, &kind_ptr);
        EXPECT_TRUE(a_ptr.is_ptr());

        EXPECT_NE(a_i32.get_ptr(), a_f32.get_ptr());
        EXPECT_NE(a_f32.get_ptr(), a_ptr.get_ptr());

        gc::shutdown();
    }

    TEST(ArrayRuntime, PtrArrayIsMarked) {
        gc::initialize(16, 256);
        NativeArray::initialize_builtins();
        NativeString::initialize_builtins();

        Variant kind("ptr", RuntimeType::Pointer);
        Variant arr = native_make_array(1, &kind);
        ASSERT_TRUE(arr.is_ptr());

        Variant s_arg("hello", RuntimeType::Pointer);
        Variant s = native_string_from_c(1, &s_arg);
        ASSERT_TRUE(s.is_ptr());

        Variant push_args[2] = {arr, s};
        native_array_push(2, push_args);

        // Mark only the array; the string must stay alive through the
        // array's mark delegate.
        gc::start_pass();
        gc::mark_object(arr.get_ptr());
        while (!gc::sweep_pass(1 << 20)) {}
        gc::finish_pass();

        EXPECT_GE(gc::live_objects(), 2u);

        gc::shutdown();
    }

} // namespace carbon::testing