#include <gtest/gtest.h>
#include "EngineResult.h"

using namespace vm;
    TEST(EngineResultTest, EnumValues)
    {
        EXPECT_EQ(static_cast<int>(EEngineResult::None), 0);
        EXPECT_EQ(static_cast<int>(EEngineResult::Dead), 1);

        EEngineResult none = EEngineResult::None;
        EEngineResult dead = EEngineResult::Dead;

        EXPECT_NE(none, dead);
    }
