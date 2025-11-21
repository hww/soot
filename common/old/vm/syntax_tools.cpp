

#include "syntax_tools.hpp"
#include "parser.hpp"

namespace vm::parser
{

	std::string GetArgException(const FListStx& list, const size_t argNum, const std::string& expected) {
		return std::format("{0} : {1} : Arg({2}) expected {3} found {4}\n", 
			list.loc.line, list.loc.pos,
			argNum, expected, list.get_item(argNum)->to_string());
	}
	void verify_min_arguments(const FListStx& list, const size_t minArgs) {
		const auto argc = list.size();
		if (argc < minArgs)
			throw FSyntaxError(list.loc, std::format("{0} : {1} : Expected min {2} found {3} in {4}\n",
				list.loc.line, list.loc.pos,
				minArgs, argc, list.to_string()));
	}
	void verify_max_arguments(const FListStx& list, const size_t maxArgs) {
		const auto argc = list.size();
		if (argc > maxArgs)
			throw FSyntaxError(list.loc, std::format("{0} : {1} : Expected maximum {2} found {3} in {4}\n",
				list.loc.line, list.loc.pos,
				maxArgs, argc, list.to_string()));
	}
	void verify_min_max_arguments(const FListStx& list, const size_t minArgs, const size_t maxArgs) {
		verify_min_arguments(list, minArgs);
		verify_max_arguments(list, maxArgs);
	}
	std::shared_ptr<FSymbolStx> get_symbol_stx(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_symbol_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "int"));
		return stx;
	}
	std::shared_ptr<FListStx> get_list_stx(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_list_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "int"));
		return stx;
	}

	s8 get_int8(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_number_stx();
		if (stx == nullptr)
			throw FSyntaxError(list.loc, GetArgException(list, argNum, "int"));
		const auto value = stx->get_int();
		if (value < std::numeric_limits<s8>::min() || value > std::numeric_limits<s8>::max())
			throw FSyntaxError(GetArgException(list, argNum, "s8 with range -128..127"));
		return static_cast<s8>(value);
	}

	s16 get_int16(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_number_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "int"));
		const auto value = stx->get_int();
		if (value < std::numeric_limits<s16>::min() || value > std::numeric_limits<s16>::max())
			throw FSyntaxError(GetArgException(list, argNum, "s16 with range -32768..32767"));
		return static_cast<s16>(value);
	}

	s32 get_int32(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_number_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "int"));
		return stx->get_int();
	}

	float get_float(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_number_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "float"));
		return stx->get_float();
	}

	StringId get_string_id(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_symbol_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "StringId"));
		define_string(stx->get_string());
		return stx->get_string_id();
	}

	std::string get_string(const FListStx& list, const size_t argNum) {
		const auto stx = list.get_item(argNum)->get_string_stx();
		if (stx == nullptr)
			throw FSyntaxError(GetArgException(list, argNum, "string"));
		return stx->get_string();
	}

	bool is_symbol_equal_to(const FListStx& list, const size_t argNum, StringId sid)
	{
		const auto stx = list.get_item(argNum)->get_symbol_stx();
		if (stx == nullptr)
			return false;
			// Check for the ":" column symbol
		const auto itemSid = stx->get_string_id();
		return itemSid == sid;
	}
}