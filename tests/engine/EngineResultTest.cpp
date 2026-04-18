#include <gtest/gtest.h>
#include "common/carbon/kernel/EngineResult.hpp"

using namespace carbon;


TEST(EngineResultTest, EnumValues)
{
    EXPECT_EQ(static_cast<int>(EEngineResult::None), 0);
    EXPECT_EQ(static_cast<int>(EEngineResult::Dead), 1);

    EEngineResult none = EEngineResult::None;
    EEngineResult dead = EEngineResult::Dead;

    EXPECT_NE(none, dead);
}
