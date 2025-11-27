#include "gtest/gtest.h"
#include "common/runtime/lib/Ptr.hpp"

using namespace runtime::lib;

// Тестовая структура
struct TestStruct {
    int value;
    char name[16];

    TestStruct(int v = 0, const char* n = "") : value(v) {
        strncpy(name, n, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

class PtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем тестовые данные в обычной памяти
        test_data = TestStruct(42, "test");

        // Создаем тестовый массив
        for (int i = 0; i < 5; i++) {
            array_data[i] = TestStruct(i * 10, std::to_string(i).c_str());
        }
    }

    TestStruct test_data;
    TestStruct array_data[5];
};

TEST_F(PtrTest, BasicCreationAndDereference) {
    Ptr<TestStruct> ptr(&test_data);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr->value, 42);
    EXPECT_STREQ(ptr->name, "test");
}

TEST_F(PtrTest, NullPointer) {
    Ptr<TestStruct> null_ptr;
    EXPECT_TRUE(null_ptr.is_null());
    EXPECT_FALSE(static_cast<bool>(null_ptr));
    EXPECT_EQ(null_ptr.c(), nullptr);
}

TEST_F(PtrTest, MakePtrFunction) {
    Ptr<TestStruct> ptr = make_ptr(&test_data);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr->value, 42);
}

TEST_F(PtrTest, PointerArithmetic) {
    Ptr<TestStruct> base_ptr(array_data);

    Ptr<TestStruct> next_ptr = base_ptr + 1;
    EXPECT_EQ(next_ptr->value, 10);

    Ptr<TestStruct> prev_ptr = next_ptr - 1;
    EXPECT_EQ(prev_ptr->value, 0);

    EXPECT_EQ(next_ptr - base_ptr, 1);
}

TEST_F(PtrTest, ArrayAccess) {
    Ptr<TestStruct> array_ptr(array_data);

    for (int i = 0; i < 5; i++) {
        Ptr<TestStruct> elem_ptr = array_ptr + i;
        EXPECT_EQ(elem_ptr->value, i * 10);
        EXPECT_STREQ(elem_ptr->name, std::to_string(i).c_str());
    }
}

TEST_F(PtrTest, TypeCasting) {
    Ptr<TestStruct> struct_ptr(&test_data);

    Ptr<u8> byte_ptr = struct_ptr.cast<u8>();
    EXPECT_EQ(byte_ptr.c(), reinterpret_cast<u8*>(&test_data));

    Ptr<TestStruct> back_ptr = byte_ptr.cast<TestStruct>();
    EXPECT_EQ(back_ptr->value, 42);
}

TEST_F(PtrTest, ComparisonOperators) {
    TestStruct data[2];  // два разных объекта

    Ptr<TestStruct> ptr1(&data[0]);
    Ptr<TestStruct> ptr2(&data[1]);
    Ptr<TestStruct> ptr3(&data[0]);  // тот же что и ptr1

    // Проверяем что offset одинаковые для одинаковых указателей
    EXPECT_EQ(ptr1.ptr, ptr3.ptr);

    // Теперь сравнения должны работать
    EXPECT_TRUE(ptr1 == ptr3);
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_TRUE(ptr1 <= ptr3);
    EXPECT_TRUE(ptr1 >= ptr3);

    // Для разных объектов offset тоже должны быть разными
    EXPECT_NE(ptr1.offset, ptr2.offset);
}

TEST_F(PtrTest, PointerModification) {
    TestStruct data(100, "original");
    Ptr<TestStruct> ptr(&data);

    // Модифицируем через указатель
    ptr->value = 200;
    strcpy(ptr->name, "modified");

    EXPECT_EQ(data.value, 200);
    EXPECT_STREQ(data.name, "modified");
}

TEST_F(PtrTest, BoolConversion) {
    TestStruct data;
    Ptr<TestStruct> valid_ptr(&data);
    Ptr<TestStruct> null_ptr(nullptr);

    EXPECT_TRUE(valid_ptr);
    EXPECT_FALSE(null_ptr);
}

TEST_F(PtrTest, CopyAndAssignment) {
    TestStruct data(123, "copy_test");
    Ptr<TestStruct> ptr1(&data);
    Ptr<TestStruct> ptr2 = ptr1; // копирование

    EXPECT_EQ(ptr1->value, 123);
    EXPECT_EQ(ptr2->value, 123);

    ptr2->value = 456;
    EXPECT_EQ(ptr1->value, 456); // оба указывают на одни данные
}

TEST_F(PtrTest, OffsetConstructor) {
    // Тестируем конструктор из смещения
    u64 test_offset = 0x1000;
    Ptr<TestStruct> ptr(test_offset);
    EXPECT_EQ(ptr.offset, test_offset);
    // Нельзя разыменовывать - это просто смещение
}

TEST_F(PtrTest, MixedOffsetAndPointer) {
    // Создаем из указателя
    Ptr<TestStruct> ptr1(&test_data);

    // Конвертируем в смещение и обратно
    u64 offset = ptr1.offset;
    Ptr<TestStruct> ptr2(offset);

    EXPECT_EQ(ptr1.offset, ptr2.offset);
}