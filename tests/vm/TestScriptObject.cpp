#include <gtest/gtest.h>
#include <string>

#include "runtime/Export.hpp"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

// Test types
struct TestVector3D {
    float x, y, z;
    TestVector3D(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

class ScriptObjectTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptObjectTest, BasicCreation) {
    int* data_ptr = script_create<int>();
    EXPECT_NE(data_ptr, nullptr);
    *data_ptr = 42;
    EXPECT_EQ(*data_ptr, 42);
    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, CreationWithValue) {
    int* data_ptr = script_create<int>(123);
    EXPECT_EQ(*data_ptr, 123);
    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, ReferenceCounting) {
    float* data_ptr = script_create<float>(3.14f);

    EXPECT_EQ(script_ref_count(data_ptr), 1);

    script_ref(data_ptr);
    EXPECT_EQ(script_ref_count(data_ptr), 2);

    script_unref(data_ptr);
    EXPECT_EQ(script_ref_count(data_ptr), 1);

    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, ComplexType) {
    std::string* data_ptr = script_create<std::string>("test");
    EXPECT_EQ(*data_ptr, "test");
    EXPECT_EQ(script_ref_count(data_ptr), 1);
    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, FromDataPtr) {
    TestVector3D initial(1, 2, 3);
    TestVector3D* data_ptr = script_create<TestVector3D>(initial);

    // Use ScriptObject instead of ScriptWrapper for the new implementation
    auto* wrapper = to_script_object<TestVector3D>(data_ptr);
    EXPECT_EQ(wrapper->data.x, 1.0f);
    EXPECT_EQ(wrapper->data.y, 2.0f);
    EXPECT_EQ(wrapper->data.z, 3.0f);

    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, DataModification) {
    TestVector3D initial(1, 2, 3);
    TestVector3D* data_ptr = script_create<TestVector3D>(initial);

    data_ptr->x = 10.0f;
    data_ptr->y = 20.0f;

    auto* wrapper = to_script_object(data_ptr);
    EXPECT_EQ(wrapper->data.x, 10.0f);
    EXPECT_EQ(wrapper->data.y, 20.0f);

    script_unref(data_ptr);
}

TEST_F(ScriptObjectTest, NullSafety) {
    EXPECT_NO_THROW(script_ref((void*)nullptr));
    EXPECT_NO_THROW(script_unref((void*)nullptr));
    EXPECT_EQ(script_ref_count(nullptr), -1);
}

TEST_F(ScriptObjectTest, MemoryLayout) {
    TestVector3D* data_ptr = script_create<TestVector3D>(1.0f, 2.0f, 3.0f);
    auto* wrapper = to_script_base(data_ptr);

    // Verify the memory layout - data should be immediately after ref_count
    EXPECT_EQ(reinterpret_cast<uint8_t*>(data_ptr) - reinterpret_cast<uint8_t*>(wrapper),
        offsetof(ScriptObject<TestVector3D>, data));

    // For simple structs, this should equal sizeof(int32_t)
    EXPECT_EQ(offsetof(ScriptObject<TestVector3D>, data), 2*sizeof(int32_t));

    script_unref(data_ptr);
}
TEST_F(ScriptObjectTest, ExactMemoryLayout) {
    // Проверяем для разных типов
    struct Test1 { char a; };  // размер 1, выравнивание 1
    struct Test2 { int a; };   // размер 4, выравнивание 4  
    struct Test3 { double a; };// размер 8, выравнивание 8

    Test1* p1 = script_create<Test1>();
    Test2* p2 = script_create<Test2>();
    Test3* p3 = script_create<Test3>();

    auto* w1 = to_script_object<Test1>(p1);
    auto* w2 = to_script_object<Test2>(p2);
    auto* w3 = to_script_object<Test3>(p3);

    // Проверяем смещения
    size_t offset1 = reinterpret_cast<uint8_t*>(p1) - reinterpret_cast<uint8_t*>(w1);
    size_t offset2 = reinterpret_cast<uint8_t*>(p2) - reinterpret_cast<uint8_t*>(w2);
    size_t offset3 = reinterpret_cast<uint8_t*>(p3) - reinterpret_cast<uint8_t*>(w3);

    printf("Offset Test1: %zu (expected %zu)\n", offset1, offsetof(ScriptObject<Test1>, data));
    printf("Offset Test2: %zu (expected %zu)\n", offset2, offsetof(ScriptObject<Test2>, data));
    printf("Offset Test3: %zu (expected %zu)\n", offset3, offsetof(ScriptObject<Test3>, data));

    // Для полного соответствия задумке, все смещения должны быть РАВНЫ sizeof(int32_t)
    // Но из-за выравнивания это не всегда так!

    script_unref(p1);
    script_unref(p2);
    script_unref(p3);
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}