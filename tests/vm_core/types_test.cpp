#include "gtest/gtest.h"
#include "types.hpp"

TEST(Types, SafeCasting) {
    EXPECT_EQ(vm::safe_cast_u32(1000u), 1000u);
    EXPECT_THROW(vm::safe_cast_u32(UINT64_MAX), vm::OverflowException);

    EXPECT_EQ(vm::safe_cast_s32(1000), 1000);
    EXPECT_THROW(vm::safe_cast_s32(INT64_MAX), vm::OverflowException);
}

TEST(Types, Vector4) {
    vm::Vector4 vec(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_EQ(vec.x, 1.0f);
    EXPECT_EQ(vec.to_string(), "(1, 2, 3, 4)");
}