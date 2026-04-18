#include "gtest/gtest.h"

#include "carbon/Export.hpp"

using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;

TEST(Variant, Construction) {
    Variant nil;
    EXPECT_TRUE(nil.is_null());

    Variant i32_val(42);
    EXPECT_TRUE(i32_val.is_int());
    EXPECT_EQ(i32_val.get_i32(), 42);

    Variant f32_val(3.14f);
    EXPECT_TRUE(f32_val.is_float());
    EXPECT_FLOAT_EQ(f32_val.get_f32(), 3.14f);

    Variant bool_val(true);
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_TRUE(bool_val.get_bool());
}

TEST(Variant, StringOperations) {
    Variant str_val("hello");
    EXPECT_TRUE(str_val.is_ptr());
    EXPECT_EQ(str_val.get_string(), "hello");
    EXPECT_EQ(str_val.to_string(), "hello");
}

TEST(Variant, Assignment) {
    Variant var;
    var.set_i32(100);
    EXPECT_EQ(var.get_i32(), 100);

    var = 2.5f;
    EXPECT_FLOAT_EQ(var.get_f32(), 2.5f);

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
    EXPECT_EQ(i32_val.get_i32(), 42);
    EXPECT_FLOAT_EQ(i32_val.to_float(), 42.0f);
    EXPECT_TRUE(i32_val.to_bool());

    Variant f32_val(0.0f);
    EXPECT_EQ(f32_val.get_f32(), 0.0);
    EXPECT_EQ(f32_val.to_int(), 0);
    EXPECT_FALSE(f32_val.to_bool());
}