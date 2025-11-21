#pragma once
#include "module_bin.hpp"

namespace vm::assembler {

	/** The opcode group separated by operands */
	enum class EOpcodeGroup {
		/** Single operand */
		OperandA,
		/** Two operands */
		OperandsAB,
		/** Three operands */
		OperandsABC,	
		/** Two operands and one immediate */
		OperandsABK,
		/** One operand and large immediate */
		LoadImm,
		/** One operand and loading data[immediate] */
		LoadStatic,
		/** Branch with one operand and immediate address */
		Branch			
	};

	struct FOpcodeInfo {
		/** The opcode */
		EOpcode opcode;
		/** The opcode group separated by operands */
		EOpcodeGroup group;
		/** The opcode name for the assembly code */
		StringId name;
	};

	/**
	 * @brief Find the op-code info by the op-code
	 * @param op - the op-code
	 * @param info - return value as op-code info
	 * @return - return TRUE when success
	 */
	bool get_opcode_info(EOpcode op, FOpcodeInfo& info);
	/**
	 * @brief Find the opcode info by the op-code
	 * @param sid - the opcode name
	 * @param info - return value as opcode info
	 * @return - return TRUE when success
	 */
	bool get_opcode_info(StringId sid, FOpcodeInfo& info);
	/**
	 * @brief Initialize the op-code table
	 */
	void init_opcode_table();
	/**
	 * @brief Convert the instruction to string
	 * @param inst - instruction
	 * @param data - data segment
	 * @return the disassemble text
	*/
	std::string decompile_instruction(FInstr inst, FRecord* data = nullptr);

	/**
	 * \brief Disassemble code 
	 * \param code - the pointer to code
	 */
	void decompile_bytecode(FByteCode* code);
}