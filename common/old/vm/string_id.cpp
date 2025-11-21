#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "string_id.hpp"

#include <algorithm>
#include <cassert>

#include "crc32.hpp"
#include "errors.hpp"

namespace vm
{
	/**
	 * \brief The list of all available strings
	 */
	static std::map<StringId, std::string> g_strings_database;
	/**
	 * \brief The number used for generating random unique symbols
	 */
	static u32 g_gen_sym_num = 0;

	StringId define_string(const std::string& str) {
		assert(!str.empty());
		const StringId sid = crc32((const char*)str.c_str(), str.size());
		const auto item = g_strings_database.find(sid);
		if (item == g_strings_database.end()) {
			g_strings_database[sid] = str;
		}
		else {
			if (str.compare(item->second))
				throw FStringCollisionException(str, item->second);
		}
		return sid;
	}


	const std::string NULL_STRING;

	const std::string& lookup_string(const StringId sid) 
	{
		const auto item = g_strings_database.find(sid);
		return item == g_strings_database.end() ? NULL_STRING : item->second;
	}

	std::string lookup_string_safe(const StringId sid)
	{
		const auto item = g_strings_database.find(sid);
		return item == g_strings_database.end() ? std::format("StingId {:X}", sid) : item->second;
	}

	std::string to_str(const StringId obj)
	{
		return lookup_string_safe(obj);
	}

	void clear_strings()
	{
		g_strings_database.clear();
	}

	void load_strings_file(std::istream& in, std::string path)
	{
		size_t lineNum = 0;
		std::string line;

		while (std::getline(in, line))
		{
			auto idx = line.find_first_of(' ');
			auto key = line.substr(0, idx);
			auto val = line.substr(idx + 1);
			auto sid = define_string(val);
			StringId string_id_from_file = std::stoul(key, nullptr, 16);
			if (sid != string_id_from_file)
				throw std::exception(std::format("File: {0} The file {1:X} {2:X} {3}\n", path, string_id_from_file, sid, val).c_str());
			lineNum++;
		}
	}

	void load_strings_file(const std::string& path)
	{
		std::ifstream is(path);
		if (is)
		{
			load_strings_file(is, path);
			is.close();
		}
	}
	struct FStringPair { StringId id; std::string* str; };

	static void sort_database(std::vector<FStringPair>& list)
	{
		for (auto& t : g_strings_database)
			list.push_back(FStringPair{ .id = t.first, .str = &t.second });

		std::sort(list.begin(), list.end(),
			[](const auto& a, const auto& b) { return *a.str < *b.str; });
	}

	void save_strings_file(const std::string& path)
	{
		std::ofstream os(path);
		if (os)
		{
			std::vector<FStringPair> str_pair;
			sort_database(str_pair);
			for (auto& t : str_pair)
			{
				os << std::format("{0:08X} {1}\n",t.id, *t.str);
			}
			os.close();
		}
	}

	void print_strings() {
		std::vector<FStringPair> str_pair;
		sort_database(str_pair);
		for (auto& item : str_pair) {
			std::cout << std::format("{0:08X} {1}\n", item.id, *item.str);
		}
	}

	StringId get_gen_sym() {
		const auto genName = std::format("gensym-{0}", g_gen_sym_num);
		const auto sym = crc32(genName);
		g_gen_sym_num++;
		return sym;
	}
}