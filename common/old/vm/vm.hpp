#pragma once
#include <string>
#include <set>
#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "variant.hpp"
#include "module_bin.hpp"

namespace vm
{
	/**
	 * The bit operations are mostly unused in the game developing
	 * they can be omitted for the low footprint. So to omit the
	 * bit operations just comment the lin below
	 */
	#define USE_BIT_OPERATIONS

	/** @brief The default position for method arguments */
	constexpr size_t ARGUMENT_REGISTERS_OFFSET = 24;

	/** @brief The default position for locals */
	constexpr size_t LOCAL_REGISTERS_OFFSET = 0;

	/**
	 * The def_value size of the stack frame. It should be 32 - 40
	 * But can be much less for simple games
	 */
	constexpr size_t DC_FRAME_MAX_REGISTERS_NUM = 34;

	struct FStackFrame;


	/** The stack frame for the script engine */
	__declspec(align(32))
	struct FStackFrame
	{

		FStackFrame()
			: code_ptr(nullptr)
			, parent_ptr(nullptr)
			, pc()
			, argc()
		{

		}

		/** Read the next instruction by the program counter (PC) */
		FInstr GetNextInstruction();

		s32 get_argc() const
		{
			return argc;
		}

		FVariant& get_register(u32 index) {
			assert(index < DC_FRAME_MAX_REGISTERS_NUM);
			return registers[index];
		}

		s32 get_static_s32(u32 index) const
		{
			return data_ptr[index].as_int32;
		}

		u32 get_static_u32(u32 index) const
		{
			return data_ptr[index].as_uint32;
		}

		float get_static_f32(u32 index) const
		{
			return *(float*)&data_ptr[index];
		}

		PTRINT get_static_pointer(u32 index) const
		{
			return *(PTRINT*)&data_ptr[index];
		}

		std::string to_string() const;


		/** Byte code pointer */
		FInstr* code_ptr;
		/** Data pointer */
		FRecord* data_ptr;
		/** a pointer to the parent frame */
		FStackFrame* parent_ptr;
		/** Program counter */
		size_t pc;
		/** Arguments count */
		s32 argc;
		/** Return reg */
		s32 ret_num;
		/** The registers of the frame */
		FVariant registers[DC_FRAME_MAX_REGISTERS_NUM];
	};

	FStackFrame* ExecuteScript(const StringId name);

	inline void print_registers(const FVariant* regs, u32 len)
	{
		for (u32 i=0; i<len; i++)
		{
			if (regs[i].is_null())
				continue;
			std::cout << "[" << i << "]" << regs[i].to_c_string() << " ";
		}
		std::cout << std::endl;
	}
}


