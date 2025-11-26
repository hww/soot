#include "gtest/gtest.h"

#include "runtime/Export.hpp"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

TEST(Variant, Construction) {
    Variant nil;
    EXPECT_TRUE(nil.is_null());

    Variant i32_val(42);
    EXPECT_TRUE(i32_val.is_int());
    EXPECT_EQ(i32_val.get_int32(), 42);

    Variant f32_val(3.14f);
    EXPECT_TRUE(f32_val.is_float());
    EXPECT_FLOAT_EQ(f32_val.get_float(), 3.14f);

    Variant bool_val(true);
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_TRUE(bool_val.get_bool());
}

TEST(Variant, StringOperations) {
    Variant str_val("hello");
    EXPECT_TRUE(str_val.is_string());
    EXPECT_EQ(str_val.get_string(), "hello");
    EXPECT_EQ(str_val.to_string(), "hello");
}

TEST(Variant, Assignment) {
    Variant var;
    var = 100;
    EXPECT_EQ(var.get_int32(), 100);

    var = 2.5f;
    EXPECT_FLOAT_EQ(var.get_float(), 2.5f);

    var = std::string("test");
    EXPECT_EQ(var.get_string(), "test");
}

TEST(Variant, Comparison) {
    Variant a(10);
    Variant b(10);
    Variant c(20);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Variant, Conversion) {
    Variant i32_val(42);
    EXPECT_EQ(i32_val.get_int32(), 42);
    EXPECT_FLOAT_EQ(i32_val.to_float(), 42.0f);
    EXPECT_TRUE(i32_val.to_bool());

    Variant f32_val(0.0f);
    EXPECT_EQ(f32_val.get_float(), 0.0);
    EXPECT_EQ(f32_val.to_int(), 0);
    EXPECT_FALSE(f32_val.to_bool());
}