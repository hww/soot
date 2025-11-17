#include <gtest/gtest.h>
#include "type_system/export.h"
#include "script/export.h"
#include "reader.h"
#include "fmt/format.h"

class DefEnumTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = type_system_create();
        type_system_initialize_builtin_types(ts);
    }

    void TearDown() override {
        type_system_destroy(ts);
    }

    TypeSystem* ts;
    script::Reader reader;
};
TEST_F(DefEnumTest, DebugSimpleEnum) {
    std::string code = "(defenum test :a :b :c)";
    auto obj = reader.read_from_string(code, "test");
    fmt::print("DEBUG: Parsed object: {}\n", obj.print());

    EnumType* test_enum = parse_defenum(obj, ts);

    ASSERT_NE(test_enum, nullptr);
    fmt::print("DEBUG: Enum name: '{}'\n", test_enum->base.base.name);
    EXPECT_STREQ(test_enum->base.base.name, "test");
}

TEST_F(DefEnumTest, BasicEnum) {
    std::string code = "(defenum color :red :green :blue)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* color_enum = parse_defenum(obj, ts);

    ASSERT_NE(color_enum, nullptr);
    EXPECT_STREQ(color_enum->base.base.name, "color");
    EXPECT_FALSE(color_enum->is_bitfield);
}

TEST_F(DefEnumTest, BitfieldEnum) {
    std::string code = "(defenum flags :bitfield #t :read 1 :write 2 :execute 4)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* flags_enum = parse_defenum(obj, ts);

    ASSERT_NE(flags_enum, nullptr);
    EXPECT_STREQ(flags_enum->base.base.name, "flags");
    EXPECT_TRUE(flags_enum->is_bitfield);
}

TEST_F(DefEnumTest, EnumWithExplicitValues) {
    // ÈÑÏÐÀÂËÅÍÍÛÉ ÔÎÐÌÀÒ: (:name value)
    std::string code = "(defenum weapon (:pistol 0) (:shotgun 1) (:rocket 2))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* weapon_enum = parse_defenum(obj, ts);

    ASSERT_NE(weapon_enum, nullptr);
    EXPECT_STREQ(weapon_enum->base.base.name, "weapon");
}

TEST_F(DefEnumTest, EnumWithType) {
    std::string code = "(defenum small :type int8 :value1 :value2)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* small_enum = parse_defenum(obj, ts);

    ASSERT_NE(small_enum, nullptr);
    EXPECT_STREQ(small_enum->base.base.name, "small");
}

TEST_F(DefEnumTest, EnumWithDocstring) {
    std::string code = R"(
        (defenum state 
          "State machine states"
          :idle
          :running
          :finished)
    )";
    auto obj = reader.read_from_string(code, "test");

    DefinitionMetadata metadata;
    EnumType* state_enum = parse_defenum(obj, ts, &metadata);

    ASSERT_NE(state_enum, nullptr);
    EXPECT_STREQ(state_enum->base.base.name, "state");
    EXPECT_TRUE(metadata.has_docstring());
    EXPECT_EQ(metadata.get_docstring_or_empty(), "State machine states");
}

TEST_F(DefEnumTest, MixedAutoAndExplicitValues) {
    // ÈÑÏÐÀÂËÅÍÍÛÉ ÔÎÐÌÀÒ
    std::string code = "(defenum mixed :first (:second 10) :third)";
    auto obj = reader.read_from_string(code, "test");
    
    EnumType* mixed_enum = parse_defenum(obj, ts);
    
    ASSERT_NE(mixed_enum, nullptr);
    EXPECT_STREQ(mixed_enum->base.base.name, "mixed");
}

TEST_F(DefEnumTest, ComplexTypeSpec) {
    std::string code = "(defenum complex :type uint32 :bitfield #f :flag1 1 :flag2 2)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* complex_enum = parse_defenum(obj, ts);

    ASSERT_NE(complex_enum, nullptr);
    EXPECT_STREQ(complex_enum->base.base.name, "complex");
}

TEST_F(DefEnumTest, InvalidBitfieldValue) {
    std::string code = "(defenum flags :bitfield invalid :read 1)";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, InvalidEnumValue) {
    // ÈÑÏÐÀÂËÅÍÍÛÉ ÔÎÐÌÀÒ - èñïîëüçóåì ïàðó äëÿ ÿâíîãî çíà÷åíèÿ
    std::string code = "(defenum test (:value \"not-a-number\"))";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}
TEST_F(DefEnumTest, DuplicateEntries) {
    std::string code = "(defenum test :value1 :value2 :value1)"; // äóáëèêàò
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, EmptyEnum) {
    std::string code = "(defenum empty)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* empty_enum = parse_defenum(obj, ts);

    ASSERT_NE(empty_enum, nullptr);
    EXPECT_STREQ(empty_enum->base.base.name, "empty");
}

TEST_F(DefEnumTest, EnumWithFalseBitfield) {
    std::string code = "(defenum normal :bitfield #f :option1 :option2)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* normal_enum = parse_defenum(obj, ts);

    ASSERT_NE(normal_enum, nullptr);
    EXPECT_STREQ(normal_enum->base.base.name, "normal");
    EXPECT_FALSE(normal_enum->is_bitfield);
}

TEST_F(DefEnumTest, LargeEnum) {
    std::string code = R"(
        (defenum big-enum
          :value0 :value1 :value2 :value3 :value4
          :value5 :value6 :value7 :value8 :value9)
    )";
    auto obj = reader.read_from_string(code, "test");

    EnumType* big_enum = parse_defenum(obj, ts);

    ASSERT_NE(big_enum, nullptr);
    EXPECT_STREQ(big_enum->base.base.name, "big-enum");
}

TEST_F(DefEnumTest, EnumWithNegativeValues) {
    std::string code = "(defenum signed :negative -1 :zero 0 :positive 1)";
    auto obj = reader.read_from_string(code, "test");

    EnumType* signed_enum = parse_defenum(obj, ts);

    ASSERT_NE(signed_enum, nullptr);
    EXPECT_STREQ(signed_enum->base.base.name, "signed");
}

TEST_F(DefEnumTest, InvalidBaseType) {
    std::string code = "(defenum test :type nonexistent-type :value1)";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}