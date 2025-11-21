#include <map>
#include "assembler_opcodes.hpp"

#include <iostream>

#include "string_id.hpp"

namespace vm::assembler
{
	bool g_is_opcode_table_initialized = false;
	std::map<StringId, FOpcodeInfo> gNameToInfoTable;
	std::map<EOpcode, FOpcodeInfo> gOpcodeToInfoTable;

	void DefineOpcodeName(const char* name, EOpcode op, EOpcodeGroup type)
	{
		FOpcodeInfo info{};
		info.opcode = op;
		info.name = define_string(name);
		info.group = type;
		gNameToInfoTable[info.name] = info;
		gOpcodeToInfoTable[op] = info;
	}

	bool get_opcode_info(const EOpcode op, FOpcodeInfo& info)
	{
		if (const auto res = gOpcodeToInfoTable.find(op); res != gOpcodeToInfoTable.end())
		{
			info = res->second;
			return true;
		}
		return false;
	}

	bool get_opcode_info(const StringId sid, FOpcodeInfo& info)
	{
		if (const auto res = gNameToInfoTable.find(sid); res != gNameToInfoTable.end())
		{
			info = res->second;
			return true;
		}
		return false;
	}

	void init_opcode_table()
	{
		if (g_is_opcode_table_initialized)
			return;
		/** The program flow */
		DefineOpcodeName("ret", EOpcode::Return, EOpcodeGroup::OperandA);
		DefineOpcodeName("move", EOpcode::Move, EOpcodeGroup::OperandsAB);
		DefineOpcodeName("call", EOpcode::Call, EOpcodeGroup::OperandsABK);
		DefineOpcodeName("calln", EOpcode::CallNat, EOpcodeGroup::OperandsABK);
		DefineOpcodeName("br", EOpcode::Branch, EOpcodeGroup::Branch);
		DefineOpcodeName("brif", EOpcode::BranchIf, EOpcodeGroup::Branch);
		DefineOpcodeName("brno", EOpcode::BranchIfNot, EOpcodeGroup::Branch);
		/** Integer */
		DefineOpcodeName("add", EOpcode::AddInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("sub", EOpcode::SubInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("mul", EOpcode::MulInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("div", EOpcode::DivInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("mod", EOpcode::ModInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("abs", EOpcode::AbsInt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("neg", EOpcode::NegInt, EOpcodeGroup::OperandA);
                DefineOpcodeName("ash", EOpcode::AshInt, EOpcodeGroup::OperandsAB);
                DefineOpcodeName("ldk", EOpcode::LoadImediateInt, EOpcodeGroup::LoadImm);
		DefineOpcodeName("addk", EOpcode::AddImm, EOpcodeGroup::OperandsABK);
		DefineOpcodeName("subk", EOpcode::SubImm, EOpcodeGroup::OperandsABK);
		DefineOpcodeName("mulk", EOpcode::MulImm, EOpcodeGroup::OperandsABK);
		DefineOpcodeName("divk", EOpcode::DivImm, EOpcodeGroup::OperandsABK);
		/** Floting point */
		DefineOpcodeName("fadd", EOpcode::AddFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fsub", EOpcode::SubFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fmul", EOpcode::MulFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fdiv", EOpcode::DivFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fmod", EOpcode::ModFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fabs", EOpcode::AbsFloat, EOpcodeGroup::OperandA);
		DefineOpcodeName("fneg", EOpcode::NegFloat, EOpcodeGroup::OperandA);
		/** Type Casting */
		DefineOpcodeName("tof", EOpcode::ToFloat, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("toi", EOpcode::ToInt, EOpcodeGroup::OperandsAB);
		/** Comparisong */
		DefineOpcodeName("cpeq", EOpcode::CmpEqual, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("cpgt", EOpcode::CmpGt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("cpge", EOpcode::CmpGtEqual, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("cplt", EOpcode::CmpLt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("cple", EOpcode::CmpLtEqual, EOpcodeGroup::OperandsABC);
		/** Comparisong floating point */
		DefineOpcodeName("fcpeq", EOpcode::CmpFloatEqual, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fcpgt", EOpcode::CmpFloatGt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fcpge", EOpcode::CmpFloatGtEqual, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fcplt", EOpcode::CmpFloatLt, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("fcple", EOpcode::CmpFloatLtEqual, EOpcodeGroup::OperandsABC);
		/** Logic oprations */
		DefineOpcodeName("and", EOpcode::LogAnd, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("or", EOpcode::LogOr, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("not", EOpcode::LogNot, EOpcodeGroup::OperandsABC);
		/** Bit oprations */
		DefineOpcodeName("band", EOpcode::BitAnd, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("bnot", EOpcode::BitNot, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("bor", EOpcode::BitOr, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("bxor", EOpcode::BitXor, EOpcodeGroup::OperandsABC);
		DefineOpcodeName("bnor", EOpcode::BitNor, EOpcodeGroup::OperandsABC);
		/** Utilities */
		DefineOpcodeName("argc", EOpcode::LocadArgc, EOpcodeGroup::OperandA);
		DefineOpcodeName("gsid", EOpcode::GetSidStr, EOpcodeGroup::OperandsAB);
		/** Find object */
		DefineOpcodeName("looki", EOpcode::LookupPointer, EOpcodeGroup::LoadStatic);
		DefineOpcodeName("lookf", EOpcode::LookupPointer, EOpcodeGroup::LoadStatic);
		DefineOpcodeName("lookp", EOpcode::LookupPointer, EOpcodeGroup::LoadStatic);
		/** Indirrect access */
		DefineOpcodeName("ldri", EOpcode::LoadIndInt, EOpcodeGroup::OperandA);
		DefineOpcodeName("ldrf", EOpcode::LoadIndFloat, EOpcodeGroup::OperandA);
		DefineOpcodeName("ldrp", EOpcode::LoadIndPointer, EOpcodeGroup::OperandA);
		DefineOpcodeName("stri", EOpcode::StoreIndInt, EOpcodeGroup::OperandsAB);
		DefineOpcodeName("strf", EOpcode::StoreIndFloat, EOpcodeGroup::OperandsAB);
		DefineOpcodeName("strp", EOpcode::StoreIndPointer, EOpcodeGroup::OperandsAB);
		/** Constants */
		DefineOpcodeName("ldsi", EOpcode::LoadStaticInt, EOpcodeGroup::LoadStatic);
		DefineOpcodeName("ldsf", EOpcode::LoadStaticFloat, EOpcodeGroup::LoadStatic);
		DefineOpcodeName("ldsp", EOpcode::LoadStaticPointer, EOpcodeGroup::LoadStatic);
		g_is_opcode_table_initialized = true;
	}

	std::string decompile_load_static(FInstr inst, FRecord* data = nullptr)
	{
		if (!g_is_opcode_table_initialized)
			init_opcode_table();
		auto opcode = inst.opcode;

		auto& info = gOpcodeToInfoTable[opcode];
		FVariant value;
		const auto imm = inst.k;
		if (data == nullptr)
			return std::format("{0:08X}  {1} {2},data[#{3}]", inst.as_uint32, to_str(info.name), inst.a, inst.k);


		switch (opcode)
		{
		case EOpcode::LoadStaticInt:
			value = ((s32*)data)[imm];
			break;
		case EOpcode::LoadStaticFloat:
			value = ((float*)data)[imm];
			break;
		case EOpcode::LoadIndPointer:
			value = (PTRINT)(char*)&data[imm];
			break;
		case EOpcode::LookupInt:
		case EOpcode::LookupFloat:
		case EOpcode::LookupPointer:
			// Load StringId of the lockup method
			value = ((s32*)data)[imm];
		}

		if (value.is_null())
			return std::format("{0:08X}  {1:12} {2},data[#{3}]", inst.as_uint32, to_str(info.name), inst.a, inst.k);

		if (value.is_i32())
		{
			const auto str = lookup_string(value.get_as_s32());
			return std::format("{0:08X}  {1:12} {2},data[#{3}] ; {3}", inst.as_uint32, to_str(info.name), inst.a,
			                   inst.k, str);
		}

		return std::format("{0:08X}  {1:12} {2},data[#{3}] ; {4}", inst.as_uint32, to_str(info.name), inst.a, inst.k,
		                   value.to_c_string());
	}

	std::string decompile_instruction(FInstr inst, FRecord* data)
	{
		if (!g_is_opcode_table_initialized)
			init_opcode_table();
		auto opcode = inst.opcode;
		auto& info = gOpcodeToInfoTable[opcode];
		switch (info.group)
		{
		case EOpcodeGroup::OperandA:
			return std::format("{0:08X}  {1:12} {2}", inst.as_uint32, to_str(info.name), inst.a);
		case EOpcodeGroup::OperandsAB:
			return std::format("{0:08X}  {1:12} {2},{3}", inst.as_uint32, to_str(info.name), inst.a, inst.b);
		case EOpcodeGroup::OperandsABC:
			return std::format("{0:08X}  {1:12} {2},{3},{4}", inst.as_uint32, to_str(info.name), inst.a, inst.b,
			                   inst.c);
		case EOpcodeGroup::OperandsABK:
			return std::format("{0:08X}  {1:12} {2},{3},#{4}", inst.as_uint32, to_str(info.name), inst.a, inst.b,
			                   inst.c);
		case EOpcodeGroup::LoadImm:
			return std::format("{0:08X}  {1:12} {2},#{3}", inst.as_uint32, to_str(info.name), inst.a, inst.k);
		case EOpcodeGroup::LoadStatic:
			return decompile_load_static(inst, data);
		case EOpcodeGroup::Branch:
			return std::format("{0:08X}  {1:12} {2},{3},{4}", inst.as_uint32, to_str(info.name), inst.a, inst.b,
			                   inst.c);
		default:
			return "undefined disassemble code";
		}
	}

	void decompile_bytecode(FByteCode* code)
	{
		if (!g_is_opcode_table_initialized)
			init_opcode_table();
		const auto code_ptr = code->get_code_ptr();
		const auto data_ptr = code->get_data_ptr();
		const auto code_size = code->get_code_size();
		std::cout << std::format("{0:08X} : code[0]\n", (u32)(PTRINT)code_ptr, code_size);
		for (u32 i = 0; i < code_size; i++)
		{
			std::cout << std::format("{0:08X} : {1}", code->file_offs + sizeof(FInstr) * i,
			                         decompile_instruction(code_ptr[i], data_ptr));
			std::cout << std::endl;
		}

		const auto data_size = code->get_data_size();
		std::cout << std::format("{0:08X} : data[0]\n", (u32)(PTRINT)data_ptr, data_size);
		for (u32 i = 0; i < data_size; i++)
		{
			const auto val = data_ptr[i];
			const u32 address = code->data_offs + sizeof(FRecord) * i;
			std::cout << std::format("{0:08X} : {1:08X} {2:08X}",
			                         address, val.as_int32[0], val.as_int32[1]);
			std::cout << std::endl;
		}
	}
}
