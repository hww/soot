#include <format>
#include "crc32.hpp"

namespace vm
{
	/** The CRC32 table for the runtime */
	static unsigned long crc32_table[] = {
#include "crc32_tab.hpp"
	};

	u32 crc32(const char* str, const size_t len)
	{
		unsigned long crc = 0;
		if (len == 0)
			return crc;
		for (size_t i = 0; i < len; i++) {
			const char c = str[i];
			crc = (crc32_table[((crc >> 24) ^ static_cast<int>(c)) & 0xff] ^ (crc << 8)) & 0xffffffff;
		}
		return crc;
	}
}