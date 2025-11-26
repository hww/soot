#include "gtest/gtest.h"
#include "runtime/Export.hpp"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;


TEST(Instructions, BasicStructure) {
    Instruction instr1 = Instruction::create_ab(Opcode::MOVE, 1, 2);
    EXPECT_EQ(instr1.opcode, Opcode::MOVE);
    EXPECT_EQ(instr1.a, 1);
    EXPECT_EQ(instr1.b, 2);
    EXPECT_EQ(instr1.c, 0);

    Instruction instr2 = Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 5, 100);
    EXPECT_EQ(instr2.opcode, Opcode::LOAD_IMMEDIATE_INT);
    EXPECT_EQ(instr2.a_imm, 5);
    EXPECT_EQ(instr2.imm16, 100);
}

TEST(Instructions, Size) {
    EXPECT_EQ(sizeof(Instruction), 4);
}

TEST(Instructions, ImmediateDetection) {
    Instruction imm_instr = Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42);
    EXPECT_TRUE(imm_instr.has_immediate());

    Instruction reg_instr = Instruction::create_abc(Opcode::ADD_INT, 1, 2, 3);
    EXPECT_FALSE(reg_instr.has_immediate());
}

TEST(Instructions, InstructionTable) {
    auto& table = InstructionTable::instance();
    auto info = table.get_info(Opcode::MOVE);

    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, SID("move"));
    EXPECT_EQ(info->operand_count, 2);
}

TEST(Instructions, StringConversion) {
    Instruction instr = Instruction::create_abc(Opcode::ADD_INT, 1, 2, 3);
    EXPECT_FALSE(instr.to_string().empty());

    auto name = opcode_to_string(Opcode::MOVE);
    EXPECT_EQ(name, "move");
}