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
    auto file = compile(R"((defun test () (print "abc")))");
    ASSERT_NE(file, nullptr);

    const auto *entry = file->find_entry_by_name(SID("test"));
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->m_nameID, SID("test"));
    EXPECT_EQ(entry->m_typeId, SID("script-lambda"));
}

TEST_F(SootcTest, DefunWithPrintRuns) {
    auto result = run(R"((defun test () (print "abc")))", "test");
    ASSERT_TRUE(result.ok) << result.error;
}

// =============================================================================
// Type declarations
// =============================================================================

TEST_F(SootcTest, DeftypeRegistersType) {
    auto file = compile("(deftype vec3 (structure) ((x float)))");
    ASSERT_NE(file, nullptr) << "deftype should produce an SsType entry";

    const auto *entry = file->find_entry_by_name(SID("vec3"));
    EXPECT_EQ(file->resolve_sid(entry[0].m_nameID), "vec3");
    EXPECT_EQ(entry->m_typeId, SID("ss-type"));
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

    const auto *entry = file->find_entry_by_name(SID("tvector"));
    ASSERT_NE(entry, nullptr) << "tvector SsType entry not found";

    const auto *ss = file->entry_as_ss_type(*entry);
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

    // SsType for vec3
    const auto *type_entry = file->find_entry_by_name(SID("vec3"));
    ASSERT_NE(type_entry, nullptr) << "vec3 SsType missing";
    EXPECT_EQ(type_entry->m_typeId, SID("ss-type"));

    // Data entry for v
    const auto *data_entry = file->find_entry_by_name(SID("v"));
    ASSERT_NE(data_entry, nullptr) << "v data entry missing";
    // (проверить, что data_entry — это data-struct, а не что-то ещё)
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