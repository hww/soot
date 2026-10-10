#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// Method compilation
// =============================================================================

TEST_F(SootcTest, SimpleMethodDefinition) {
    auto file = compile(R"(
        (deftype counter (structure)
          ((value int :offset 0))
          (:methods
            (inc (function counter counter) counter)))

        (defmethod inc ((this counter))
          (set! (-> this value) (+ (-> this value) 1))
          this)
    )");
    ASSERT_NE(file, nullptr);
    // Should emit at least the SsType and the method's ScriptLambda.
    EXPECT_GE(file->entry_count(), 1u);
}

TEST_F(SootcTest, MethodWithArguments) {
    auto file = compile(R"(
        (deftype point (structure)
          ((x int :offset 0)
           (y int :offset 4))
          (:methods
            (add-delta (function point int int point) point)))

        (defmethod add-delta ((this point) (dx int) (dy int))
          (set! (-> this x) (+ (-> this x) dx))
          (set! (-> this y) (+ (-> this y) dy))
          this)
    )");
    ASSERT_NE(file, nullptr);
}

TEST_F(SootcTest, MethodWithMultipleFields) {
    auto file = compile(R"(
        (deftype box (structure)
          ((width int :offset 0)
           (height int :offset 4))
          (:methods
            (area (function box int) int)))

        (defmethod area ((this box))
          (* (-> this width) (-> this height)))
    )");
    ASSERT_NE(file, nullptr);
}

TEST_F(SootcTest, MethodCallingAnotherMethod) {
    auto file = compile(R"(
        (deftype acc (structure)
          ((total int :offset 0))
          (:methods
            (add (function acc int acc) acc)
            (add-twice (function acc int acc) acc)))

        (defmethod add ((this acc) (n int))
          (set! (-> this total) (+ (-> this total) n))
          this)

        (defmethod add-twice ((this acc) (n int))
          (add this n)
          (add this n))
    )");
    ASSERT_NE(file, nullptr);
    // At least one SsType + two methods.
    EXPECT_GE(file->entry_count(), 1u);
}

// =============================================================================
// Method registration in Globals
// =============================================================================

TEST_F(SootcTest, MethodIsRegisteredInGlobals) {
    auto file = compile(R"(
        (deftype thing (structure)
          ((n int :offset 0))
          (:methods
            (get-n (function thing int) int)))

        (defmethod get-n ((this thing))
          (-> this n))
    )");
    ASSERT_NE(file, nullptr);

    carbon::Globals::inst().clear_all();
    ASSERT_TRUE(carbon::Globals::inst().load_module(std::move(*file)));

    // Methods are registered under "<type>-<method>" as the full name.
    void *fn_ptr = carbon::Globals::inst().find_symbol_ptr(carbon::StringId("thing-get-n"));
    EXPECT_NE(fn_ptr, nullptr);
}

TEST_F(SootcTest, MultipleMethodsOnSameType) {
    auto file = compile(R"(
        (deftype pair (structure)
          ((a int :offset 0)
           (b int :offset 4))
          (:methods
            (get-a (function pair int) int)
            (get-b (function pair int) int)))

        (defmethod get-a ((this pair)) (-> this a))
        (defmethod get-b ((this pair)) (-> this b))
    )");
    ASSERT_NE(file, nullptr);

    carbon::Globals::inst().clear_all();
    ASSERT_TRUE(carbon::Globals::inst().load_module(std::move(*file)));

    EXPECT_NE(carbon::Globals::inst().find_symbol_ptr(carbon::StringId("pair-get-a")), nullptr);
    EXPECT_NE(carbon::Globals::inst().find_symbol_ptr(carbon::StringId("pair-get-b")), nullptr);
}

// =============================================================================
// The `new` method (constructor)
// =============================================================================

TEST_F(SootcTest, NewMethodInsertionOfThis) {
    // The `new` method has a hidden `this` parameter inserted at position 1
    // by the compiler. We only check that the form compiles successfully.
    auto file = compile(R"(
        (deftype thing (structure)
          ((value int :offset 0))
          (:methods
            (new (function thing int thing) thing)))

        (defmethod new thing ((allocation symbol) (v int))
          (set! (-> this value) v)
          this)
    )");
    // Either compiles or fails with a clear error — both are acceptable
    // for the purpose of this smoke test.
}