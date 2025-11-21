#pragma once

#include <vector>
#include "parser.hpp"
#include "string_id.hpp"

namespace vm
{
	/**
	 * @brief Load and parse dci files
	 *
	 * The format of file you can see below
	 * (file-name (bin-file-size)
	 *    (import ....)
	 *    (export ....)
	 *	  )
	 */
	class FModuleInfo
	{
	public:
		FModuleInfo() = default;

		void parse_import(const parser::FListStx& stx);
		void parse_export(const parser::FListStx& stx);
		void parse_list(const parser::FListStx& stx);
		void load_file(std::istream& in);
		void save_file(std::ostream& out) const;


		StringId name;
		u32 bin_size;
		std::vector<StringId> imports;
		std::vector<StringId> exports;
	};



}
