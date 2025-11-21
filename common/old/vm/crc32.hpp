#pragma once

#include <string>
#include "platform.hpp"

namespace vm
{
	/** Define the table for const expression */
	static constexpr unsigned long const_crc32_table[] = {
#include "crc32_tab.hpp"
	};


	/**
	 * Compile time function
	 * Return a 32-bit CRC of the c string and the lenght.
	 * @param str - Any std::string value.
	 * @param len - The string's lenght.
	 * @warning - Use for constants in the C source only
	 */
	constexpr unsigned int const_crc32(const char* str, size_t len) {
		unsigned long crc = 0;
		if (len == 0)
			return crc;
		for (size_t i = 0; i < len; i++) {
			const char c = str[i];
			crc = (const_crc32_table[((crc >> 24) ^ static_cast<int>(c)) & 0xff] ^ (crc << 8)) & 0xffffffff;
		}
		return crc;
	}



	/**
	 * Return a 32-bit CRC of the c string and the length.
	 * @param str - Any std::string value.
	 * @param len - The string's length.
	 */
	u32 crc32(const char* str, size_t len);

	/**
	 * Return a 32-bit CRC of the c.
	 * @param str - Any std::string value.
	 */
	inline u32 crc32(const char* str)
	{
		return crc32(str, strlen(str));
	}

	/**
	 * Return a 32-bit CRC of the std::string.
	 * @param str - Any std::string value.
	 */
	inline u32 crc32(const std::string& str)
	{
		return crc32(str.c_str(), str.size());
	}
}