#include "gtest/gtest.h"
#include "runtime/Export.hpp"
#include <vector>

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

class BinaryFilePoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        BinaryFilePool::initialize(1024);
    }

    void TearDown() override {
        BinaryFilePool::shutdown();
    }
};

TEST_F(BinaryFilePoolTest, Initialization) {
    BinaryFilePool::shutdown();
    EXPECT_FALSE(BinaryFilePool::is_initialized());

    bool result = BinaryFilePool::initialize(1024);
    EXPECT_TRUE(result);
    EXPECT_TRUE(BinaryFilePool::is_initialized());
    EXPECT_EQ(BinaryFilePool::get_pool_size(), 1024);
    EXPECT_EQ(BinaryFilePool::get_used_memory(), 0);
    EXPECT_EQ(BinaryFilePool::get_free_memory(), 1024);
}

TEST_F(BinaryFilePoolTest, AllocateSingleModule) {
    

    Module module(SID("test_module"), "test.bin");
    std::vector<u8> module_data = { 0x01, 0x02, 0x03, 0x04 };

    void* module_addr = BinaryFilePool::allocate(
        static_cast<u32>(module_data.size()),
        &module,
        module.name
    );

    EXPECT_NE(module_addr, nullptr);
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 1);
    EXPECT_EQ(BinaryFilePool::get_used_memory(), 4);
    EXPECT_EQ(BinaryFilePool::get_free_memory(), 1020);

    EXPECT_EQ(module.binary_file, reinterpret_cast<BinaryFile*>(module_addr));
    EXPECT_EQ(module.generation, 1);

    std::memcpy(module_addr, module_data.data(), module_data.size());
    u8* data_ptr = static_cast<u8*>(module_addr);
    EXPECT_EQ(data_ptr[0], 0x01);
    EXPECT_EQ(data_ptr[1], 0x02);
    EXPECT_EQ(data_ptr[2], 0x03);
    EXPECT_EQ(data_ptr[3], 0x04);
}

TEST_F(BinaryFilePoolTest, AllocateMultipleModules) {
    

    Module module1(SID("module1"), "m1.bin");
    Module module2(SID("module2"), "m2.bin");

    std::vector<u8> data1 = { 0x01, 0x02 };
    std::vector<u8> data2 = { 0x03, 0x04, 0x05 };

    auto free = BinaryFilePool::get_free_memory();
    auto expect_used = 8;
    void* addr1 = BinaryFilePool::allocate(
        static_cast<u32>(data1.size()), &module1, module1.name);
    void* addr2 = BinaryFilePool::allocate(
        static_cast<u32>(data2.size()), &module2, module2.name);

    EXPECT_NE(addr1, nullptr);
    EXPECT_NE(addr2, nullptr);
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 2);
    EXPECT_EQ(BinaryFilePool::get_used_memory(), expect_used);
    EXPECT_EQ(BinaryFilePool::get_free_memory(), free - expect_used);

    EXPECT_NE(addr1, addr2);
    EXPECT_GT(static_cast<u8*>(addr2), static_cast<u8*>(addr1));

    EXPECT_EQ(module1.binary_file, reinterpret_cast<BinaryFile*>(addr1));
    EXPECT_EQ(module2.binary_file, reinterpret_cast<BinaryFile*>(addr2));
}

TEST_F(BinaryFilePoolTest, OutOfMemory) {
   

   Module module(SID("large_module"), "large.bin");
   std::vector<u8> large_data(10+BinaryFilePool::get_free_memory(), 0xAA);
   
   void* result = BinaryFilePool::allocate(
       static_cast<u32>(large_data.size()), &module, module.name);
   
   EXPECT_EQ(result, nullptr);
   EXPECT_EQ(BinaryFilePool::get_allocation_count(), 0);
   EXPECT_EQ(BinaryFilePool::get_used_memory(), 0);
}

TEST_F(BinaryFilePoolTest, Alignment) {
    
    Module module1(SID("mod1"), "m1.bin");
    Module module2(SID("mod2"), "m2.bin");
    Module module3(SID("mod3"), "m3.bin");

    void* addr1 = BinaryFilePool::allocate(3, &module1, module1.name);
    void* addr2 = BinaryFilePool::allocate(5, &module2, module2.name);
    void* addr3 = BinaryFilePool::allocate(2, &module3, module3.name);

    EXPECT_NE(addr1, nullptr);
    EXPECT_NE(addr2, nullptr);
    EXPECT_NE(addr3, nullptr);

    u32 used = BinaryFilePool::get_used_memory();
    EXPECT_EQ(used, 16);

    u8* a1 = static_cast<u8*>(addr1);
    u8* a2 = static_cast<u8*>(addr2);
    u8* a3 = static_cast<u8*>(addr3);

    EXPECT_EQ(a2 - a1, 4);
    EXPECT_EQ(a3 - a2, 8);
}

TEST_F(BinaryFilePoolTest, Deallocation) {
    

    Module module(SID("test_module"), "test.bin");
    std::vector<u8> data = { 0x01, 0x02 };

    void* addr = BinaryFilePool::allocate(
        static_cast<u32>(data.size()), &module, module.name);

    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 1);

    EXPECT_EQ(module.binary_file, reinterpret_cast<BinaryFile*>(addr));

    bool result = BinaryFilePool::deallocate(module.name);
    EXPECT_TRUE(result);
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 0);

    EXPECT_EQ(module.binary_file, nullptr);
}

TEST_F(BinaryFilePoolTest, FindAllocation) {
    

    Module module1(SID("module1"), "m1.bin");
    Module module2(SID("module2"), "m2.bin");

    void* addr1 = BinaryFilePool::allocate(1, &module1, module1.name);
    void* addr2 = BinaryFilePool::allocate(1, &module2, module2.name);

    EXPECT_EQ(BinaryFilePool::find_allocation(module1.name), addr1);
    EXPECT_EQ(BinaryFilePool::find_allocation(module2.name), addr2);
    EXPECT_EQ(BinaryFilePool::find_allocation(SID("nonexistent")), nullptr);
}

TEST_F(BinaryFilePoolTest, Utilization) {
     Module module(SID("test_module"), "test.bin");
    void* addr = BinaryFilePool::allocate(BinaryFilePool::get_free_memory() / 2, &module, module.name);

    EXPECT_NE(addr, nullptr);
    EXPECT_NEAR(BinaryFilePool::get_utilization(), 50.0, 0.1);
}

TEST_F(BinaryFilePoolTest, ShutdownAndReinitialize) {
    

    Module module(SID("test_module"), "test.bin");
    void* addr = BinaryFilePool::allocate(1, &module, module.name);

    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 1);

    EXPECT_EQ(module.binary_file, reinterpret_cast<BinaryFile*>(addr));

    BinaryFilePool::shutdown();
    EXPECT_FALSE(BinaryFilePool::is_initialized());
    EXPECT_EQ(BinaryFilePool::get_allocation_count(), 0);

    EXPECT_EQ(module.binary_file, nullptr);

    bool result = BinaryFilePool::initialize(200);
    EXPECT_TRUE(result);
    EXPECT_TRUE(BinaryFilePool::is_initialized());
    EXPECT_EQ(BinaryFilePool::get_pool_size(), 200);
}