#include "gtest/gtest.h"
#include "vm/ptr.hpp"
#include "vm/binary_file_pool.hpp"
#include "vm/module.hpp"
#include <vector>

using namespace vm;

// Тестовые структуры
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
        // Инициализируем реальный пул с достаточным размером
        ASSERT_TRUE(BinaryFilePool::initialize(2048));

        // Проверяем, что глобальная переменная установлена
        ASSERT_NE(g_module_pool_base, nullptr);

        // Создаем временный модуль для аллокации
        temp_module = new Module(SID("ptr_test"), SID("test"), "ptr_test.bin");

        setup_test_data();
        setup_array_data();

        // Проверяем, что данные не в offset 0
        ASSERT_GT(test_data_offset, 0u) << "Test data should not be at offset 0";
        ASSERT_GT(array_data_offset, 0u) << "Array data should not be at offset 0";
    }

    void TearDown() override {
        if (temp_module) {
            delete temp_module;
            temp_module = nullptr;
        }
        BinaryFilePool::shutdown();
    }

private:
    void setup_test_data() {
        // Выделяем память для одиночной структуры
        test_data_ptr = BinaryFilePool::allocate(
            sizeof(TestStruct), temp_module, temp_module->name
        );
        ASSERT_NE(test_data_ptr, nullptr);

        // Инициализируем данные
        TestStruct* data = static_cast<TestStruct*>(test_data_ptr);
        *data = TestStruct(42, "test");

        // Вычисляем оффсет
        u8* data_u8 = static_cast<u8*>(test_data_ptr);
        test_data_offset = static_cast<u32>(data_u8 - g_module_pool_base);
    }

    void setup_array_data() {
        // Выделяем память для массива
        array_data_ptr = BinaryFilePool::allocate(
            5 * sizeof(TestStruct), temp_module, temp_module->name
        );
        ASSERT_NE(array_data_ptr, nullptr);

        // Инициализируем массив
        TestStruct* array = static_cast<TestStruct*>(array_data_ptr);
        for (int i = 0; i < 5; i++) {
            array[i] = TestStruct(i * 10, std::to_string(i).c_str());
        }

        // Вычисляем оффсет
        u8* array_u8 = static_cast<u8*>(array_data_ptr);
        array_data_offset = static_cast<u32>(array_u8 - g_module_pool_base);
    }

public:
    void* test_data_ptr = nullptr;
    void* array_data_ptr = nullptr;
    u32 test_data_offset = 0;
    u32 array_data_offset = 0;
    Module* temp_module = nullptr;
};

TEST_F(PtrTest, GlobalPoolBaseIsSet) {
    EXPECT_NE(g_module_pool_base, nullptr);
    EXPECT_EQ(g_module_pool_base, BinaryFilePool::get_base_address());
}

TEST_F(PtrTest, DataNotAtPoolBase) {
    // Проверяем, что данные не в offset 0
    EXPECT_GT(test_data_offset, 0u);
    EXPECT_GT(array_data_offset, 0u);
}

TEST_F(PtrTest, BasicCreationAndDereference) {
    // Проверяем, что данные действительно записаны
    TestStruct* raw_check = static_cast<TestStruct*>(test_data_ptr);
    EXPECT_EQ(raw_check->value, 42);
    EXPECT_STREQ(raw_check->name, "test");

    // Тестируем Ptr
    Ptr<TestStruct> ptr(test_data_offset);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.offset, test_data_offset);

    // Дереференсинг
    EXPECT_EQ(ptr->value, 42);
    EXPECT_STREQ(ptr->name, "test");
}

TEST_F(PtrTest, NullPointer) {
    Ptr<TestStruct> null_ptr(0);
    EXPECT_TRUE(null_ptr.is_null());
    EXPECT_FALSE(static_cast<bool>(null_ptr));
    EXPECT_EQ(null_ptr.c(), nullptr);
}

TEST_F(PtrTest, MakePtrFromRawPointer) {
    TestStruct* raw_ptr = static_cast<TestStruct*>(test_data_ptr);
    Ptr<TestStruct> ptr = make_ptr(raw_ptr);

    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.offset, test_data_offset);
    EXPECT_EQ(ptr->value, 42);
}

TEST_F(PtrTest, PointerArithmetic) {
    Ptr<TestStruct> base_ptr(test_data_offset);

    Ptr<TestStruct> next_ptr = base_ptr + 1;
    EXPECT_EQ(next_ptr.offset, test_data_offset + sizeof(TestStruct));

    Ptr<TestStruct> prev_ptr = base_ptr - 1;
    EXPECT_EQ(prev_ptr.offset, test_data_offset - sizeof(TestStruct));

    EXPECT_EQ(next_ptr - base_ptr, 1);
    EXPECT_EQ(base_ptr - prev_ptr, 1);
}

TEST_F(PtrTest, ArrayAccess) {
    Ptr<TestStruct> array_ptr(array_data_offset);

    for (int i = 0; i < 5; i++) {
        Ptr<TestStruct> elem_ptr = array_ptr + i;
        EXPECT_EQ(elem_ptr->value, i * 10);
        EXPECT_STREQ(elem_ptr->name, std::to_string(i).c_str());
    }
}

TEST_F(PtrTest, TypeCasting) {
    Ptr<TestStruct> struct_ptr(test_data_offset);

    Ptr<u8> byte_ptr = struct_ptr.cast<u8>();
    EXPECT_EQ(byte_ptr.offset, test_data_offset);

    Ptr<TestStruct> back_ptr = byte_ptr.cast<TestStruct>();
    EXPECT_EQ(back_ptr.offset, test_data_offset);
    EXPECT_EQ(back_ptr->value, 42);
}

TEST_F(PtrTest, ConstPointer) {
    Ptr<TestStruct> mutable_ptr(test_data_offset);
    Ptr<const TestStruct> const_ptr = mutable_ptr.cast<const TestStruct>();

    EXPECT_EQ(const_ptr->value, 42);

    // Изменяем через mutable и проверяем через const
    mutable_ptr->value = 100;
    EXPECT_EQ(const_ptr->value, 100);
}

TEST_F(PtrTest, DifferentDataTypes) {
    Ptr<int> int_ptr(test_data_offset);
    Ptr<float> float_ptr(test_data_offset);
    Ptr<char> char_ptr(test_data_offset);

    EXPECT_EQ(int_ptr.c(), static_cast<int*>(test_data_ptr));
    EXPECT_EQ(float_ptr.c(), static_cast<float*>(test_data_ptr));
    EXPECT_EQ(char_ptr.c(), static_cast<char*>(test_data_ptr));
}

TEST_F(PtrTest, PointerToPoolBase) {
    Ptr<u8> base_ptr = make_ptr(g_module_pool_base);
    EXPECT_EQ(base_ptr.offset, 0u);
    EXPECT_TRUE(base_ptr.is_null());  // Базовый указатель пула не считается null
}

TEST_F(PtrTest, PointerAfterPoolCompaction) {
    Ptr<TestStruct> original_ptr(test_data_offset);
    EXPECT_EQ(original_ptr->value, 42);

    // Изменяем значение
    original_ptr->value = 999;
    EXPECT_EQ(original_ptr->value, 999);

    // Компактифицируем пул
    EXPECT_TRUE(BinaryFilePool::compactify());

    // Проверяем, что указатель все еще работает
    EXPECT_EQ(original_ptr->value, 999);

    // Можем продолжить использование
    original_ptr->value = 123;
    EXPECT_EQ(original_ptr->value, 123);
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

TEST_F(PtrTest, ValidOffsets) {
    // Проверяем, что оффсеты валидны и не равны 0
    EXPECT_GT(test_data_offset, 0u);
    EXPECT_GT(array_data_offset, 0u);
    EXPECT_NE(test_data_offset, array_data_offset);

    // Проверяем, что оффсеты выровнены
    EXPECT_EQ(test_data_offset % alignof(TestStruct), 0u);
    EXPECT_EQ(array_data_offset % alignof(TestStruct), 0u);
}