#pragma once
#include <string>
#include "variant.hpp"
#include "parser.hpp"


namespace vm::parser
{
	std::string GetArgException(const FListStx& list, size_t argNum, const std::string& expected);

	//--------------------------------------------------------------------------
	// Get various types
	//--------------------------------------------------------------------------

	s8 get_int8(const FListStx& list, const size_t argNum);
	s16 get_int16(const FListStx& list, const size_t argNum);
	s32 get_int32(const FListStx& list, const size_t argNum);
	float get_float(const FListStx& list, const size_t argNum);
	StringId get_string_id(const FListStx& list, size_t argNum);
	std::string get_string(const FListStx& list, const size_t argNum);
	std::shared_ptr<FListStx> get_list_stx(const FListStx& list, const size_t argNum);
	std::shared_ptr<FSymbolStx> get_symbol_stx(const FListStx& list, const size_t argNum);

	//--------------------------------------------------------------------------
	// Predicates
	//--------------------------------------------------------------------------

	void verify_min_arguments(const FListStx& list, const size_t minArgs);
	void verify_max_arguments(const FListStx& list, const size_t maxArgs);
	void verify_min_max_arguments(const FListStx& list, const size_t minArgs, const size_t maxArgs = -1);
	bool is_symbol_equal_to(const FListStx& list, const size_t argNum, StringId sid);

}

