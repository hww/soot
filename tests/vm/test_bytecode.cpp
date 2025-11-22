#include "gtest/gtest.h"
#include "vm/bytecode.hpp"

using namespace vm;

TEST(ByteCode, BasicStructure) {
    ByteCode code;
    EXPECT_EQ(sizeof(ByteCode), 16);
}

TEST(ByteCode, FileHeaderValidation) {
    FileHeader header;
    header.magic = DC_MAGIC;
    header.version = CURRENT_VERSION;

    EXPECT_TRUE(header.is_valid());

    header.magic = 0x12345678;
    EXPECT_FALSE(header.is_valid());
}

TEST(BinaryFile, Creation) {
    BinaryFile file;
    file.create(10, 1024); // 10 definitions, 1KB total

    auto header = file.get_header();
    ASSERT_NE(header, nullptr);
    EXPECT_TRUE(header->is_valid());
    EXPECT_EQ(file.get_definition_count(), 0);
}

TEST(BinaryFile, AddFunction) {
    BinaryFile file;
    file.create(10, 1024);

    std::vector<Instruction> code = {
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 25, 42),
        Instruction::create_a(Opcode::RETURN, 25)
    };

    ByteCode* func = file.add_function("test_func"_sid, code);
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(file.get_definition_count(), 1);

    // Verify the function can be retrieved
    ByteCode* found = file.get_function("test_func"_sid);
    EXPECT_EQ(found, func);
}

TEST(BinaryFile, AddData) {
    BinaryFile file;
    file.create(10, 1024);

    Definition* def = file.add_data("pi_value"_sid, "f32"_sid, Variant(3.14159f));
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->name, "pi_value"_sid);
    EXPECT_EQ(def->type, "f32"_sid);
}