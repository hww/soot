#include "TestHelper.hpp"

using namespace sootc::testing;

// =============================================================================
// Constants
// =============================================================================

TEST_F(SootcTest, IntegerConstant) {
    auto result = run(R"(
        (defun get () 42)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, NegativeIntegerConstant) {
    auto result = run(R"(
        (defun get () -123)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), -123);
}

TEST_F(SootcTest, FloatConstant) {
    auto result = run(R"(
        (defun get () 3.14)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FLOAT_EQ(result.value.to_float(), 3.14f);
}

TEST_F(SootcTest, BooleanTrueConstant) {
    auto result = run(R"(
        (defun get () #t)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, BooleanFalseConstant) {
    auto result = run(R"(
        (defun get () #f)
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 0);
}

// =============================================================================
// Arithmetic
// =============================================================================

TEST_F(SootcTest, IntegerAddition) {
    auto result = run(R"(
        (defun add () (+ 2 3))
    )",
                      "add");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 5);
}

TEST_F(SootcTest, IntegerSubtraction) {
    auto result = run(R"(
        (defun sub () (- 10 4))
    )",
                      "sub");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

TEST_F(SootcTest, IntegerMultiplication) {
    auto result = run(R"(
        (defun mul () (* 6 7))
    )",
                      "mul");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, IntegerDivision) {
    auto result = run(R"(
        (defun div () (/ 20 4))
    )",
                      "div");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 5);
}

TEST_F(SootcTest, IntegerModulo) {
    auto result = run(R"(
        (defun mod () (% 17 5))
    )",
                      "mod");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 2);
}

TEST_F(SootcTest, UnaryMinus) {
    auto result = run(R"(
        (defun neg () (- 42))
    )",
                      "neg");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), -42);
}

TEST_F(SootcTest, NestedArithmetic) {
    auto result = run(R"(
        (defun calc () (+ (* 2 3) (- 10 4)))
    )",
                      "calc");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 12);
}

TEST_F(SootcTest, FloatAddition) {
    auto result = run(R"(
        (defun add () (+ 1.5 2.5))
    )",
                      "add");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FLOAT_EQ(result.value.to_float(), 4.0f);
}

TEST_F(SootcTest, FloatSubtraction) {
    auto result = run(R"(
        (defun sub () (- 5.5 2.5))
    )",
                      "sub");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FLOAT_EQ(result.value.to_float(), 3.0f);
}

TEST_F(SootcTest, FloatMultiplication) {
    auto result = run(R"(
        (defun mul () (* 1.5 2.0))
    )",
                      "mul");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FLOAT_EQ(result.value.to_float(), 3.0f);
}

TEST_F(SootcTest, FloatDivision) {
    auto result = run(R"(
        (defun div () (/ 7.5 2.5))
    )",
                      "div");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FLOAT_EQ(result.value.to_float(), 3.0f);
}

// =============================================================================
// Comparison
// =============================================================================

TEST_F(SootcTest, IntegerEqual) {
    auto result = run(R"(
        (defun eq () (== 5 5))
    )",
                      "eq");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IntegerNotEqual) {
    auto result = run(R"(
        (defun ne () (!= 5 3))
    )",
                      "ne");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IntegerLessThan) {
    auto result = run(R"(
        (defun lt () (< 3 5))
    )",
                      "lt");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IntegerGreaterThan) {
    auto result = run(R"(
        (defun gt () (> 5 3))
    )",
                      "gt");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IntegerLessThanEqual) {
    auto result = run(R"(
        (defun le () (<= 5 5))
    )",
                      "le");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IntegerGreaterThanEqual) {
    auto result = run(R"(
        (defun ge () (>= 3 5))
    )",
                      "ge");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 0);
}

// =============================================================================
// Unary Operations
// =============================================================================

TEST_F(SootcTest, AbsFunction) {
    auto result = run(R"(
        (defun f () (abs -42))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, NegFunction) {
    auto result = run(R"(
        (defun f () (neg 42))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), -42);
}

TEST_F(SootcTest, NotFunction) {
    auto result = run(R"(
        (defun f () (not #f))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, LogNotFunction) {
    auto result = run(R"(
        (defun f () (lognot 0))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), -1);
}

// =============================================================================
// Control Flow
// =============================================================================

TEST_F(SootcTest, IfTrueBranch) {
    auto result = run(R"(
        (defun f () (if #t 1 2))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, IfFalseBranch) {
    auto result = run(R"(
        (defun f () (if #f 1 2))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 2);
}

TEST_F(SootcTest, IfWithoutElse) {
    auto result = run(R"(
        (defun f () (if #f 1))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    // Without an else branch, the result is whatever was in the
    // result register — we only check that the function returns.
}

TEST_F(SootcTest, CondFirstClause) {
    auto result = run(R"(
        (defun f () (cond (#t 1) (#t 2)))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 1);
}

TEST_F(SootcTest, CondSecondClause) {
    auto result = run(R"(
        (defun f () (cond (#f 1) (#t 2)))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 2);
}

TEST_F(SootcTest, CondElseClause) {
    auto result = run(R"(
        (defun f () (cond (#f 1) (#f 2) (else 3)))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 3);
}

TEST_F(SootcTest, LetBinding) {
    auto result = run(R"(
        (defun f () (let ((x 5) (y 3)) (+ x y)))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 8);
}

TEST_F(SootcTest, NestedLet) {
    auto result = run(R"(
        (defun f ()
          (let ((x 2))
            (let ((y 3))
              (* x y))))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

TEST_F(SootcTest, BeginSequence) {
    auto result = run(R"(
        (defun f () (begin 1 2 3))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 3);
}

TEST_F(SootcTest, WhileLoop) {
    auto result = run(R"(
        (defun f ()
          (let ((i 0) (sum 0))
            (while (< i 5)
              (set! sum (+ sum i))
              (set! i (+ i 1)))
            sum))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 10); // 0+1+2+3+4
}

// =============================================================================
// Function Calls
// =============================================================================

TEST_F(SootcTest, CallUserFunction) {
    auto result = run(R"(
        (defun helper () 42)
        (defun get () (helper))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, CallWithArguments) {
    auto result = run(R"(
        (defun add (a b) (+ a b))
        (defun get () (add 3 4))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 7);
}

TEST_F(SootcTest, NestedFunctionCalls) {
    auto result = run(R"(
        (defun add1 (x) (+ x 1))
        (defun add2 (x) (add1 (add1 x)))
        (defun get () (add2 5))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 7);
}

// =============================================================================
// Parameters and Variables
// =============================================================================

TEST_F(SootcTest, SingleParameter) {
    auto result = run(R"(
        (defun identity (x) x)
    )",
                      "identity");
    ASSERT_TRUE(result.ok) << result.error;
    // Cannot easily pass args through the current helper; the test only
    // verifies that the function compiles and can be invoked with zero args.
}

TEST_F(SootcTest, MultipleParameters) {
    auto result = run(R"(
        (defun add3 (a b c) (+ (+ a b) c))
        (defun get () (add3 1 2 3))
    )",
                      "get");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 6);
}

// =============================================================================
// Set!
// =============================================================================

TEST_F(SootcTest, SetLocalVariable) {
    auto result = run(R"(
        (defun f ()
          (let ((x 1))
            (set! x 42)
            x))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 42);
}

TEST_F(SootcTest, SetAndReturn) {
    auto result = run(R"(
        (defun f ()
          (let ((x 0))
            (set! x (+ x 100))
            x))
    )",
                      "f");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value.to_int(), 100);
}