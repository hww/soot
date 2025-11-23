#include "gtest/gtest.h"
#include "vm/binary_file.hpp"
#include "vm/instructions.hpp"

using namespace vm;

class BinaryFileTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(BinaryFileTest, ByteCodeBasicStructure) {
    EXPECT_GE(sizeof(ByteCode), 16U); // At least the basic fields
}

TEST_F(BinaryFileTest, BinFileHeaderValidation) {
    BinFileHeader header;  // Убрал скобки - это объявление переменной, не функции
    header.magic_num = DC_MAGIC;

    EXPECT_TRUE(header.is_valid_magic());

    header.magic_num = 0x12345678;
    EXPECT_FALSE(header.is_valid_magic());
}

TEST_F(BinaryFileTest, BinaryFileCreation) {
    BinaryFile file;
    file.initialize(1024); // Только data_size, без max_definitions

    auto header = file.get_header();
    EXPECT_TRUE(header->is_valid_magic());
    EXPECT_EQ(file.get_definition_count(), 0);
    // get_free_size больше нет - убрать эту проверку
}

TEST_F(BinaryFileTest, AddDefinitionViaBuilder) {
    BinaryFileBuilder builder;

    builder.add_definition(SID("test_func"), SID("function"));
    builder.add_definition(SID("test_data"), SID("global"));

    auto binary_file = builder.build_file();

    EXPECT_EQ(binary_file->get_definition_count(), 2);

    // Verify definition was added
    auto def = binary_file->get_definition(0);
    EXPECT_EQ(def->name, SID("test_func"));
    EXPECT_EQ(def->type, SID("function"));
}

TEST_F(BinaryFileTest, DefinitionBoundsChecking) {
    BinaryFileBuilder builder;

    builder.add_definition(SID("first"), SID("function"));
    builder.add_definition(SID("second"), SID("function"));

    auto binary_file = builder.build_file();

    // Должен нормально работать с двумя определениями
    EXPECT_EQ(binary_file->get_definition_count(), 2);

    // Попытка получить несуществующее определение должна бросать исключение
    EXPECT_THROW(binary_file->get_definition(2), ByteCodeError);
}

TEST_F(BinaryFileTest, GetDefinitionPtr) {
    BinaryFileBuilder builder;

    builder.add_definition(SID("answer"), SID("int"));

    auto binary_file = builder.build_file();

    // Получаем указатель на данные определения
    auto retrieved = binary_file->get_definition_ptr<ByteCode>(0);
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(BinaryFileTest, BinaryFileBuilderBasic) {
    BinaryFileBuilder builder;  // Без аргументов

    builder.add_definition(SID("main"), SID("function"));
    builder.add_definition(SID("data"), SID("global"));

    std::vector<Instruction> code = {
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100),
        Instruction::create_a(Opcode::RETURN, 1)
    };
    builder.add_code(code);

    std::vector<Record> data = {
        Record{.as_s32 = 42 }
    };
    builder.add_data(data);

    // Add debug info
    builder.add_debug_info(0, 10, SID("main.goal"));
    builder.add_debug_info(4, 11, SID("main.goal"));

    auto binary_data = builder.build();
    EXPECT_FALSE(binary_data.empty());
}

TEST_F(BinaryFileTest, LoadFromBuilderData) {
    BinaryFileBuilder builder;  // Без аргументов
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
    BinaryFileBuilder builder;  // Без аргументов

    std::vector<Instruction> code = {
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100),
        Instruction::create_a(Opcode::RETURN, 1)
    };

    // Use add_function which automatically adds the definition
    builder.add_function(SID("main"), code);

    // Add debug information
    builder.add_debug_info(0, 15, SID("test.goal"));  // First instruction at line 15
    builder.add_debug_info(4, 16, SID("test.goal"));  // Second instruction at line 16

    auto binary_file = builder.build_file();

    auto header = binary_file->get_header();
    auto bytecode_ptr = header->get_definition_ptr<ByteCode>(0);

    EXPECT_TRUE(bytecode_ptr->has_debug_info());
    EXPECT_EQ(bytecode_ptr->debug_count, 2);

    // Test debug info lookup
    auto location = bytecode_ptr->find_source_location(0);
    EXPECT_EQ(location.source_line, 15);
    EXPECT_EQ(location.source_file, SID("test.goal"));
}

TEST_F(BinaryFileTest, InvalidFileMagic) {
    std::vector<u8> bad_data(100, 0xFF); // Fill with invalid data

    BinaryFile file;
    EXPECT_THROW(file.load(std::move(bad_data)), ByteCodeError);
}

TEST_F(BinaryFileTest, MoveSemantics) {
    BinaryFileBuilder builder;
    builder.add_definition(SID("test"), SID("function"));

    auto file1 = builder.build_file();
    EXPECT_TRUE(file1->is_loaded());
    EXPECT_EQ(file1->get_definition_count(), 1);

    // Move construction
    BinaryFile file2 = std::move(*file1);
    EXPECT_FALSE(file1->is_loaded()); // NOLINT
    EXPECT_TRUE(file2.is_loaded());
    EXPECT_EQ(file2.get_definition_count(), 1);
}