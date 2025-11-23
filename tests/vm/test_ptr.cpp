#include "gtest/gtest.h"
#include "vm/ptr.hpp"
#include <vector>

using namespace vm;

// Тестовые структуры
struct TestStruct {
    int value;
    char name[16];
};

class PtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем тестовый пул вручную для тестов Ptr
        test_pool_base = new u8[1024];
        g_module_pool_base = test_pool_base;

        // Инициализируем тестовые данные в пуле
        test_data_offset = 100;
        TestStruct* data = reinterpret_cast<TestStruct*>(test_pool_base + test_data_offset);
        data->value = 42;
        strcpy(data->name, "test");

        array_data_offset = 200;
        for (int i = 0; i < 5; i++) {
            TestStruct* elem = reinterpret_cast<TestStruct*>(test_pool_base + array_data_offset + i * sizeof(TestStruct));
            elem->value = i * 10;
            strcpy(elem->name, std::to_string(i).c_str());
        }
    }

    void TearDown() override {
        delete[] test_pool_base;
        g_module_pool_base = nullptr;
    }

    u8* test_pool_base;
    u32 test_data_offset;
    u32 array_data_offset;
};


TEST_F(PtrTest, BasicCreationAndDereference) {
    // Явное создание через конструктор
    Ptr<TestStruct> ptr = Ptr<TestStruct>(test_data_offset);

    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr->value, 42);
    EXPECT_STREQ(ptr->name, "test");
}

TEST_F(PtrTest, NullPointer) {
    Ptr<TestStruct> null_ptr = Ptr<TestStruct>(0);  // Явно через конструктор

    EXPECT_TRUE(null_ptr.is_null());
    EXPECT_FALSE(static_cast<bool>(null_ptr));
    EXPECT_EQ(null_ptr.c(), nullptr);
}

TEST_F(PtrTest, PointerArithmetic) {
    Ptr<TestStruct> base_ptr = Ptr<TestStruct>(test_data_offset);

    // Арифметика указателей (уже учитывает sizeof)
    Ptr<TestStruct> ptr2 = base_ptr + 1;
    EXPECT_EQ(ptr2.offset, test_data_offset + sizeof(TestStruct));

    Ptr<TestStruct> ptr3 = base_ptr - 1;
    EXPECT_EQ(ptr3.offset, test_data_offset - sizeof(TestStruct));

    // Разность указателей
    EXPECT_EQ(ptr2 - base_ptr, 1);
    EXPECT_EQ(base_ptr - ptr3, 1);
}

TEST_F(PtrTest, TypeCasting) {
    Ptr<TestStruct> struct_ptr = Ptr<TestStruct>(test_data_offset);

    // Кастинг к другому типу (теперь без static_assert)
    Ptr<u8> byte_ptr = struct_ptr.cast<u8>();
    EXPECT_EQ(byte_ptr.offset, test_data_offset);

    // Кастинг обратно
    Ptr<TestStruct> back_ptr = byte_ptr.cast<TestStruct>();
    EXPECT_EQ(back_ptr.offset, test_data_offset);
}

TEST_F(PtrTest, MakePtrFromRawPointer) {
    // Получаем сырой указатель на данные в пуле
    TestStruct* raw_ptr = reinterpret_cast<TestStruct*>(test_pool_base + test_data_offset);

    // Создаем Ptr через make_ptr
    Ptr<TestStruct> ptr = make_ptr(raw_ptr);

    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.offset, test_data_offset);
    EXPECT_EQ(ptr->value, 42);
}

TEST_F(PtrTest, MakePtrFromNull) {
    Ptr<TestStruct> ptr = make_ptr<TestStruct>(nullptr);
    EXPECT_TRUE(ptr.is_null());
}


TEST_F(PtrTest, ComparisonOperators) {
    Ptr<TestStruct> ptr1(test_data_offset);
    Ptr<TestStruct> ptr2(test_data_offset + 100);
    Ptr<TestStruct> ptr3(test_data_offset);

    EXPECT_TRUE(ptr1 == ptr3);
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_TRUE(ptr1 < ptr2);
    EXPECT_TRUE(ptr2 > ptr1);
    EXPECT_TRUE(ptr1 <= ptr3);
    EXPECT_TRUE(ptr1 >= ptr3);
}

TEST_F(PtrTest, ArrayAccess) {
    Ptr<TestStruct> array_ptr(array_data_offset);

    // Доступ к элементам массива через арифметику
    for (int i = 0; i < 5; i++) {
        Ptr<TestStruct> element_ptr = array_ptr + i;
        EXPECT_EQ(element_ptr->value, i * 10);
        EXPECT_STREQ(element_ptr->name, std::to_string(i).c_str());
    }
}

TEST_F(PtrTest, ConstPointer) {
    Ptr<TestStruct> mutable_ptr(test_data_offset);
    Ptr<const TestStruct> const_ptr = mutable_ptr.cast<const TestStruct>();

    // Можем читать через const
    EXPECT_EQ(const_ptr->value, 42);

    // Изменяем через mutable
    mutable_ptr->value = 100;
    EXPECT_EQ(const_ptr->value, 100); // Видим изменения
}

TEST_F(PtrTest, DifferentDataTypes) {
    // Тестируем Ptr с разными типами
    Ptr<int> int_ptr(test_data_offset);
    Ptr<float> float_ptr(test_data_offset);
    Ptr<char> char_ptr(test_data_offset);

    // Все должны работать со своими типами
    EXPECT_EQ(int_ptr.c(), reinterpret_cast<int*>(test_pool_base + test_data_offset));
    EXPECT_EQ(float_ptr.c(), reinterpret_cast<float*>(test_pool_base + test_data_offset));
    EXPECT_EQ(char_ptr.c(), reinterpret_cast<char*>(test_pool_base + test_data_offset));
}