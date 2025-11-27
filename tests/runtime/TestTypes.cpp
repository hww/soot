#include "gtest/gtest.h"

#include "runtime/Export.hpp"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

TEST(Types, SafeCasting) {
    EXPECT_EQ(runtime::safe_cast_u32(1000u), 1000u);
    EXPECT_THROW(runtime::safe_cast_u32(UINT64_MAX), runtime::OverflowException);

    EXPECT_EQ(runtime::safe_cast_s32(1000), 1000);
    EXPECT_THROW(runtime::safe_cast_s32(INT64_MAX), runtime::OverflowException);
}

TEST(Types, Vector4) {
    runtime::Vector4 vec(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_EQ(vec.x, 1.0f);
    EXPECT_EQ(vec.to_string(), "(1, 2, 3, 4)");
}