// test_parsers.h
#include "test_parsers.h"
#include "test_data.h"

TEST_F(ParserTest, BasicEnumParsing) {
    ASSERT_TRUE(test_parse_defenum(TestData::BASIC_ENUM, "test-basic-enum"));

    auto* enum_type = ts.try_enum_lookup("test-basic-enum");
    ASSERT_NE(enum_type, nullptr);
    ASSERT_EQ(enum_type->entries().size(), 3);
    ASSERT_EQ(enum_type->entries().at("value1"), 0);
    ASSERT_EQ(enum_type->entries().at("value2"), 1);
    ASSERT_EQ(enum_type->entries().at("value3"), 2);
}