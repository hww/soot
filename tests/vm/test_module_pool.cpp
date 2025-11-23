#include "gtest/gtest.h"
#include "vm/module_pool.hpp"
#include <vector>

using namespace vm;

class ModulePoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        ModulePool::shutdown(); // ќчищаем перед каждым тестом
    }

    void TearDown() override {
        ModulePool::shutdown(); // ќчищаем после каждого теста
    }
};

TEST_F(ModulePoolTest, Initialization) {
    EXPECT_FALSE(ModulePool::is_initialized());

    bool result = ModulePool::initialize(1024);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ModulePool::is_initialized());
    EXPECT_EQ(ModulePool::get_pool_size(), 1024);
    EXPECT_EQ(ModulePool::get_used_memory(), 0);
    EXPECT_EQ(ModulePool::get_free_memory(), 1024);
}

TEST_F(ModulePoolTest, LoadSingleModule) {
    ModulePool::initialize(1024);

    std::vector<u8> module_data = { 0x01, 0x02, 0x03, 0x04 };
    void* module_addr = ModulePool::load_module(module_data);

    EXPECT_NE(module_addr, nullptr);
    EXPECT_EQ(ModulePool::get_module_count(), 1);
    EXPECT_EQ(ModulePool::get_used_memory(), 4); // 4 байта выровненные до 4
    EXPECT_EQ(ModulePool::get_free_memory(), 1020);

    // ѕровер€ем что данные скопированы
    u8* data_ptr = static_cast<u8*>(module_addr);
    EXPECT_EQ(data_ptr[0], 0x01);
    EXPECT_EQ(data_ptr[1], 0x02);
    EXPECT_EQ(data_ptr[2], 0x03);
    EXPECT_EQ(data_ptr[3], 0x04);
}

TEST_F(ModulePoolTest, LoadMultipleModules) {
    ModulePool::initialize(100);

    std::vector<u8> module1 = { 0x01, 0x02 }; // 2 байта -> 4 с выравниванием
    std::vector<u8> module2 = { 0x03, 0x04, 0x05 }; // 3 байта -> 4 с выравниванием

    void* addr1 = ModulePool::load_module(module1);
    void* addr2 = ModulePool::load_module(module2);

    EXPECT_NE(addr1, nullptr);
    EXPECT_NE(addr2, nullptr);
    EXPECT_EQ(ModulePool::get_module_count(), 2);
    EXPECT_EQ(ModulePool::get_used_memory(), 8); // 4 + 4
    EXPECT_EQ(ModulePool::get_free_memory(), 92);

    // ћодули не должны перекрыватьс€
    EXPECT_NE(addr1, addr2);
    EXPECT_GT(static_cast<u8*>(addr2), static_cast<u8*>(addr1));
}

TEST_F(ModulePoolTest, OutOfMemory) {
    ModulePool::initialize(10); // ћаленький пул

    std::vector<u8> large_module(20, 0xAA); // 20 байт
    void* result = ModulePool::load_module(large_module);

    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(ModulePool::get_module_count(), 0);
    EXPECT_EQ(ModulePool::get_used_memory(), 0);
}

TEST_F(ModulePoolTest, Alignment) {
    ModulePool::initialize(100);

    // ћодули разного размера дл€ проверки выравнивани€
    std::vector<u8> module3 = { 1, 2, 3 };          // 3 байта -> 4
    std::vector<u8> module5 = { 1, 2, 3, 4, 5 };    // 5 байт -> 8
    std::vector<u8> module2 = { 1, 2 };             // 2 байта -> 4

    void* addr1 = ModulePool::load_module(module3);
    void* addr2 = ModulePool::load_module(module5);
    void* addr3 = ModulePool::load_module(module2);

    EXPECT_NE(addr1, nullptr);
    EXPECT_NE(addr2, nullptr);
    EXPECT_NE(addr3, nullptr);

    // ѕровер€ем выравнивание
    u32 used = ModulePool::get_used_memory();
    EXPECT_EQ(used, 16); // 4 + 8 + 4

    // ѕровер€ем что модули идут последовательно
    u8* a1 = static_cast<u8*>(addr1);
    u8* a2 = static_cast<u8*>(addr2);
    u8* a3 = static_cast<u8*>(addr3);

    EXPECT_EQ(a2 - a1, 4); // module3 зан€л 4 байта
    EXPECT_EQ(a3 - a2, 8); // module5 зан€л 8 байт
}

TEST_F(ModulePoolTest, ShutdownAndReinitialize) {
    ModulePool::initialize(100);
    std::vector<u8> module = { 0xAA };
    void* addr1 = ModulePool::load_module(module);

    EXPECT_NE(addr1, nullptr);
    EXPECT_EQ(ModulePool::get_module_count(), 1);

    // ѕереинициализаци€
    ModulePool::shutdown();
    EXPECT_FALSE(ModulePool::is_initialized());
    EXPECT_EQ(ModulePool::get_module_count(), 0);

    // ѕовторна€ инициализаци€
    bool result = ModulePool::initialize(200);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ModulePool::is_initialized());
    EXPECT_EQ(ModulePool::get_pool_size(), 200);
}

TEST_F(ModulePoolTest, GetModuleAddress) {
    ModulePool::initialize(100);

    std::vector<u8> module1 = { 0x01 };
    std::vector<u8> module2 = { 0x02 };

    void* addr1 = ModulePool::load_module(module1);
    void* addr2 = ModulePool::load_module(module2);

    EXPECT_EQ(ModulePool::get_module_address(0), addr1);
    EXPECT_EQ(ModulePool::get_module_address(1), addr2);
    EXPECT_EQ(ModulePool::get_module_address(2), nullptr); // ¬ыход за границы
}

TEST_F(ModulePoolTest, Utilization) {
    ModulePool::initialize(100);

    std::vector<u8> module(50, 0xAA); // 50 байт -> 52 с выравниванием
    void* addr = ModulePool::load_module(module);

    EXPECT_NE(addr, nullptr);
    EXPECT_NEAR(ModulePool::get_utilization(), 52.0, 0.1); // 52%
}