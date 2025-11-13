#ifndef CRC32_H
#define CRC32_H

#include <cstdint>
#include <string>

uint32_t compute_crc32(const std::string& str);
uint32_t compute_crc32(const char* data, size_t length);

#endif