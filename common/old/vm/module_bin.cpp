#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <exception>

#include "module_bin.hpp"
#include "variant.hpp"
#include "string_id.hpp"
namespace vm
{

	void FBinFile::load_file(std::ifstream& stream)
	{
		if (is_loaded)
			throw std::exception("Do not call it twice");
		if (!stream.is_open()) 
			throw std::exception("Do not call it twice");

		stream.seekg(0, std::ios::end);
		const auto fileSize = stream.tellg();
		stream.seekg(0, std::ios::beg);

		const auto buff = new char[fileSize];
		stream.read(buff, fileSize);
		file = reinterpret_cast<FBinFileHeader*>(buff);

		if (!file->is_valid_magic())
			throw FException("The file is invalid");
		is_loaded = true;
	}
	void FBinFile::save_file(std::ofstream& out) const
	{
		out.write(reinterpret_cast<char*>(file), safe_cast_to_int32(file->used_size));
	}


	void FDefinition::Define(const StringId name, const StringId type, u32  offset)
	{
		Name = name;
		Type = type;
		Offset = offset;
	}



}


