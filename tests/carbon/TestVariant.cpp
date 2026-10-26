#include "gtest/gtest.h"

#include "carbon/Export.hpp"
#include "carbon/gc/GC.hpp"

using namespace carbon;

class VariantTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gc::initialize(1, 16); }
    static void TearDownTestSuite() { gc::shutdown(); }
};

TEST_F(VariantTest, Construction) {
    Variant nil;
    EXPECT_TRUE(nil.is_null());

    Variant i32_val(42);
    EXPECT_TRUE(i32_val.is_int());
    EXPECT_EQ(i32_val.get_i32(), 42);

    Variant f32_val(3.14f);
    EXPECT_TRUE(f32_val.is_float());
    EXPECT_FLOAT_EQ(f32_val.get_f32(), 3.14f);

    Variant bool_val(true);
    EXPECT_TRUE(bool_val.is_int());
    EXPECT_TRUE(bool_val.get_bool());
}

TEST_F(VariantTest, StringOperations) {
    Variant str_val("hello");
    EXPECT_TRUE(str_val.is_ptr());
    EXPECT_EQ(str_val.get_string(), "hello"); // явный get_string — ок
    // to_string() no longer reads a Pointer as a C string: without
    // type information a Pointer may point to a structure, a function,
    // or an array, and reading it as a string would be undefined
    // behaviour. The typed formatter {:t=string} is the correct way
    // to print a string-valued Variant.
    EXPECT_EQ(fmt::format("{:t=string}", str_val), "\"hello\"");
}

TEST_F(VariantTest, Assignment) {
    Variant var;
    var.set_i32(100);
    EXPECT_EQ(var.get_i32(), 100);

    var = 2.5f;
    EXPECT_FLOAT_EQ(var.get_f32(), 2.5f);

    var = std::string("test");
    EXPECT_EQ(var.get_string(), "test");
}

TEST_F(VariantTest, Comparison) {
    Variant a(10);
    Variant b(10);
    Variant c(20);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST_F(VariantTest, Conversion) {
    Variant i32_val(42);
    EXPECT_EQ(i32_val.get_i32(), 42);
    EXPECT_FLOAT_EQ(i32_val.to_float(), 42.0f);
    EXPECT_TRUE(i32_val.to_bool());

    Variant f32_val(0.0f);
    EXPECT_EQ(f32_val.get_f32(), 0.0);
    EXPECT_EQ(f32_val.to_int(), 0);
    EXPECT_FALSE(f32_val.to_bool());
}