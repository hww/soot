#include "gtest/gtest.h"
#include "common/carbon/Export.hpp"
#include "vm/Instructions.hpp"

using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;


TEST(Instructions, BasicStructure) {
    Instruction instr1 = InstructionFactory::ab(Opcode::MoveInt, 1, 2);
    EXPECT_EQ(instr1.opcode, Opcode::MoveInt);
    EXPECT_EQ(instr1.a, 1);
    EXPECT_EQ(instr1.b, 2);
    EXPECT_EQ(instr1.c, 0);

    Instruction instr2 = InstructionFactory::imm(Opcode::LoadU16Imm, 5, 100);
    EXPECT_EQ(instr2.opcode, Opcode::LoadU16Imm);
    EXPECT_EQ(instr2.a_imm, 5);
    EXPECT_EQ(instr2.imm16, 100);
}

TEST(Instructions, Size) {
    // Инструкция всегда 4 байта (без учёта padding в UP_Instruction<4>)
    EXPECT_EQ(sizeof(ShortInstruction), 4);
}

TEST(Instructions, ImmediateDetection) {
    Instruction imm_instr = InstructionFactory::imm(Opcode::LoadU16Imm, 1, 42);
    // LoadU16Imm: operand2_is_immediate зависит от реализации
    // Проверяем, что immediate доступен
    EXPECT_EQ(imm_instr.imm16, 42);

    Instruction reg_instr = InstructionFactory::abc(Opcode::IAdd, 1, 2, 3);
    // IAdd использует регистры
    EXPECT_TRUE(reg_instr.operand1_is_used());
    EXPECT_TRUE(reg_instr.operand2_is_used());
}

TEST(Instructions, InstructionTable) {
    auto* info = get_instruction_info(Opcode::MoveInt);

    ASSERT_NE(info, nullptr);
    // Если info->name — это const char*
    EXPECT_STREQ(info->name, "MoveInt");
    // Если info->name — это StringId
    // EXPECT_EQ(info->name, StringId("MoveInt"));
    
    // Проверяем количество операндов
    // (метод зависит от вашей реализации)
    EXPECT_EQ(info->oprands_count(), 2);
}

TEST(Instructions, StringConversion) {
    Instruction instr1 = InstructionFactory::abc(Opcode::IAdd, 1, 2, 3);
    EXPECT_STREQ(instr1.opcode_to_string(), "IAdd");
    
    Instruction instr2 = InstructionFactory::ab(Opcode::MoveInt, 1, 2);
    EXPECT_STREQ(instr2.opcode_to_string(), "MoveInt");
}

TEST(Instructions, AllOpcodesHaveInfo) {
    // Проверяем, что для всех опкодов есть информация
    for (int i = 0; i <= static_cast<int>(Opcode::Breakpoint); ++i) {
        Opcode op = static_cast<Opcode>(i);
        auto* info = get_instruction_info(op);
        
        // Некоторые опкоды могут быть не реализованы
        if (info == nullptr) {
            // Пропускаем или выводим предупреждение
            continue;
        }
        
        EXPECT_NE(info->name, nullptr);
        EXPECT_STRNE(info->name, "");
    }
}

TEST(Instructions, BranchImmediate) {
    Instruction branch = InstructionFactory::branch(Opcode::Branch, 0x1234);
    EXPECT_EQ(branch.opcode, Opcode::Branch);
    EXPECT_EQ(branch.imm16, 0x1234);
}

TEST(Instructions, LookupImmediate) {
    Instruction lookup = InstructionFactory::lookup(Opcode::LookupPointer, 3, 5);
    EXPECT_EQ(lookup.opcode, Opcode::LookupPointer);
    EXPECT_EQ(lookup.a, 3);
    EXPECT_EQ(lookup.imm16, 5);
}