#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// Full programs
// =============================================================================

TEST_F(SootcTest, FibonacciIterative) {
    auto result = run(R"(
        (defun fib (n)
          (let ((a 0) (b 1) (i 0))
            (while (< i n)
              (let ((tmp (+ a b)))
                (set! a b)
                (set! b tmp))
              (set! i (+ i 1)))
            a))
        (defun get () (fib 10))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 55);
}

TEST_F(SootcTest, SumOfSquares) {
    auto result = run(R"(
        (defun square (x) (* x x))
        (defun sum-of-squares (n)
          (let ((sum 0) (i 1))
            (while (<= i n)
              (set! sum (+ sum (square i)))
              (set! i (+ i 1)))
            sum))
        (defun get () (sum-of-squares 5))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    // 1 + 4 + 9 + 16 + 25 = 55
    EXPECT_EQ(result.value.to_int(), 55);
}

TEST_F(SootcTest, FactorialIterative) {
    auto result = run(R"(
        (defun fact (n)
          (let ((result 1) (i 1))
            (while (<= i n)
              (set! result (* result i))
              (set! i (+ i 1)))
            result))
        (defun get () (fact 5))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 120);
}

TEST_F(SootcTest, GreatestCommonDivisor) {
    auto result = run(R"(
        (defun gcd (a b)
          (while (!= b 0)
            (let ((tmp b))
              (set! b (% a b))
              (set! a tmp)))
          a)
        (defun get () (gcd 48 18))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

TEST_F(SootcTest, PowerFunction) {
    auto result = run(R"(
        (defun power (base exp)
          (let ((result 1) (i 0))
            (while (< i exp)
              (set! result (* result base))
              (set! i (+ i 1)))
            result))
        (defun get () (power 2 10))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1024);
}

TEST_F(SootcTest, CountDown) {
    auto result = run(R"(
        (defun count-down (n)
          (let ((count 0))
            (while (> n 0)
              (set! count (+ count 1))
              (set! n (- n 1)))
            count))
        (defun get () (count-down 7))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 7);
}

// =============================================================================
// Mixed type programs
// =============================================================================

TEST_F(SootcTest, IntegerAndFloatMix) {
    auto result = run(R"(
        (defun f ()
          (let ((i 3) (fl 2.5))
            (+ i fl)))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    // The first operand's type determines the result type.
    // i is int, so the result should be int (2.5 truncated to 2).
    EXPECT_EQ(result.value.to_int(), 5);
}

TEST_F(SootcTest, AllComparisonOperators) {
    auto result = run(R"(
        (defun f ()
          (let ((score 0))
            (if (== 1 1) (set! score (+ score 1)))
            (if (!= 1 2) (set! score (+ score 1)))
            (if (< 1 2)  (set! score (+ score 1)))
            (if (<= 1 1) (set! score (+ score 1)))
            (if (> 2 1)  (set! score (+ score 1)))
            (if (>= 1 1) (set! score (+ score 1)))
            score))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

// =============================================================================
// Multiple functions
// =============================================================================

TEST_F(SootcTest, ManySmallFunctions) {
    auto result = run(R"(
        (defun one () 1)
        (defun two () 2)
        (defun three () 3)
        (defun six () (+ (one) (+ (two) (three))))
        (defun get () (six))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

TEST_F(SootcTest, RecursiveLikeChain) {
    auto result = run(R"(
        (defun a (n) (+ n 1))
        (defun b (n) (a (a n)))
        (defun c (n) (b (b n)))
        (defun get () (c 0))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    // c(0) = b(b(0)) = b(a(a(0))) = b(a(1)) = b(2) = a(a(2)) = a(3) = 4
    EXPECT_EQ(result.value.to_int(), 4);
}

// =============================================================================
// Static data + type integration
// =============================================================================

TEST_F(SootcTest, TypeAndFunctionTogether) {
    auto file = compile(R"(
        (deftype vec3 (structure)
          ((x float :offset 0)
           (y float :offset 4)
           (z float :offset 8)))
        (defun get () 42)
    )");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->entry_count(), 2u); // one ScriptLambda + one SsType
}

TEST_F(SootcTest, EnumIsRegisteredButNoCode) {
    auto file = compile(R"(
        (defenum color (red) (green) (blue))
    )");
    // An enum has no runtime entry by itself in this codebase.
    // It is registered in TypeSystem, not emitted as a binary entry.
    // So compile may return nullptr (no binary) or a file with zero entries.
    // We only assert that it does not crash.
}

// =============================================================================
// REPL-like session
// =============================================================================

TEST_F(SootcTest, ConstantThenFunction) {
    auto result = run(R"(
        (defconstant magic 42)
        (defun get () magic)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, MultipleConstants) {
    auto result = run(R"(
        (defconstant a 10)
        (defconstant b 20)
        (defun get () (+ a b))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 30);
}