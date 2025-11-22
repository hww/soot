#include "gtest/gtest.h"
#include "vm/export.hpp"

TEST(Variant, Construction) {
    vm::Variant nil;
    EXPECT_TRUE(nil.is_null());

    vm::Variant i32_val(42);
    EXPECT_TRUE(i32_val.is_int());
    EXPECT_EQ(i32_val.get_int32(), 42);

    vm::Variant f32_val(3.14f);
    EXPECT_TRUE(f32_val.is_float());
    EXPECT_FLOAT_EQ(f32_val.get_float(), 3.14f);

    vm::Variant bool_val(true);
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_TRUE(bool_val.get_bool());
}

TEST(Variant, StringOperations) {
    vm::Variant str_val("hello");
    EXPECT_TRUE(str_val.is_string());
    EXPECT_EQ(str_val.get_string(), "hello");
    EXPECT_EQ(str_val.to_string(), "hello");
}

TEST(Variant, Assignment) {
    vm::Variant var;
    var = 100;
    EXPECT_EQ(var.get_int32(), 100);

    var = 2.5f;
    EXPECT_FLOAT_EQ(var.get_float(), 2.5f);

    var = std::string("test");
    EXPECT_EQ(var.get_string(), "test");
}

TEST(Variant, Comparison) {
    vm::Variant a(10);
    vm::Variant b(10);
    vm::Variant c(20);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Variant, Conversion) {
    vm::Variant i32_val(42);
    EXPECT_EQ(i32_val.get_int32(), 42);
    EXPECT_FLOAT_EQ(i32_val.to_float(), 42.0f);
    EXPECT_TRUE(i32_val.to_bool());

    vm::Variant f32_val(0.0f);
    EXPECT_EQ(f32_val.get_float(), 0.0);
    EXPECT_EQ(f32_val.to_int(), 0);
    EXPECT_FALSE(f32_val.to_bool());
}