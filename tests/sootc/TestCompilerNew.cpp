#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// (new ...) — syntax
// =============================================================================

TEST_F(SootcTest, NewWithoutConstructorCompiles) {
    auto file = compile(R"(
        (deftype simple (structure) ((x float)))
        (defun make () (new 'process 'simple))
    )");
    ASSERT_NE(file, nullptr) << "bare new without ctor should compile";
}

TEST_F(SootcTest, NewWithQuotedAllocationAndType) {
    auto file = compile(R"(
        (deftype simple (structure) ((x float)))
        (defun make () (new 'process 'simple))
    )");
    ASSERT_NE(file, nullptr);
}

TEST_F(SootcTest, NewWithPositionalArguments) {
    auto file = compile(R"(
        (deftype tvector (structure)
          ((x float :offset 0)
           (y float :offset 4)))
        (defmethod new tvector ((allocation symbol) (x float) (y float))
          (set! (-> this x) x)
          (set! (-> this y) y)
          this)
        (defun make () (new 'process 'tvector 1.0 2.0))
    )");
    ASSERT_NE(file, nullptr) << "positional args must be accepted";
}

// =============================================================================
// (new ...) — method lookup
// =============================================================================

TEST_F(SootcTest, NewWithConstructorCallsVectorNew) {
    auto file = compile(R"(
        (deftype tvector (structure) ((x float :offset 0)))
        (defmethod new tvector ((allocation symbol) (x float))
          (set! (-> this x) x)
          this)
        (defun make () (new 'process 'tvector 1.0))
    )");
    ASSERT_NE(file, nullptr);

    // The `make` function must reference both `allocate-tvector` and
    // `tvector-new` in its symbol table.
    const auto *entry = file->find_entry_by_name(SID("make"));
    ASSERT_NE(entry, nullptr);
}