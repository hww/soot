// test_relocation.cpp
#include "gtest/gtest.h"
#include "carbon/Export.hpp"
#include "fmt/format.h"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;

class RelocationTest : public ::testing::Test {
protected:
    void SetUp() override {
        string_id::initialize();
    }
    
    // Создаем тестовый бинарник с известной структурой
    std::vector<u8> create_test_binary() {
        BinaryFileBuilder builder;
        
        // Добавляем тестовую функцию
        std::vector<Instruction> code = {
            Instruction::create_a(Opcode::RETURN, 0)
        };
        
        builder.add_function(SID("test_func"), code, {}, {});
        
        // Получаем бинарник
        std::vector<u8> binary = builder.build();
        
        lg::info("Created test binary of size: {}", binary.size());
        return binary;
    }
};

TEST_F(RelocationTest, BinaryFileHeaderInitialization) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* file = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Проверяем, что base_offset инициализирован
    lg::info("base_offset before relocation: {}", (void*)file->base_offset);
    
    // После создания base_offset должен указывать на себя
    EXPECT_EQ(file->base_offset, file);
    
    lg::info("Magic: 0x{:08X}, expected: 0x{:08X}", file->magic, BinaryFile::MAGIC);
    EXPECT_EQ(file->magic, BinaryFile::MAGIC);
}

TEST_F(RelocationTest, RelocatePointersWithNullBase) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* file = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Сохраняем исходные значения
    u32 original_definitions_offset = file->definitions.offset;
    
    // Устанавливаем base_offset в nullptr (симулируем незагруженное состояние)
    file->base_offset = nullptr;
    
    // Вызываем relocate_pointers
    file->relocate_pointers();
    
    // После вызова base_offset должен указывать на file
    EXPECT_EQ(file->base_offset, file);
    
    // Указатели не должны измениться при первом вызове
    EXPECT_EQ(file->definitions.offset, original_definitions_offset);
}

TEST_F(RelocationTest, RelocatePointersWithDelta) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* original = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Сохраняем исходные значения
    u32 original_definitions_offset = original->definitions.offset;
    
    // Создаем копию в другом месте памяти
    std::vector<u8> new_buffer(binary.size() + 1024);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 512);
    std::memcpy(relocated, original, binary.size());
    
    // Устанавливаем base_offset на оригинал
    relocated->base_offset = original;
    
    lg::info("Original file: {}, Relocated file: {}", (void*)original, (void*)relocated);
    lg::info("Original definitions offset: {}, Relocated definitions offset before: {}", 
             original_definitions_offset, relocated->definitions.offset);
    
    // Вызываем релокацию
    relocated->relocate_pointers();
    
    lg::info("Relocated definitions offset after: {}", relocated->definitions.offset);
    
    // Проверяем, что указатели сдвинуты на правильную дельту
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(original);
    EXPECT_EQ(relocated->definitions.offset, original_definitions_offset + delta);
    EXPECT_EQ(relocated->base_offset, relocated);
}

TEST_F(RelocationTest, RelocateByteCodePointers) {
    BinaryFileBuilder builder;
    
    // Создаем функцию с данными
    std::vector<Instruction> code = {
        Instruction::create_abc(Opcode::ADD_INT, 0, 1, 2),
        Instruction::create_a(Opcode::RETURN, 0)
    };
    
    std::vector<u8> data = {0x01, 0x02, 0x03, 0x04};
    
    builder.add_function(SID("test_func"), code, data, {});
    
    std::vector<u8> binary = builder.build();
    BinaryFile* original = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Находим ByteCode
    Definition* def = original->get_definition(0);
    ByteCode* bc = reinterpret_cast<ByteCode*>(def->data_ptr.c());
    
    // Сохраняем исходные указатели
    u32 original_code_ptr = bc->code_ptr.offset;
    u32 original_data_ptr = bc->data_ptr.offset;
    
    lg::info("Original ByteCode: code_ptr={}, data_ptr={}", original_code_ptr, original_data_ptr);
    
    // Копируем в другое место
    std::vector<u8> new_buffer(binary.size() + 1024);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 256);
    std::memcpy(relocated, original, binary.size());
    
    // Обновляем указатель на ByteCode в relocated
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(original);
    relocated->definitions.offset += delta;
    
    for (u32 i = 0; i < relocated->definitions_count; i++) {
        Definition* def_rel = relocated->get_definition(i);
        def_rel->data_ptr.offset += delta;
    }
    
    relocated->base_offset = original;
    
    // Вызываем релокацию
    relocated->relocate_pointers();
    
    // Находим ByteCode в relocated
    Definition* def_rel = relocated->get_definition(0);
    ByteCode* bc_rel = reinterpret_cast<ByteCode*>(def_rel->data_ptr.c());
    
    lg::info("Relocated ByteCode: code_ptr={}, data_ptr={}", bc_rel->code_ptr.offset, bc_rel->data_ptr.offset);
    
    // Проверяем, что указатели сдвинуты правильно
    EXPECT_EQ(bc_rel->code_ptr.offset, original_code_ptr + delta);
    EXPECT_EQ(bc_rel->data_ptr.offset, original_data_ptr + delta);
}

TEST_F(RelocationTest, MultipleRelocations) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* file = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Первая релокация
    file->relocate_pointers();
    EXPECT_EQ(file->base_offset, file);
    
    u32 first_definitions_offset = file->definitions.offset;
    
    // Симулируем перемещение в памяти (копируем в новое место)
    std::vector<u8> new_buffer(binary.size() + 512);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 128);
    std::memcpy(relocated, file, binary.size());
    
    relocated->base_offset = file;
    
    // Вторая релокация
    relocated->relocate_pointers();
    
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(file);
    EXPECT_EQ(relocated->definitions.offset, first_definitions_offset + delta);
    EXPECT_EQ(relocated->base_offset, relocated);
}