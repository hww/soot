#pragma once

#include <vector>
#include <string>
#include <map>

#include "parser.hpp"
#include "module_bin.hpp"
#include "errors.hpp"
#include "vm.hpp"

using namespace vm::parser;

namespace vm::assembler {

	template<class T>
	struct TAsmBuilder
	{
		void add(T data) {
			Data.push_back(data);
		}

		/**
		 * Add the label for current counter position
		 */
		u16 add_label(StringId sid) {
			const size_t address = sizeof(T) * Data.size();
			if (address > std::numeric_limits<s16>::max())
				throw std::exception("Too large address for the label");
			Labels[sid] = address;
			return static_cast<u16>(address);
		}

		u16 add_i32(const StringId name, const s32 data)
		{
			const auto address = add_label(name);
			FRecord record{};
			record.as_int32[0] = data;
			Data.push_back(record);
			return address;
		}
		u16 add_f32(const StringId name, const float data)
		{
			const auto address = add_label(name);
			FRecord record{};
			record.as_float[0] = data;
			Data.push_back(record);
			return address;
		}
		u16 add_string(const StringId name, const std::string& str)
		{
			const auto address = add_label(name);
			FRecord data{};
			size_t i = 0;
			for (;i<str.size();i++)
			{
				data.as_uint64 = (data.as_uint64 << sizeof(char)) | str[i];
				if (i % (sizeof(FRecord)) == 0)
				{
					Data.push_back(data);
					data.as_uint64 = 0;
				}
			}
			if (i% sizeof(FRecord) != 0)
				Data.push_back(data);

			return address;
		}

		/**
		 * Get the label for given SID
		 */
		size_t get_label(StringId sid) const {
			return Labels.at(sid);
		}
		bool has_label(StringId sid) const
		{
			return Labels.contains(sid);
		}
		T Get(u32 idx) const { return Data[idx]; }
		/** Get the size of data in the structure */
		size_t size() const
		{
			return Data.size();
		}
		/** The data container */
		std::vector<T> Data;
		/** The list of labels and their offsets from begin */
		std::map<StringId, size_t> Labels;
	};

	/**
	 * @brief Single argument info. Can include name type and default value
	 */
	struct FArgument
	{
		StringId name;
		StringId type;
		/** \brief The syntax object with default value*/
		std::shared_ptr<FSyntax> def_value;
	};

	enum class EArgParserState { DefaultValue, ColumnSymbol, TypeName };
	enum class EArgListParserState { Arguments, Optional, Rest, Done };

	struct FArguments
	{
		static constexpr StringId UNDEFINED_ARG_TYPE = SID("nil");

		std::vector<FArgument> Arguments;
		u32 arg_min;
		u32 arg_max;
		u32 arg_opts;
		u32 arg_rest;
		EArgListParserState ArgState;

		/**
		 * @brief Add new argument to the list
		 * @param nameStx - syntax with argument name
		 * @param typeStx - syntax with argument type or null for undefined
		 * @param defaultValue - syntax with argument default value or null for undefined
		 */
		void add(
			const FSymbolStx* nameStx,
			const FSymbolStx* typeStx,
			const std::shared_ptr<FSyntax>& defaultValue);

		size_t size() const { return Arguments.size(); }

		s32 get_index_of(StringId name) const
		{
			assert(Arguments.size() < static_cast<size_t>(std::numeric_limits<s32>::min()));
			for (u32 i=0; i<Arguments.size(); i++)
			{
				if (Arguments[i].name == name)
					return static_cast<s32>(i);
			}
			return INDEX_NONE;
		}
	};

	
	struct FCodeBuilder {



		StringId Name;
		TAsmBuilder<FInstr> Code;
		TAsmBuilder<FRecord> Data;
		FArguments Args;
		FArguments Locals;

		size_t GetRequiredFrameSize() const
		{
			return Args.size() + Locals.size();
		}

		size_t GetRequiredSize() const
		{
			return ALIGN_SIZE(sizeof(FByteCode) + sizeof(FInstr) * Code.size() + sizeof(FRecord) * Data.size());
		}

		s32 GetGegNumber(StringId name) const
		{
			const auto arg = Args.get_index_of(name);
			if (arg >= 0)
				return arg + ARGUMENT_REGISTERS_OFFSET;
			const auto loc = Locals.get_index_of(name);
			if (loc >= 0)
				return loc + LOCAL_REGISTERS_OFFSET;
			return INDEX_NONE;
		}
	};
	std::unique_ptr<FBinFile> Compile(std::istream& in);
}
