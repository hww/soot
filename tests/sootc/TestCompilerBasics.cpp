#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// Empty / trivial inputs
// =============================================================================

TEST_F(SootcTest, EmptyInputProducesNoBinary) {
    std::string error;
    auto        file = compile("", &error);
    // Empty input is not an error; it simply produces no binary.
    EXPECT_EQ(file, nullptr);
}

TEST_F(SootcTest, DefunCompilesToScriptLambda) {
    auto file = compile(R"((defun test () (_print "abc")))");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(file->entry_count(), 1u);

    const auto *entry = file->entries();
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry[0].m_nameID, SID("test"));
    EXPECT_EQ(entry[0].m_typeId, SID("script-lambda"));
}

TEST_F(SootcTest, DefunWithPrintRuns) {
    auto result = run(R"((defun test () (_print "abc")))", "test");
    ASSERT_TRUE(result.ok) << result.error;
}

// =============================================================================
// Type declarations
// =============================================================================

TEST_F(SootcTest, DeftypeRegistersType) {
    auto file = compile("(deftype vec3 (structure) ((x float)))");
    ASSERT_NE(file, nullptr) << "deftype should produce an SsType entry";

    ASSERT_EQ(file->entry_count(), 1u);
    const auto *entry = file->entries();
    EXPECT_EQ(file->resolve_sid(entry[0].m_nameID), "vec3");
    EXPECT_EQ(entry[0].m_typeId, SID("ss-type"));
}

TEST_F(SootcTest, DeftypeVectorHasFourFields) {
    auto file = compile(R"(
        (deftype tvector (structure)
          ((x float :offset 0)
           (y float :offset 4)
           (z float :offset 8)
           (w float :offset 12)))
    )");
    ASSERT_NE(file, nullptr);

    const auto *entry = file->entries();
    const auto *ss = file->entry_as_ss_type(entry[0]);
    ASSERT_NE(ss, nullptr);
    EXPECT_EQ(ss->m_numFields, 4u);
    EXPECT_EQ(ss->m_size, 16u);
}

// =============================================================================
// Static instance
// =============================================================================

TEST_F(SootcTest, StaticInstanceHasCorrectLayout) {
    auto file = compile(R"(
        (deftype vec3 (structure) ((x float :offset 0)))
        (define v (new 'static 'vec3 :x 1.0))
    )");
    ASSERT_NE(file, nullptr);

    // Should have two entries: the SsType and the data instance.
    EXPECT_EQ(file->entry_count(), 2u);
}

// =============================================================================
// Constants
// =============================================================================

TEST_F(SootcTest, DefconstantIsInlined) {
    auto result = run(R"(
        (defconstant magic 42)
        (defun get () magic)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, DefineConstantIsInlined) {
    auto result = run(R"(
        (define magic 42)
        (defun get () magic)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}