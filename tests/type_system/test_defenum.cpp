#include <gtest/gtest.h>
#include "type_system/type_system.h"
#include "type_system/defenum.h"
#include "reader.h"
#include "fmt/format.h"
#include "script/export.h"

class DefEnumTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = new TypeSystem();
        ts->add_builtin_types();
    }

    void TearDown() override {
        delete ts;
    }

    TypeSystem* ts;
    script::Reader reader;
};
TEST_F(DefEnumTest, DebugEmptyListCheck) {
    std::string code = "(defenum test a b c)";
    auto obj = reader.read_from_string(code, "test");

    fmt::print("=== EMPTY LIST ANALYSIS ===\n");

    // Пройдем до конца списка
    auto* current = &obj;
    int level = 0;

    while (current->is_pair()) {
        auto* pair = current->as_pair();
        fmt::print("Level {}: CAR={}, CDR={}\n", level, pair->car.print(), pair->cdr.print());

        // Проверим тип CDR
        fmt::print("  CDR type check: ");
        fmt::print("{}\n", script::pretty_print::to_string(pair->cdr)); // Добавьте этот метод в Object

        current = &pair->cdr;
        level++;

        if (level > 10) break; // защита от бесконечного цикла
    }

    fmt::print("Final CDR: {}\n", current->print());
    fmt::print("Final CDR is_pair: {}\n", current->is_pair());
    fmt::print("Final CDR is_empty_list: {}\n", current->is_empty_list());
    fmt::print("Final CDR type enum: {}\n", static_cast<int>(current->type));
}
TEST_F(DefEnumTest, DebugParsing) {
    std::string code = "(defenum test a b c)";
    auto obj = reader.read_from_string(code, "test");

    fmt::print("=== PARSED STRUCTURE ===\n");
    fmt::print("Full: {}\n", obj.print());

    if (obj.is_pair()) {
        auto first = obj.as_pair()->car;
        fmt::print("First: {}\n", first.print());

        auto rest = obj.as_pair()->cdr;
        if (rest.is_pair()) {
            auto second = rest.as_pair()->car;
            fmt::print("Second: {}\n", second.print());
            // и т.д.
        }
    }
};
TEST_F(DefEnumTest, DebugSimpleEnum) {
    std::string code = "(defenum test (a) (b) (c))";  // БЕЗ двоеточий!
    auto obj = reader.read_from_string(code, "test");
    fmt::print("DEBUG: Parsed object: {}\n", obj.print());

    try {
        EnumType* test_enum = parse_defenum(obj, ts);
        ASSERT_NE(test_enum, nullptr);
        fmt::print("DEBUG: Enum name: '{}'\n", test_enum->get_name());
        EXPECT_EQ(test_enum->get_name(), "test");
    }
    catch (const std::exception& e) {
        fmt::print("ERROR: {}\n", e.what());
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, BasicEnum) {
    // ПРАВИЛЬНО: значения без двоеточий
    std::string code = "(defenum color (red) (green) (blue))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* color_enum = parse_defenum(obj, ts);
    ASSERT_NE(color_enum, nullptr);
    EXPECT_EQ(color_enum->get_name(), "color");
    EXPECT_FALSE(color_enum->is_bitfield());
}



TEST_F(DefEnumTest, BitfieldEnum) {
    // ПРАВИЛЬНО: опции с :, значения без
    std::string code = "(defenum flags :bitfield #t (read) (write) (execute))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* flags_enum = parse_defenum(obj, ts);
    ASSERT_NE(flags_enum, nullptr);
    EXPECT_EQ(flags_enum->get_name(), "flags");
    EXPECT_TRUE(flags_enum->is_bitfield());
}

TEST_F(DefEnumTest, EnumWithExplicitValues) {
    // ПРАВИЛЬНО: пары (name value)
    std::string code = "(defenum weapon (pistol 0) (shotgun 1) (rocket 2))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* weapon_enum = parse_defenum(obj, ts);
    ASSERT_NE(weapon_enum, nullptr);
    EXPECT_EQ(weapon_enum->get_name(), "weapon");
}

TEST_F(DefEnumTest, EnumWithType) {
    // ПРАВИЛЬНО: опция :type, значения без двоеточий
    std::string code = "(defenum small :type int32 (value1) (value2))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* small_enum = parse_defenum(obj, ts);
    ASSERT_NE(small_enum, nullptr);
    EXPECT_EQ(small_enum->get_name(), "small");
}

TEST_F(DefEnumTest, EnumWithDocstring) {
    std::string code = R"(
        (defenum state 
          "State machine states"
          (idle)
          (running)
          (finished))
    )";
    auto obj = reader.read_from_string(code, "test");

    try {
        DefinitionMetadata metadata;
        EnumType* state_enum = parse_defenum(obj, ts, &metadata);
        ASSERT_NE(state_enum, nullptr);
        EXPECT_EQ(state_enum->get_name(), "state");
        EXPECT_TRUE(metadata.has_docstring());
        EXPECT_EQ(metadata.get_docstring_or_empty(), "State machine states");
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, MixedAutoAndExplicitValues) {
    // ПРАВИЛЬНО: смешанный формат
    std::string code = "(defenum mixed (first) (second 10) (third))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* mixed_enum = parse_defenum(obj, ts);
    ASSERT_NE(mixed_enum, nullptr);
    EXPECT_EQ(mixed_enum->get_name(), "mixed");
}


TEST_F(DefEnumTest, ComplexTypeSpec) {
    // ПРАВИЛЬНЫЙ ФОРМАТ: все entries в скобках
    std::string code = "(defenum complex :type int32 :bitfield #f (flag1 1) (flag2 2))";
    auto obj = reader.read_from_string(code, "test");

    EnumType* complex_enum = parse_defenum(obj, ts);
    ASSERT_NE(complex_enum, nullptr);
}



TEST_F(DefEnumTest, InvalidBitfieldValue) {
    std::string code = "(defenum flags :bitfield invalid read 1)";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, InvalidEnumValue) {
    // Используем пару для явного значения
    std::string code = "(defenum test (value \"not-a-number\"))";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, DuplicateEntries) {
    std::string code = "(defenum test value1 value2 value1)"; // дубликат
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, EmptyEnum) {
    std::string code = "(defenum empty)";
    auto obj = reader.read_from_string(code, "test");

    try {
        EnumType* empty_enum = parse_defenum(obj, ts);
        ASSERT_NE(empty_enum, nullptr);
        EXPECT_EQ(empty_enum->get_name(), "empty");
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, EnumWithFalseBitfield) {
    std::string code = "(defenum normal :bitfield #f (option1) (option2))";
    auto obj = reader.read_from_string(code, "test");

    try {
        EnumType* normal_enum = parse_defenum(obj, ts);
        ASSERT_NE(normal_enum, nullptr);
        EXPECT_EQ(normal_enum->get_name(), "normal");
        EXPECT_FALSE(normal_enum->is_bitfield());
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, LargeEnum) {
    std::string code = R"(
        (defenum big-enum
          (value0) (value1) (value2) (value3) (value4)
          (value5) (value6) (value7) (value8) (value9))
    )";
    auto obj = reader.read_from_string(code, "test");

    try {
        EnumType* big_enum = parse_defenum(obj, ts);
        ASSERT_NE(big_enum, nullptr);
        EXPECT_EQ(big_enum->get_name(), "big-enum");
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, EnumWithNegativeValues) {
    std::string code = "(defenum signed (negative -1) (zero 0) (positive 1))";
    auto obj = reader.read_from_string(code, "test");

    try {
        EnumType* signed_enum = parse_defenum(obj, ts);
        ASSERT_NE(signed_enum, nullptr);
        EXPECT_EQ(signed_enum->get_name(), "signed");
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}

TEST_F(DefEnumTest, InvalidBaseType) {
    std::string code = "(defenum test :type nonexistent-type value1)";
    auto obj = reader.read_from_string(code, "test");

    EXPECT_THROW(parse_defenum(obj, ts), std::runtime_error);
}

TEST_F(DefEnumTest, EnumWithCopyEntries) {
    // ПРАВИЛЬНЫЙ ФОРМАТ: все entries в скобках
    std::string code1 = "(defenum base (a 1) (b 2) (c 3))";
    auto obj1 = reader.read_from_string(code1, "test");
    parse_defenum(obj1, ts);

    std::string code2 = "(defenum extended :copy-entries base (d 4) (e 5))";
    auto obj2 = reader.read_from_string(code2, "test");

    EnumType* extended_enum = parse_defenum(obj2, ts);
    ASSERT_NE(extended_enum, nullptr);
}

TEST_F(DefEnumTest, SimpleSymbolEntries) {
    std::string code = "(defenum simple (entry1) (entry2) (entry3))";
    auto obj = reader.read_from_string(code, "test");

    try {
        EnumType* simple_enum = parse_defenum(obj, ts);
        ASSERT_NE(simple_enum, nullptr);
        EXPECT_EQ(simple_enum->get_name(), "simple");

        // Проверяем что entries создались
        auto& entries = simple_enum->entries();
        EXPECT_GT(entries.size(), 0);
    }
    catch (const std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
}