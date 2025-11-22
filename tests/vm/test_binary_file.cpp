#include "gtest/gtest.h"
#include "vm/binary_file.hpp"
#include "vm/instructions.hpp"
#include "vm/ptr.hpp"

using namespace vm;

class BinaryFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize global memory for Ptr tests
        g_ee_main_mem = new u8[1024 * 1024];
    }

    void TearDown() override {
        delete[] g_ee_main_mem;
        g_ee_main_mem = nullptr;
    }
};

TEST_F(BinaryFileTest, ByteCodeBasicStructure) {
    EXPECT_GE(sizeof(ByteCode), 16U); // At least the basic fields
}

TEST_F(BinaryFileTest, BinFileHeaderValidation) {
    BinFileHeader header(SID("test"));
    header.magic_num = DC_MAGIC;

    EXPECT_TRUE(header.is_valid_magic());

    header.magic_num = 0x12345678;
    EXPECT_FALSE(header.is_valid_magic());
}

TEST_F(BinaryFileTest, BinaryFileCreation) {
    BinaryFile file;
    file.initialize(10, 1024); // 10 definitions, 1KB data

    auto header = file.get_header();
    EXPECT_TRUE(header.valid());
    EXPECT_TRUE(header->is_valid_magic());
    EXPECT_EQ(file.get_definition_count(), 0);
    EXPECT_GT(file.get_free_size(), 0U);
}

TEST_F(BinaryFileTest, AddDefinition) {
    BinaryFile file;
    file.initialize(10, 1024);

    auto ptr = file.define(SID("test_func"), SID("function"));
    EXPECT_TRUE(ptr.valid());
    EXPECT_EQ(file.get_definition_count(), 1);

    // Verify definition was added
    auto def = file.get_definition(0);
    EXPECT_TRUE(def.valid());
    EXPECT_EQ(def->name, SID("test_func"));
    EXPECT_EQ(def->type, SID("function"));
}

TEST_F(BinaryFileTest, DefinitionBoundsChecking) {
    BinaryFile file;
    file.initialize(2, 1024); // Only 2 definitions

    file.define(SID("first"), SID("function"));
    file.define(SID("second"), SID("function"));

    // Should throw on third definition
    EXPECT_THROW(file.define(SID("third"), SID("function")), ByteCodeError);
}

TEST_F(BinaryFileTest, GetDefinitionPtr) {
    BinaryFile file;
    file.initialize(5, 1024);

    auto def_ptr = file.define<s32>(SID("answer"), SID("int"));
    EXPECT_TRUE(def_ptr.valid());

    // Should be able to retrieve and use
    auto retrieved = file.get_definition_ptr<s32>(0);
    EXPECT_TRUE(retrieved.valid());

    // Can write through pointer
    *retrieved = 42;
    EXPECT_EQ(*def_ptr, 42);
}

TEST_F(BinaryFileTest, BinaryFileBuilderBasic) {
    BinaryFileBuilder builder(10);

    builder.add_definition(SID("main"), SID("function"));
    builder.add_definition(SID("data"), SID("global"));

    std::vector<Instruction> code = {
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100),
        Instruction::create_a(Opcode::RETURN, 1)
    };
    builder.add_code(code);

    std::vector<Record> data = {
        Record{.as_int32 = {42, 0} }
    };
    builder.add_data(data);

    // Add debug info
    builder.add_debug_info(0, 10, SID("main.goal"));
    builder.add_debug_info(4, 11, SID("main.goal"));

    auto binary_data = builder.build();
    EXPECT_FALSE(binary_data.empty());
}

TEST_F(BinaryFileTest, LoadFromBuilderData) {
    BinaryFileBuilder builder(5);
    builder.add_definition(SID("test"), SID("function"));

    auto binary_data = builder.build();

    BinaryFile file;
    file.load(std::move(binary_data));

    EXPECT_TRUE(file.is_loaded());
    EXPECT_EQ(file.get_definition_count(), 1);

    auto def = file.get_definition(0);
    EXPECT_EQ(def->name, SID("test"));
}

TEST_F(BinaryFileTest, ByteCodeDebugInfo) {
    BinaryFileBuilder builder(5);

    std::vector<Instruction> code = {
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100),
        Instruction::create_a(Opcode::RETURN, 1)
    };
    builder.add_code(code);

    // Add debug information
    builder.add_debug_info(0, 15, SID("test.goal"));  // First instruction at line 15
    builder.add_debug_info(4, 16, SID("test.goal"));  // Second instruction at line 16

    auto binary_data = builder.build();
    BinaryFile file;
    file.load(std::move(binary_data));

    auto header = file.get_header();
    auto bytecode_ptr = header->get_definition_ptr<ByteCode>(0);
    EXPECT_TRUE(bytecode_ptr.valid());

    EXPECT_TRUE(bytecode_ptr->has_debug_info());
    EXPECT_EQ(bytecode_ptr->debug_count, 2);

    // Test debug info lookup
    auto location = bytecode_ptr->find_source_location(header, 0);
    EXPECT_EQ(location.source_line, 15);
    EXPECT_EQ(location.source_file, SID("test.goal"));
}

TEST_F(BinaryFileTest, InvalidFileMagic) {
    std::vector<u8> bad_data(100, 0xFF); // Fill with invalid data

    BinaryFile file;
    EXPECT_THROW(file.load(std::move(bad_data)), ByteCodeError);
}

TEST_F(BinaryFileTest, MoveSemantics) {
    BinaryFile file1;
    file1.initialize(5, 512);
    file1.define(SID("test"), SID("function"));

    BinaryFile file2 = std::move(file1);
    EXPECT_FALSE(file1.is_loaded()); // NOLINT
    EXPECT_TRUE(file2.is_loaded());
    EXPECT_EQ(file2.get_definition_count(), 1);
}