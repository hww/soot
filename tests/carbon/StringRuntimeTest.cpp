#include <gtest/gtest.h>

#include "carbon/gc/GC.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeString.hpp"

namespace carbon::testing {

    namespace {

        Variant from_c(const char *s) {
            Variant arg(s, RuntimeType::Pointer);
            return native_string_from_c(1, &arg);
        }

    } // namespace

    TEST(StringRuntime, FromC) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant s = from_c("hello");
        ASSERT_TRUE(s.is_ptr());
        ASSERT_NE(s.get_ptr(), nullptr);
        EXPECT_STREQ(static_cast<const char *>(s.get_ptr()), "hello");

        gc::shutdown();
    }

    TEST(StringRuntime, Len) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant s = from_c("hello");
        Variant len = native_string_len(1, &s);
        EXPECT_EQ(len.get_i64(), 5);

        Variant empty = from_c("");
        Variant len0 = native_string_len(1, &empty);
        EXPECT_EQ(len0.get_i64(), 0);

        gc::shutdown();
    }

    TEST(StringRuntime, Concat) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant a = from_c("foo");
        Variant b = from_c("bar");
        Variant args[2] = {a, b};
        Variant c = native_string_concat(2, args);
        ASSERT_TRUE(c.is_ptr());
        EXPECT_STREQ(static_cast<const char *>(c.get_ptr()), "foobar");

        gc::shutdown();
    }

    TEST(StringRuntime, Eq) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant a = from_c("hello");
        Variant b = from_c("hello");
        Variant c = from_c("world");

        Variant args1[2] = {a, b};
        EXPECT_TRUE(native_string_eq(2, args1).get_bool());

        Variant args2[2] = {a, c};
        EXPECT_FALSE(native_string_eq(2, args2).get_bool());

        gc::shutdown();
    }

    TEST(StringRuntime, Cstr) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant s = from_c("hello");
        Variant r = native_string_cstr(1, &s);
        EXPECT_EQ(r.get_ptr(), s.get_ptr());

        gc::shutdown();
    }

    TEST(StringRuntime, Alloc) {
        gc::initialize(16, 256);
        NativeString::initialize_builtins();

        Variant size_arg(static_cast<i64>(16));
        Variant s = native_string_alloc(1, &size_arg);
        ASSERT_TRUE(s.is_ptr());
        ASSERT_NE(s.get_ptr(), nullptr);

        // Alloc zero-fills, so the string is empty.
        Variant len = native_string_len(1, &s);
        EXPECT_EQ(len.get_i64(), 0);

        gc::shutdown();
    }

} // namespace carbon::testing