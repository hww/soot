// test_relocation.cpp
#include "gtest/gtest.h"
#include "carbon/Export.hpp"
#include "files/RelocatableBuffer.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/Variant.hpp"

using namespace carbon::vm;
using namespace carbon::lib;
using namespace carbon::files;
using namespace carbon::modules;

class RelocationTest : public ::testing::Test {
protected:
    void SetUp() override {

    }
    
    // Создаем тестовый бинарник с известной структурой
    std::vector<u8> create_test_binary() {
        BinaryFileBuilder builder("module1");
        
        // Добавляем тестовую функцию
        std::vector<Instruction> code = {
            Instruction::create_a(Opcode::RETURN, 0)
        };
        
        RelocatableBuffer rbuffer;
        rbuffer.add_function(code, {}, {});
        builder.add_definition("test_func", "function", rbuffer);
        
        // Получаем бинарник
        std::vector<u8> binary = builder.build();
        
        lg::info("Created test binary of size: {}", binary.size());
        return binary;
    }
};

TEST_F(RelocationTest, BasicRelocation) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* file = reinterpret_cast<BinaryFile*>(binary.data());
    
    file->relocate_pointers(true, nullptr);

    // Проверяем, что base_offset инициализирован
    lg::info("base_offset before relocation: {}", (void*)file->get_base_offset());
    
    // После создания base_offset должен указывать на себя
    EXPECT_EQ(file->get_base_offset(), file);
    
    lg::info("Magic: 0x{:08X}, expected: 0x{:08X}", file->magic, BinaryFile::MAGIC);
    EXPECT_EQ(file->magic, BinaryFile::MAGIC);
}


TEST_F(RelocationTest, RelocatePointersWithDelta) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* original = reinterpret_cast<BinaryFile*>(binary.data());
    
    // Сохраняем исходные значения
    u64 original_definitions_offset = original->definitions.offset;
    
    // Создаем копию в другом месте памяти
    std::vector<u8> new_buffer(binary.size() + 1024);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 512);
    std::memcpy(relocated, original, binary.size());
    
    lg::info("Original file: {}, Relocated file: {}", (void*)original, (void*)relocated);
    lg::info("Original definitions offset: {}, Relocated definitions offset before: {}", 
             original_definitions_offset, relocated->definitions.offset);
    
    // Вызываем релокацию
    relocated->relocate_pointers(true, nullptr);
    
    lg::info("Relocated definitions offset after: {}", relocated->definitions.offset);
    
    // Проверяем, что указатели сдвинуты на правильную дельту
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(original);
    EXPECT_EQ(relocated->definitions.offset, original_definitions_offset + delta);
    EXPECT_EQ(relocated->base_offset, relocated);
}

TEST_F(RelocationTest, RelocateFunctionDescPointers) {
    BinaryFileBuilder builder("module1");
    
    // Создаем функцию с данными
    std::vector<Instruction> code = {
        Instruction::create_abc(Opcode::ADD_INT, 0, 1, 2),
        Instruction::create_a(Opcode::RETURN, 0)
    };
    
    std::vector<u8> data = {0x01, 0x02, 0x03, 0x04};
    RelocatableBuffer rbuffer;
    rbuffer.add_function(code, data, {});
    builder.add_definition("test_func", "function", rbuffer);
    
    std::vector<u8> binary = builder.build();
    BinaryFile* original = reinterpret_cast<BinaryFile*>(binary.data());
    fmt::print("original file\n{}", original->inspect().c_str());

    // Находим FunctionDesc
    Definition* def = original->get_definition(0);
    FunctionDesc* bc = reinterpret_cast<FunctionDesc*>(def->data.get());
    
    // Сохраняем исходные указатели
    u64 original_code_ptr = bc->code_ptr.offset;
    u64 original_data_ptr = bc->data_ptr.offset;
    
    lg::info("Original FunctionDesc: code_ptr={}, data_ptr={}", original_code_ptr, original_data_ptr);
    
    // Копируем в другое место
    std::vector<u8> new_buffer(binary.size() + 1024);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 256);
    std::memcpy(relocated, original, binary.size());
    
    // Просто вызываем relocate_pointers — он сам всё обновит
    relocated->relocate_pointers(true, nullptr);
    
    // Проверяем
    Definition* def_rel = relocated->get_definition(0);
    FunctionDesc* bc_rel = reinterpret_cast<FunctionDesc*>(def_rel->data.get());
    
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(original);
    EXPECT_EQ(bc_rel->code_ptr.offset, original_code_ptr + delta);
    EXPECT_EQ(bc_rel->data_ptr.offset, original_data_ptr + delta);
}

TEST_F(RelocationTest, MultipleRelocations) {
    std::vector<u8> binary = create_test_binary();
    BinaryFile* file = reinterpret_cast<BinaryFile*>(binary.data());
    fmt::print("source file\n{}", file->inspect().c_str());
    
    // Первая релокация
    file->relocate_pointers(true, nullptr);
    EXPECT_EQ(file->get_base_offset(), file);
    
    // Сохраняем как uintptr_t, а не u32
    uintptr_t first_definitions_ptr = reinterpret_cast<uintptr_t>(file->definitions.ptr);
    
    // Симулируем перемещение
    std::vector<u8> new_buffer(binary.size() + 512);
    BinaryFile* relocated = reinterpret_cast<BinaryFile*>(new_buffer.data() + 128);
    std::memcpy(relocated, file, binary.size());
    
    // Вторая релокация
    relocated->relocate_pointers(true, nullptr);
    fmt::print("dst file\n{}", relocated->inspect().c_str());
    
    ptrdiff_t delta = reinterpret_cast<u8*>(relocated) - reinterpret_cast<u8*>(file);
    
    // Сравниваем указатели
    uintptr_t expected = first_definitions_ptr + delta;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(relocated->definitions.ptr), expected);
    EXPECT_EQ(relocated->get_base_offset(), relocated);
}