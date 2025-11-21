#include "module_info.hpp"

#include <filesystem>
#include <fstream>

#include "parser.hpp"
#include "platform.hpp"
#include "syntax_tools.hpp"

namespace vm
{
	void FModuleInfo::parse_import(const parser::FListStx& stx)
	{
		int itemCnt = -1;
		const auto items = stx.get_list();
		for (auto& t : items) {
			switch (++itemCnt)
			{
			case 0:
				// Skip first item with 'import'
				continue;
			default:
			{
				// all next expressions must be as the lists 
				const auto expression = t->get_symbol_stx();
				if (expression == nullptr)
					throw FSyntaxError(t->loc, "Expected name of definition", t->to_string());
				// the body of the lambda
				auto id = define_string(expression->get_string());
				imports.push_back(id);
			}
			}
		}
	}
	void FModuleInfo::parse_export(const parser::FListStx& stx)
	{
		int itemCnt = -1;
		const auto items = stx.get_list();
		for (auto& t : items) {
			switch (++itemCnt)
			{
				case 0:
					// Skip first item with 'export'
					continue;
				default:
				{
					// all next expressions must be as the lists 
					const auto expression = t->get_symbol_stx();
					if (expression == nullptr)
						throw FSyntaxError(t->loc, "Expected name of definition", t->to_string());
					// the body of the lambda
					auto id = define_string(expression->get_string());
					exports.push_back(id);
				}
			}
		}
	}
	void FModuleInfo::parse_list(const parser::FListStx& stx)
	{
		const auto stringId = get_string_id(stx, 0);
		const auto front = stx.front();
		switch (stringId)
		{
		case SID("export"):
			parse_export(stx);
			break;
		case SID("import"):
			parse_import(stx);
			break;
		default:
			throw FSyntaxError(stx.loc, "Unexpected expression", stx.to_string());
		}
	}
	void FModuleInfo::load_file(std::istream& in)
	{
		auto stx = parser::parse_stream(in);
		const auto topList = stx->get_list_stx();
		if (topList == nullptr)
			throw FSyntaxError(stx->loc, "Expected the list as top level object", stx->to_string());
		auto lineNum = -1;
		// For each element of the top level
		for (auto& t : *topList)
		{
			lineNum++;
			switch (lineNum)
			{
			case 0: // file name
				{
					name = get_string_id(*topList, 0);
				}
				break;
			case 1: // some id or tag?
				{
					const auto secondList = t->get_list_stx();
					bin_size = get_int32(*secondList, 0);
				}
				break;
			default: // export or import
				{
					// Get child list. The top level contains only lists
					const auto secondList = t->get_list_stx();
					parse_list(*secondList);
				}
				break;
			}
		}

	}

	void FModuleInfo::save_file(std::ostream& out) const
	{
		throw FException("Method is undefined");
	}



}
