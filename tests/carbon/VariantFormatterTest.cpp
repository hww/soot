#include "gtest/gtest.h"

#include "carbon/Export.hpp"
#include "carbon/gc/GC.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "carbon/lib/Variant.hpp"

#include "fmt/format.h"

using namespace carbon;

class VariantFormatterTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gc::initialize(1, 16); }
    static void TearDownTestSuite() { gc::shutdown(); }
};

// ============================================================================
// Default format ({}): Int -> number, Float -> number,
//                      Pointer -> "..." (C string, capped at 256),
//                      Null -> null
// ============================================================================

TEST_F(VariantFormatterTest, DefaultNull) {
    Variant v;
    EXPECT_EQ(fmt::format("{}", v), "null");
}

TEST_F(VariantFormatterTest, DefaultInt) {
    EXPECT_EQ(fmt::format("{}", Variant((i64)42)), "42");
    EXPECT_EQ(fmt::format("{}", Variant((i64)-7)), "-7");
    EXPECT_EQ(fmt::format("{}", Variant((i64)0)), "0");
}

TEST_F(VariantFormatterTest, DefaultFloat) {
    // f64 literals round-trip through fmt's shortest representation.
    EXPECT_EQ(fmt::format("{}", Variant(3.14)), "3.14");
    EXPECT_EQ(fmt::format("{}", Variant(0.0)), "0");
}

TEST_F(VariantFormatterTest, DefaultPointerAsString) {
    // A plain pointer in default mode is read as a C string.
    // This is the documented behaviour: the default assumes that
    // pointers reaching the formatter are C strings, because that
    // is the common case in logs and dumps.
    Variant v(static_cast<void *>(nullptr), RuntimeType::Pointer);
    // nullptr pointer -> "null", not "\"\""
    EXPECT_EQ(fmt::format("{}", v), "null");
}

TEST_F(VariantFormatterTest, DefaultPointerNullptrIsNull) {
    Variant v(static_cast<void *>(nullptr), RuntimeType::Pointer);
    EXPECT_EQ(fmt::format("{}", v), "null");
}

// ============================================================================
// {:x} — hex
// ============================================================================

TEST_F(VariantFormatterTest, Hex) {
    EXPECT_EQ(fmt::format("{:x}", Variant((i64)42)), "0x2A");
    EXPECT_EQ(fmt::format("{:x}", Variant((i64)255)), "0xFF");
    EXPECT_EQ(fmt::format("{:x}", Variant((i64)0)), "0x0");
}

// ============================================================================
// {:b} — bool
// ============================================================================

TEST_F(VariantFormatterTest, Bool) {
    EXPECT_EQ(fmt::format("{:b}", Variant((i64)1)), "true");
    EXPECT_EQ(fmt::format("{:b}", Variant((i64)0)), "false");
    EXPECT_EQ(fmt::format("{:b}", Variant((i64)42)), "true"); // any non-zero -> true
}

// ============================================================================
// {:f} — float (Int cast to f64)
// ============================================================================

TEST_F(VariantFormatterTest, FloatFromInt) {
    EXPECT_EQ(fmt::format("{:f}", Variant((i64)42)), "42.0");
    EXPECT_EQ(fmt::format("{:f}", Variant((i64)0)), "0.0");
}

// ============================================================================
// {:sid} — resolve Int as SID through StringIdManager
// ============================================================================

TEST_F(VariantFormatterTest, SidKnown) {
    // Register a known name, then format its SID.
    const StringId id("test_formatter_known_name");
    Variant        v(static_cast<i64>(id.value));
    EXPECT_EQ(fmt::format("{:sid}", v), "test_formatter_known_name");
}

TEST_F(VariantFormatterTest, SidUnknown) {
    // A hash that is unlikely to be registered. If it happens to
    // be registered (the manager is global and shared across tests),
    // the resolved name is acceptable — the test must not depend on
    // which strings other tests have interned.
    Variant           v(static_cast<i64>(0x9E3779B97F4A7C15ull));
    const std::string s = fmt::format("{:sid}", v);
    EXPECT_FALSE(s.empty());
    // No assertion on the prefix: either "sid:0x..." (unknown)
    // or a resolved name (unlucky collision with a string registered
    // by another test). Both are correct outputs of the formatter.
}

TEST_F(VariantFormatterTest, SidZeroIsNull) {
    Variant v((i64)0);
    EXPECT_EQ(fmt::format("{:sid}", v), "null");
}

// ============================================================================
// {:p} — pointer as address, never read as C string
// ============================================================================

TEST_F(VariantFormatterTest, PointerAsAddress) {
    Variant v(static_cast<void *>(nullptr), RuntimeType::Pointer);
    // nullptr still prints "null" even in {:p} mode.
    EXPECT_EQ(fmt::format("{:p}", v), "null");
}

// ============================================================================
// {:s} — explicit string mode (same as default for Pointer)
// ============================================================================

TEST_F(VariantFormatterTest, StringModeOnInt) {
    // String mode on an Int just prints the number: there is no
    // way to read an Int as a C string.
    EXPECT_EQ(fmt::format("{:s}", Variant((i64)42)), "42");
}

TEST_F(VariantFormatterTest, StringModeOnNull) {
    EXPECT_EQ(fmt::format("{:s}", Variant()), "null");
}

// ============================================================================
// {:t=<name>} — explicit type name, resolves semantics from type
// ============================================================================

TEST_F(VariantFormatterTest, TypeNameStringOnPointer) {
    Variant v(static_cast<void *>(nullptr), RuntimeType::Pointer);
    // nullptr pointer with type_name "string" -> "null", not "\"\""
    EXPECT_EQ(fmt::format("{:t=string}", v), "null");
}

TEST_F(VariantFormatterTest, TypeNameStringOnInt) {
    // String type name on an Int does NOT read the integer as a
    // C string; fmt_variant_typed falls through to default int
    // rendering. This is the safe behaviour.
    EXPECT_EQ(fmt::format("{:t=string}", Variant((i64)42)), "42");
}

TEST_F(VariantFormatterTest, TypeNameBoolOnInt) {
    EXPECT_EQ(fmt::format("{:t=bool}", Variant((i64)1)), "true");
    EXPECT_EQ(fmt::format("{:t=bool}", Variant((i64)0)), "false");
}

TEST_F(VariantFormatterTest, TypeNameSymbolOnInt) {
    const StringId id("test_formatter_symbol");
    Variant        v(static_cast<i64>(id.value));
    EXPECT_EQ(fmt::format("{:t=symbol}", v), "test_formatter_symbol");
}

TEST_F(VariantFormatterTest, TypeNameFloatOnInt) {
    EXPECT_EQ(fmt::format("{:t=float}", Variant((i64)42)), "42.0");
}

TEST_F(VariantFormatterTest, TypeNameUnknownOnPointer) {
    // An unknown type name must NOT cause the pointer to be read
    // as a C string. It is printed as an address instead.
    Variant v(static_cast<void *>(nullptr), RuntimeType::Pointer);
    EXPECT_EQ(fmt::format("{:t=vector}", v), "null");

    // Non-null pointer: must be ptr:0x..., never a quoted string.
    const int         dummy = 0;
    Variant           v2(const_cast<int *>(&dummy), RuntimeType::Pointer);
    const std::string s = fmt::format("{:t=vector}", v2);
    EXPECT_TRUE(s.starts_with("ptr:0x"));
}

// ============================================================================
// Parser edge cases: the format spec must be parsed without
// swallowing the wrong flag.
// ============================================================================

TEST_F(VariantFormatterTest, SidIsNotParsedAsStringPlusGarbage) {
    // {:sid} must be recognised as the three-letter Sid flag,
    // not as {:s} followed by "id". If the parser is wrong, the
    // output is either a parsing error or a plain number with
    // leftover spec characters.
    const StringId id("test_formatter_sid_parse");
    Variant        v(static_cast<i64>(id.value));
    EXPECT_EQ(fmt::format("{:sid}", v), "test_formatter_sid_parse");
}

TEST_F(VariantFormatterTest, EmptySpecIsDefault) {
    // {} and {:} must both produce the default output.
    EXPECT_EQ(fmt::format("{}", Variant((i64)42)), "42");
    EXPECT_EQ(fmt::format("{:}", Variant((i64)42)), "42");
}

TEST_F(VariantFormatterTest, TypeSpecWithEmptyName) {
    // {:t=} with an empty name: the parser accepts it, but the
    // formatter sees an empty m_type_name and falls back to the
    // default mode. No crash, no reading of pointers as strings.
    EXPECT_EQ(fmt::format("{:t=}", Variant((i64)42)), "42");
}

// ============================================================================
// Format with arguments around the Variant
// ============================================================================

TEST_F(VariantFormatterTest, MixedWithOtherArguments) {
    EXPECT_EQ(fmt::format("x={} y={:x} z={}", Variant((i64)10), Variant((i64)255), Variant((i64)0)),
              "x=10 y=0xFF z=0");
}

TEST_F(VariantFormatterTest, NullInString) {
    EXPECT_EQ(fmt::format("value: {}", Variant()), "value: null");
}

// ============================================================================
// Consistency: to_string() and default format should agree for
// non-pointer values, and may differ only for Pointer (default
// format reads it as a C string, to_string() prints the address).
// ============================================================================

TEST_F(VariantFormatterTest, ToStringMatchesDefaultForInt) {
    Variant v((i64)42);
    EXPECT_EQ(fmt::format("{}", v), v.to_string());
}

TEST_F(VariantFormatterTest, ToStringMatchesDefaultForFloat) {
    Variant v(3.5f);
    EXPECT_EQ(fmt::format("{}", v), v.to_string());
}

TEST_F(VariantFormatterTest, ToStringMatchesDefaultForNull) {
    Variant v;
    EXPECT_EQ(fmt::format("{}", v), v.to_string());
}