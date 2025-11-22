#include "gtest/gtest.h"
#include "vm/ptr.hpp"
#include "vm/types.hpp"

using namespace vm;

// Mock global memory for testing
u8* vm::g_ee_main_mem = new u8[1024 * 1024]; // 1MB test memory

namespace {
    // Test structures
    struct TestStruct {
        s32 value;
        f32 data;
    };

    struct DerivedStruct : TestStruct {
        s64 extra;
    };
}

class PtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test memory
        std::memset(g_ee_main_mem, 0, 1024 * 1024);
    }

    void TearDown() override {
        // Cleanup
        std::memset(g_ee_main_mem, 0, 1024 * 1024);
    }
};

TEST_F(PtrTest, DefaultConstructor) {
    Ptr<TestStruct> ptr;
    EXPECT_TRUE(ptr.is_null());
    EXPECT_FALSE(ptr.valid());
    EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(PtrTest, ExplicitOffsetConstructor) {
    Ptr<TestStruct> ptr(100);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.get_offset(), 100);
}

TEST_F(PtrTest, MakePtrFromPointer) {
    TestStruct obj{ 42, 3.14f };
    TestStruct* raw_ptr = &obj;

    Ptr<TestStruct> ptr = make_ptr(raw_ptr);
    EXPECT_TRUE(ptr.valid());
    EXPECT_EQ(ptr.get_offset(), static_cast<u32>(reinterpret_cast<u8*>(&obj) - g_ee_main_mem));
}

TEST_F(PtrTest, MakePtrNull) {
    Ptr<TestStruct> ptr = make_ptr<TestStruct>(nullptr);
    EXPECT_TRUE(ptr.is_null());
}

TEST_F(PtrTest, DereferenceOperator) {
    TestStruct obj{ 123, 2.71f };
    Ptr<TestStruct> ptr = make_ptr(&obj);

    EXPECT_EQ((*ptr).value, 123);
    EXPECT_FLOAT_EQ((*ptr).data, 2.71f);

    // Modify through dereference
    (*ptr).value = 456;
    EXPECT_EQ(obj.value, 456);
}

TEST_F(PtrTest, ArrowOperator) {
    TestStruct obj{ 789, 1.41f };
    Ptr<TestStruct> ptr = make_ptr(&obj);

    EXPECT_EQ(ptr->value, 789);
    EXPECT_FLOAT_EQ(ptr->data, 1.41f);

    ptr->value = 999;
    EXPECT_EQ(obj.value, 999);
}

TEST_F(PtrTest, ConstDereference) {
    TestStruct obj{ 111, 9.99f };
    const Ptr<TestStruct> ptr = make_ptr(&obj);

    EXPECT_EQ(ptr->value, 111);
    EXPECT_FLOAT_EQ((*ptr).data, 9.99f);
}

TEST_F(PtrTest, ComparisonOperators) {
    Ptr<TestStruct> ptr1(100);
    Ptr<TestStruct> ptr2(100);
    Ptr<TestStruct> ptr3(200);

    EXPECT_TRUE(ptr1 == ptr2);
    EXPECT_FALSE(ptr1 == ptr3);
    EXPECT_TRUE(ptr1 != ptr3);
    EXPECT_TRUE(ptr1 < ptr3);
    EXPECT_TRUE(ptr3 > ptr1);
    EXPECT_TRUE(ptr1 <= ptr2);
    EXPECT_TRUE(ptr1 >= ptr2);
}

TEST_F(PtrTest, PointerArithmetic) {
    Ptr<TestStruct> ptr(100);

    // Addition
    Ptr<TestStruct> ptr_plus = ptr + 2;
    EXPECT_EQ(ptr_plus.get_offset(), 100 + 2 * sizeof(TestStruct));

    // Subtraction
    Ptr<TestStruct> ptr_minus = ptr - 1;
    EXPECT_EQ(ptr_minus.get_offset(), 100 - sizeof(TestStruct));

    // Difference between pointers
    std::ptrdiff_t diff = ptr_plus - ptr;
    EXPECT_EQ(diff, 2);
}

TEST_F(PtrTest, IncrementDecrement) {
    Ptr<TestStruct> ptr(100);

    // Prefix increment
    Ptr<TestStruct> pre_inc = ++ptr;
    EXPECT_EQ(ptr.get_offset(), 100 + sizeof(TestStruct));
    EXPECT_EQ(pre_inc.get_offset(), ptr.get_offset());

    // Postfix increment
    Ptr<TestStruct> post_inc = ptr++;
    EXPECT_EQ(post_inc.get_offset(), 100 + sizeof(TestStruct));
    EXPECT_EQ(ptr.get_offset(), 100 + 2 * sizeof(TestStruct));

    // Prefix decrement
    Ptr<TestStruct> pre_dec = --ptr;
    EXPECT_EQ(ptr.get_offset(), 100 + sizeof(TestStruct));

    // Postfix decrement
    Ptr<TestStruct> post_dec = ptr--;
    EXPECT_EQ(post_dec.get_offset(), 100 + sizeof(TestStruct));
    EXPECT_EQ(ptr.get_offset(), 100);
}

TEST_F(PtrTest, TypeCasting) {
    Ptr<DerivedStruct> derived_ptr(100);

    // Upcast
    Ptr<TestStruct> base_ptr = derived_ptr.template cast<TestStruct>();
    EXPECT_EQ(base_ptr.get_offset(), 100);

    // Downcast (should work for same offset)
    Ptr<DerivedStruct> back_ptr = base_ptr.template cast<DerivedStruct>();
    EXPECT_EQ(back_ptr.get_offset(), 100);
}

TEST_F(PtrTest, CVoidSpecialization) {
    Ptr<void> void_ptr(150);
    EXPECT_TRUE(void_ptr.valid());
    EXPECT_EQ(void_ptr.get_offset(), 150);

    // Can cast to typed pointer
    Ptr<TestStruct> typed_ptr = void_ptr.cast<TestStruct>();
    EXPECT_EQ(typed_ptr.get_offset(), 150);

    // But no dereference or arithmetic
    // void_ptr++; // This should not compile
    // *void_ptr;  // This should not compile
}

TEST_F(PtrTest, BoolConversion) {
    Ptr<TestStruct> null_ptr;
    Ptr<TestStruct> valid_ptr(100);

    EXPECT_FALSE(static_cast<bool>(null_ptr));
    EXPECT_TRUE(static_cast<bool>(valid_ptr));

    if (valid_ptr) {
        SUCCEED();
    }
    else {
        FAIL();
    }
}

TEST_F(PtrTest, CMethod) {
    TestStruct obj{ 42, 3.14f };
    Ptr<TestStruct> ptr = make_ptr(&obj);

    TestStruct* raw_ptr = ptr.c();
    EXPECT_EQ(raw_ptr, &obj);
    EXPECT_EQ(raw_ptr->value, 42);

    const Ptr<TestStruct> const_ptr = ptr;
    const TestStruct* const_raw_ptr = const_ptr.c();
    EXPECT_EQ(const_raw_ptr, &obj);
}

TEST_F(PtrTest, NullPtrCMethod) {
    Ptr<TestStruct> null_ptr;
    EXPECT_EQ(null_ptr.c(), nullptr);
}

TEST_F(PtrTest, ConstConversion) {
    TestStruct obj{ 42, 3.14f };

    // Non-const to const conversion should work
    Ptr<TestStruct> non_const_ptr = make_ptr(&obj);
    Ptr<const TestStruct> const_ptr = non_const_ptr;  // This should work now

    EXPECT_EQ(const_ptr->value, 42);
    EXPECT_FLOAT_EQ(const_ptr->data, 3.14f);

    // Const to non-const should fail at compile time
    // Ptr<TestStruct> bad_conversion = const_ptr; // This should still fail
}

TEST_F(PtrTest, MakePtrConst) {
    TestStruct obj{ 123, 2.71f };
    const TestStruct* const_ptr = &obj;

    // Should work with const pointers
    Ptr<const TestStruct> ptr = make_ptr(const_ptr);
    EXPECT_EQ(ptr->value, 123);
    EXPECT_FLOAT_EQ(ptr->data, 2.71f);
}