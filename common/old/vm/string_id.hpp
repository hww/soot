#pragma once

#include <string>
#include "crc32.hpp"
/**
 * Making StringId from postfix to c string
 * Example: "hello world"_id
 */
constexpr unsigned int operator"" _sid(const char* v, size_t C) {
  return vm::const_crc32(v, C);
}

/**
 * Convert the string to the string ID
 * @params str - const char* as c string value
 */
#define SID(str) str##_sid


namespace vm
{
	using StringId = u32;
	/**
	 * \brief The string will be used when the string id
	 * does not found a record
	 */
	extern const std::string NULL_STRING;

	std::string to_str(StringId obj);



	/**
	 * Find string in the database by the StringId
	 * in case if there is no this string in the
	 * database return the NULL_STRING
	 * @params sid - The string's id
	 */
	const std::string& lookup_string(const StringId sid);
	/**
	 * Find string in the database by the StringId
	 * but in case if it was not found return the
	 * string with the string id in it.
	 * @params sid - The string's id
	 */
	std::string lookup_string_safe(const StringId sid);

	/**
	 * @brief Define new string in the system
	 * @param str - The string to define
	 * @return String Id of the string
	 */
	StringId define_string(const std::string& str);
	/**
	 * Clear the DB of strings
	 * @attention It is not destructed method
	 * just will eliminate the strings out
	 * of memory
	 */
	void clear_strings();
	/**
	 * Load strings file to the database
	 * @params in - The input stream
	 * @params path - a path to the file used for printing log only. Can be any string.
	 */
	void load_strings_file(std::istream& in, std::string path);
	/**
	 * Load strings file to the database
	 * @params path - a path to the file
	 */
	void load_strings_file(const std::string& path);

	/** Print strings in the database to the screen */
	void print_strings();

	/** Generate general symbol -- random unique Symbol StringId */
	StringId get_gen_sym();

	void save_strings_file(const std::string& path);
}
