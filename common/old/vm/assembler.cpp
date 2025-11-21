#include <fstream>
#include "assembler.hpp"
#include "string_id.hpp"
#include "assembler_opcodes.hpp"
#include "syntax_tools.hpp"

namespace vm::assembler
{
	/**
	 * There are two ways to get register number: by number or by the name
	 */
	s8 GetRegNum(const FCodeBuilder& builder, const FListStx& list, const size_t argNum) {
		const auto argStx = list.get_item(argNum);
		if (argStx->is_number())
		{
			const auto stx = argStx->get_number_stx();
			if (stx == nullptr)
				throw FSyntaxError(list.loc, GetArgException(list, argNum, "int"));
			const auto regNum = stx->get_int();
			if (regNum < std::numeric_limits<s8>::min() || regNum > std::numeric_limits<s8>::max())
				throw FSyntaxError(GetArgException(list, argNum, "s8 with range -128..127"));
			return static_cast<s8>(regNum);

		}
		if (argStx->is_symbol())
		{
			const auto stx = argStx->get_symbol_stx();
			if (stx == nullptr)
				throw FSyntaxError(list.loc, GetArgException(list, argNum, "symbol"));
			const auto regName = stx->get_string_id();
			const auto regNum = builder.GetGegNumber(regName);
			if (regNum < 0)
				throw FSyntaxError(stx->loc,"The variable is not found", stx->to_string());
			if (regNum >static_cast<s32>(std::numeric_limits<s8>::max()))
				throw FSyntaxError(GetArgException(list, argNum, "s8 with range -128..127"));
			return static_cast<s8>(regNum);
		}
		throw FSyntaxError(GetArgException(list, argNum, "Expected number or register name, found"));
	}
	void CompileLineA(FCodeBuilder& builder, const FListStx& list, const EOpcode op) {
		verify_min_max_arguments(list, 2, 2);
		const auto a = GetRegNum(builder, list, 1);
		builder.Code.add(FInstr(op, a, 0, 0));
	}
	void CompileLineAB(FCodeBuilder& builder, const FListStx& list, const EOpcode op) {
		verify_min_max_arguments(list, 3, 3);
		const auto a = GetRegNum(builder, list, 1);
		const auto b = GetRegNum(builder, list, 2);
		builder.Code.add(FInstr(op, a, b, 0));
	}
	void CompileLineABC(FCodeBuilder& builder, const FListStx& list, const EOpcode op) {
		verify_min_max_arguments(list, 4, 4);
		const auto a = GetRegNum(builder, list, 1);
		const auto b = GetRegNum(builder, list, 2);
		const auto c = GetRegNum(builder, list, 3);
		builder.Code.add(FInstr(op, a, b, c));
	}
	void CompileLineBranch(FCodeBuilder& builder, const FListStx& list, const EOpcode op) {
		verify_min_max_arguments(list, 3, 3);
		const auto a = GetRegNum(builder, list, 1);
		const auto constant = list.get_item(2);
		if (constant->is_number()) {
			auto address = get_int16(list, 2);
			// The address should be divided by four
			address = address >> 2;
			builder.Code.add(FInstr(op, a, address));
		}
		else if (constant->is_symbol()) {
			const auto labelName = get_string_id(list, 2);
			if (!builder.Code.has_label(labelName))
				throw FSyntaxError(list.loc, "Label is not defined", lookup_string_safe(labelName));
			auto labelAddress = builder.Code.get_label(labelName);
			// The address should be divided by four
			labelAddress = labelAddress >> 2;
			if (labelAddress > std::numeric_limits<u16>::max())
				throw FSyntaxError(list.loc, "Label index is overflow", lookup_string_safe(labelName));
			builder.Code.add(FInstr(op, a, static_cast<u16>(labelAddress)));
		}
		else {
			throw FSyntaxError(GetArgException(list, 2, "string_id or int16"));
		}
	}
	void CompileLineLoadImm(FCodeBuilder& builder, const FListStx& list, const EOpcode op) {
		verify_min_max_arguments(list, 3, 3);
		const auto a = GetRegNum(builder, list, 1);
		const auto k = get_int16(list, 2);
		builder.Code.add(FInstr(op, a, k));
	}
	void CompileLineLoadStatic(FCodeBuilder& builder,const FListStx& list, const EOpcode opcode) {
		verify_min_max_arguments(list, 3, 3);
		const auto a = GetRegNum(builder, list, 1);
		const auto addressLarge = builder.Data.size();
		if (addressLarge > std::numeric_limits<u16>::max())
			throw FSyntaxError(list.loc, "Seems data table size is too large", list.to_string());
		const auto address = static_cast<u16>(addressLarge);
		FRecord record{};
		switch (opcode)
		{
		case EOpcode::LoadStaticInt:
			{
				record.as_int32[0] = get_int32(list, 2);
				builder.Data.add(record);
			}
			break;
		case EOpcode::LoadStaticFloat:
			{
				record.as_float[0] = get_float(list, 2);
				builder.Data.add(record);
			}
			break;
		case EOpcode::LoadStaticPointer:
			{
				const auto str = get_string(list, 2);
				const auto sid = define_string(str);
				builder.Data.add_string(sid, str);
			}
			break;
		case EOpcode::LookupInt:
		case EOpcode::LookupFloat:
		case EOpcode::LookupPointer:
		{
				record.as_int32[0] = get_string_id(list, 2);
				builder.Data.add(record);
		}
		break;
		default:
			throw FSyntaxError(list.loc, "Unexpected error", list.to_string());
		}
		builder.Code.add(FInstr(opcode, a, address));
	}
	void CompileLineABK(FCodeBuilder& builder, const FListStx& list, const EOpcode opcode) {
		verify_min_max_arguments(list, 4, 4);
		auto a = GetRegNum(builder, list, 1);
		auto b = GetRegNum(builder, list, 2);
		auto k = get_int32(list, 3);
		builder.Code.add(FInstr(opcode, a, b, k));
	}


	/**
	* @brief There are several variants to define arguments
	*		 ;; for typed value
	*        (variable-name : variable-type)
	*        ;; for optional value
	*        (variable-name default-value)
	*		  ;; for optional typed value
	*        (variable-name default-value : variable-type)
	*/
	void CompileSingleArgument(FCodeBuilder& builder, const FListStx& list, FArguments& argList)
	{
		verify_min_arguments(list, 2);
		verify_max_arguments(list, 4);
		// The variable name state
		auto varName = get_symbol_stx(list, 0);
		// Parse rest of elements but start search def_value value
		auto state = EArgParserState::DefaultValue;
		// After parsing will be defined next two pointers
		std::shared_ptr<FSyntax> defaultValue;
		std::shared_ptr<FSymbolStx> typeName = nullptr;

		for (size_t i=1; i< list.size(); i++)
		{
			const auto item = list.get_item(i);
			switch (state)
			{
			case EArgParserState::DefaultValue:
				{
					if (is_symbol_equal_to(list, i, SID(":")))
					{
						// Switch to the typename search
						state = EArgParserState::TypeName;
						continue;
					}
	
					// The def_value value
					defaultValue = item;
					state = EArgParserState::ColumnSymbol;
				}
				break;
			case EArgParserState::ColumnSymbol:
				if (is_symbol_equal_to(list, i, SID(":")))
				{
					// Switch to the typename search
					state = EArgParserState::TypeName;
					continue;
				}
				throw FSyntaxError(list.loc, "Expected ':' as type separator", list.to_string());
				break;
			case EArgParserState::TypeName:
				typeName = get_symbol_stx(list, i);
				state = EArgParserState::TypeName;
				break;
			}
		}
		if (typeName == nullptr && defaultValue == nullptr)
			throw FSyntaxError(list.loc, "Expected definition of (def_value value or type) or bought", list.to_string());
		argList.add(varName.get(), typeName.get(), defaultValue);
	}

	/**
	 * @brief There are several variants to define arguments
	 *        ;; for not typed value
	 *        (function-name variable-name)
	 *		  ;; for typed value
	 *        (function-name (variable-name : variable-type))
	 *		  ;; for optional typed or not value
	 *        (function-name &optional (variable-name default-value : variable-type)) 
	 *		  ;; for optional typed or not value
	 *        (function-name &rest (variable-name : variable-type))
	 */
	void CompileArguments(FCodeBuilder& builder, const FListStx& list, FArguments& argList)
	{
		int itemCnt = -1;
		const auto items = list.get_list();
		for (auto& t : items) {
			itemCnt++;
			switch (itemCnt)
			{
			case 0:
				// Skip the function name 
				break;
			default:
				// The arguments
				if (t->is_symbol()) {
					const auto varName = get_string_id(list, itemCnt);
					argList.add(t->get_symbol_stx().get(), nullptr,nullptr);
				} else if (t->is_list()) {
					const auto varStx = get_list_stx(list, itemCnt);
					CompileSingleArgument(builder, *varStx, argList);
				}
				break;
			}
		}
	}
	
	/**
	 * @brief Compile single code line
	 * @param builder - The assembly builder
	 * @param list - The expression list
	 */
	void CompileCodeLine(FCodeBuilder& builder, const FListStx& list)
	{
		verify_min_arguments(list, 1);

		switch (const auto opStringId = get_string_id(list, 0))
		{
		case SID("label"):
		{
			const auto sid = get_string_id(list, 1);
			if (builder.Code.has_label(sid))
				throw FSyntaxError(list.loc, "Label is already defined", list.to_string());
			builder.Code.add_label(sid);
		}
		break;
		case SID("let"):
		{
			CompileArguments(builder, list, builder.Locals);
		}
		break;
		default:
		{
			FOpcodeInfo opInfo{};
			const auto opStx = list.get_item(0);
			if (!get_opcode_info(opStringId, opInfo))
				throw FSyntaxError(opStx->loc, "Unexpected instruction", opStx->to_string());
			switch (opInfo.group) {
			case EOpcodeGroup::OperandA:
				CompileLineA(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::OperandsAB:
				CompileLineAB(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::OperandsABC:
				CompileLineABC(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::OperandsABK:
				CompileLineABK(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::LoadImm:
				CompileLineLoadImm(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::LoadStatic:
				CompileLineLoadStatic(builder, list, opInfo.opcode);
				break;
			case EOpcodeGroup::Branch:
				CompileLineBranch(builder, list, opInfo.opcode);
				break;
			}
		}
		}
	}
	/**
	 * @brief Compile the 'define' expression with the format below
	 *			(define name (args)
	 *			        (expression1)
	 *			        (expression2)
	 *					)
	 * @param builder - The assembly builder
	 * @param list - The definition's list
	 */
	void CompileDefine(FCodeBuilder& builder, const FListStx& list)
	{
		int itemCnt = -1;
		const auto items = list.get_list();
		for (auto& t : items) {
			switch (++itemCnt)
			{
			case 0:
				// Skip first item with 'define'
				continue;
			case 1:
				{
					if (t->is_symbol())
					{
						// The data definitions
						verify_min_arguments(list, 3);
						const auto name = get_string_id(list, 1);
						const auto valStx = list.get_item(2);
						if (valStx->is_symbol())
						{
							builder.Data.add_i32(name, get_string_id(list, 1));
						}
						else if (valStx->is_integer())
						{
							builder.Data.add_i32(name, get_int32(list, 1));
						}
						else if (valStx->is_float())
						{
							builder.Data.add_f32(name, get_float(list, 1));
						}
						else if (valStx->is_string())
						{
							builder.Data.add_string(name, get_string(list, 1));
						}
						else if (valStx->is_string())
						{
							throw FSyntaxError(list.loc, "Unimplemented structures", list.to_string());
						}
						else
							throw FSyntaxError(list.loc, "Unexpected definition value", list.to_string());


					}
					else if(t->is_list())
					{
						// the argument's list 
						const auto arguments = t->get_list_stx();
						if (arguments == nullptr)
							throw FSyntaxError(t->loc, "Expected list of arguments", t->to_string());
						builder.Name = get_string_id(*arguments, 0);
						CompileArguments(builder, *arguments, builder.Args);
					}
					else
						throw FSyntaxError(t->loc, "Expected symbol or list", t->to_string());
				}
				break;
			default:
				{
					// all next expressions must be as the lists 
					const auto expression = t->get_list_stx();
					if (expression == nullptr)
						throw FSyntaxError(t->loc, "Expected list for instruction line", list.to_string());
					// the body of the lambda
					CompileCodeLine(builder, *expression);
				}
			}
		}
	}


	void CompileTopEntry(FCodeBuilder& builder, const FListStx& stx)
	{
		const auto stringId = get_string_id(stx, 0);
		const auto front = stx.front();
		switch(stringId)
		{
		case SID("file"):
			throw FSyntaxError(stx.loc, "Please Implement File header", stx.to_string());
		case SID("define"):
			CompileDefine(builder, stx);
			break;
		case SID("lambda"):
			CompileDefine(builder, stx);
			break;
		default:
			throw FSyntaxError(stx.loc, "Unexpected expression", stx.to_string());
		}
	}

	void FArguments::add(
		const FSymbolStx* nameStx,
		const FSymbolStx* typeStx,
		const std::shared_ptr<FSyntax>& defaultValue)
	{
		auto name = nameStx->get_string_id();
		auto type = typeStx == nullptr ? UNDEFINED_ARG_TYPE : typeStx->get_string_id();

		switch (ArgState)
		{
		case EArgListParserState::Arguments:
			arg_min++;
			arg_max++;
			break;
		case EArgListParserState::Optional:
			arg_opts++;
			arg_max++;
			break;
		case EArgListParserState::Rest:
			arg_rest++;
			arg_max++;
			ArgState = EArgListParserState::Done;
			break;
		case EArgListParserState::Done:
			throw FSyntaxError();
		}

		Arguments.push_back({.name = name, .type = type, .def_value = defaultValue });
	}

	/**
	 * Parse the top level expression
	 */
	std::unique_ptr<FBinFile> Compile(std::istream& in)
	{
		init_opcode_table();
			const auto stx = parse_stream(in);
#ifdef DEBUG_PRINT_PARSED_SCRIPT
			stx->write_indented(std::cout,4);
			std::cout << std::endl;
#endif
			const auto topList = stx->get_list_stx();
			if (topList == nullptr)
				throw FSyntaxError(stx->loc, "Expected the list as top level object", stx->to_string());
			// The list of code blocks will be stored in the next vector
			std::vector<FCodeBuilder> codeList;
			// For each element of the top level
			for (auto& t : *topList) {
				// Get child list. The top level contains only lists
				const auto secondList = t->get_list_stx();
				if (secondList == nullptr) 
					throw FSyntaxError(t->loc, "Expected the list found", t->to_string());
				FCodeBuilder builder;
				CompileTopEntry(builder, *secondList);
				codeList.push_back(builder);
			}
			// Collect the sizes
			size_t codeAndDataSize = 0;
			for (auto& asmCode : codeList)
			{
				codeAndDataSize += asmCode.GetRequiredSize();
			}
			auto defNum = safe_cast_u_int32(codeList.size());
			// Make a bin file. Start with formatting the file to required size
			auto binFile = std::make_unique<FBinFile>();
			binFile->initialize(defNum, codeAndDataSize);
			for (auto& asmCode : codeList)
			{
#ifdef DEBUG_PRINT_ASSEMBLED_LAMBDA
				std::cout << ToString(asmCode.Name) << std::endl;
#endif
				// Make definition but do not copy data for it
				const auto byteCode = binFile->define<FByteCode>(asmCode.Name, SID("lambda"));
				binFile->get_file_header()->initialize(
					byteCode,
					asmCode.Code.Data,
					asmCode.Data.Data);
#ifdef DEBUG_PRINT_ASSEMBLED_LAMBDA
				decompile_bytecode(byteCode);
#endif
			}
			return  std::move(binFile);
	}
}
