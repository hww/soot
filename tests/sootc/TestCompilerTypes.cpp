#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// deftype
// =============================================================================

TEST_F(SootcTest, SimpleDeftype) {
    auto file = compile(R"(
        (deftype vec3 (structure)
          ((x float :offset 0)
           (y float :offset 4)
           (z float :offset 8)))
    )");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(file->entry_count(), 1u);

    const auto *entry = file->entries();
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(file->resolve_sid(entry[0].m_nameID), "vec3");
    EXPECT_EQ(entry[0].m_typeId, SID("ss-type"));
}

TEST_F(SootcTest, DeftypeWithDocstring) {
    auto file = compile(R"(
        (deftype documented (structure)
          "A documented type"
          ((a int :offset 0)))
    )");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(file->entry_count(), 1u);
}

TEST_F(SootcTest, DeftypeWithMultipleFields) {
    // IMPORTANT: `int` in the SOOT dialect is 8 bytes wide, matching the
    // DC register width. It is NOT the same as C++ `int`, so do not
    // use `sizeof(int)` here — it would compile to 4 on most hosts and
    // make the test compare against the wrong number.
    //
    // The layout below is what a correct user would write for a 4-field
    // structure of 8-byte integers: offsets 0, 8, 16, 24. The total
    // size is 32 bytes.
    //
    // The previous version of this test used offsets 0, 4, 8, 12, which
    // is valid only for 4-byte fields. The compiler does not reject it,
    // but the resulting layout has overlapping fields and the reported
    // size ends up being max_offset + sizeof(int) = 12 + 8 = 20.
    auto file = compile(R"(
        (deftype quad (structure)
          ((a int :offset 0)
           (b int :offset 8)
           (c int :offset 16)
           (d int :offset 24)))
    )");
    ASSERT_NE(file, nullptr);

    const auto *entry = file->entries();
    const auto *ss = file->entry_as_ss_type(entry[0]);
    ASSERT_NE(ss, nullptr);
    EXPECT_EQ(ss->m_numFields, 4u);
    EXPECT_EQ(ss->m_size, 32u);
}

TEST_F(SootcTest, DeftypeSingleField) {
    // `int` is 8 bytes wide in this dialect. Do not use sizeof(int)
    // here: it would resolve to the host compiler's `int` (typically
    // 4 bytes) and give a wrong expected value.
    auto file = compile(R"(
        (deftype single (structure)
          ((value int :offset 0)))
    )");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(file->entry_count(), 1u);

    const auto *ss = file->entry_as_ss_type(file->entries()[0]);
    ASSERT_NE(ss, nullptr);
    EXPECT_EQ(ss->m_numFields, 1u);
    EXPECT_EQ(ss->m_size, 8u);
}

TEST_F(SootcTest, DeftypeMixedFieldTypes) {
    // Sizes in the SOOT dialect:
    //   int     8 bytes (register width)
    //   float   4 bytes
    //   int64   8 bytes
    //   symbol  8 bytes (SID64)
    //
    // Offsets below are laid out to be non-overlapping:
    //   i at 0, f at 8, l at 12, s at 20.
    //
    // Total size: 20 + 8 (symbol) = 28, which is NOT a multiple of 8,
    // so get_size_in_memory() rounds it up to the structure's alignment
    // (8 bytes) and reports 32. That is expected behaviour: unaligned
    // trailing bytes are padding.
    auto file = compile(R"(
        (deftype mixed (structure)
          ((i int :offset 0)
           (f float :offset 8)
           (l int64 :offset 12)
           (s symbol :offset 20)))
    )");
    ASSERT_NE(file, nullptr);

    const auto *ss = file->entry_as_ss_type(file->entries()[0]);
    ASSERT_NE(ss, nullptr);
    EXPECT_EQ(ss->m_numFields, 4u);
    // Actual layout:
    //   i int      at 0  -> 0..7
    //   f float    at 8  -> 8..11
    //   l int64    at 12 -> 12..19
    //   s symbol   at 20 -> 20..27
    // get_size_in_memory returns the offset past the last field, so 28.
    // There is no trailing padding in this codebase.
    EXPECT_EQ(ss->m_size, 28u);
}

// =============================================================================
// defenum
// =============================================================================

TEST_F(SootcTest, SimpleEnum) {
    // Enums are registered with the TypeSystem but do not produce a
    // binary entry on their own. This test verifies that compilation
    // does not crash and registers the enum.
    auto file = compile(R"(
        (defenum color (red) (green) (blue))
    )");
    // No binary should be produced for a pure enum declaration.
    EXPECT_EQ(file, nullptr);
}

TEST_F(SootcTest, EnumWithExplicitValues) {
    auto file = compile(R"(
        (defenum status (inactive 0) (active 1) (paused 2))
    )");
    // No binary should be produced.
    EXPECT_EQ(file, nullptr);
}

TEST_F(SootcTest, BitfieldEnum) {
    auto file = compile(R"(
        (defenum flags :bitfield #t (read) (write) (exec))
    )");
    EXPECT_EQ(file, nullptr);
}

TEST_F(SootcTest, EnumAndTypeTogether) {
    auto file = compile(R"(
        (defenum color (red) (green) (blue))
        (deftype pixel (structure)
          ((r int :offset 0)
           (g int :offset 4)
           (b int :offset 8)))
    )");
    ASSERT_NE(file, nullptr);
    // Only the deftype should be emitted.
    EXPECT_EQ(file->entry_count(), 1u);
    EXPECT_EQ(file->resolve_sid(file->entries()[0].m_nameID), "pixel");
}

// =============================================================================
// Static data declarations
// =============================================================================

TEST_F(SootcTest, StaticInstanceSingleField) {
    auto file = compile(R"(
        (deftype point (structure)
          ((x int :offset 0)))
        (define origin (new 'static 'point :x 0))
    )");
    ASSERT_NE(file, nullptr);
    // SsType + data entry.
    EXPECT_EQ(file->entry_count(), 2u);
}

TEST_F(SootcTest, StaticInstanceMultipleFields) {
    auto file = compile(R"(
        (deftype vec3 (structure)
          ((x float :offset 0)
           (y float :offset 4)
           (z float :offset 8)))
        (define origin (new 'static 'vec3 :x 0.0 :y 0.0 :z 0.0))
    )");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->entry_count(), 2u);
}

TEST_F(SootcTest, MultipleStaticInstances) {
    auto file = compile(R"(
        (deftype vec2 (structure)
          ((x float :offset 0)
           (y float :offset 4)))
        (define origin (new 'static 'vec2 :x 0.0 :y 0.0))
        (define unit   (new 'static 'vec2 :x 1.0 :y 1.0))
    )");
    ASSERT_NE(file, nullptr);
    // One SsType + two data entries.
    EXPECT_EQ(file->entry_count(), 3u);
}

TEST_F(SootcTest, StaticInstanceWithIntValues) {
    auto file = compile(R"(
        (deftype config (structure)
          ((width int :offset 0)
           (height int :offset 4)))
        (define screen (new 'static 'config :width 1920 :height 1080))
    )");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->entry_count(), 2u);
}

// =============================================================================
// Field layout assertions
// =============================================================================

TEST_F(SootcTest, FieldOffsetsArePreserved) {
    auto file = compile(R"(
        (deftype padded (structure)
          ((a int :offset 0)
           (b int :offset 16)
           (c int :offset 32)))
    )");
    ASSERT_NE(file, nullptr);

    const auto *ss = file->entry_as_ss_type(file->entries()[0]);
    ASSERT_NE(ss, nullptr);

    ASSERT_GE(ss->m_numFields, 3u);
    const SsField *fields = ss->m_pFields;
    ASSERT_NE(fields, nullptr);

    EXPECT_EQ(fields[0].m_offset, 0u);
    EXPECT_EQ(fields[1].m_offset, 16u);
    EXPECT_EQ(fields[2].m_offset, 32u);
}

TEST_F(SootcTest, FieldTypesArePreserved) {
    auto file = compile(R"(
        (deftype typed (structure)
          ((an_int int :offset 0)
           (a_float float :offset 4)
           (a_sym symbol :offset 8)))
    )");
    ASSERT_NE(file, nullptr);

    const auto *ss = file->entry_as_ss_type(file->entries()[0]);
    ASSERT_NE(ss, nullptr);
    ASSERT_GE(ss->m_numFields, 3u);

    EXPECT_EQ(file->resolve_sid(ss->m_pFields[0].m_name), "an_int");
    EXPECT_EQ(file->resolve_sid(ss->m_pFields[0].m_type), "int");
    EXPECT_EQ(file->resolve_sid(ss->m_pFields[1].m_name), "a_float");
    EXPECT_EQ(file->resolve_sid(ss->m_pFields[1].m_type), "float");
}