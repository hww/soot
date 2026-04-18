#ifndef STRING_ID_HASH_H
#define STRING_ID_HASH_H

#include <cstdint>
#include <string>

namespace util {

// ============================================================================
// Runtime вычисления
// ============================================================================

uint64_t ToStringId64(const char* str) noexcept;
uint64_t ToStringId64(const std::string& str) noexcept;

uint32_t ToStringId32(const char* str) noexcept;
uint32_t ToStringId32(const std::string& str) noexcept;

// ============================================================================
// Constexpr вычисления (для литералов)
// ============================================================================

// FNV-1a 64-bit
constexpr uint64_t ToStringId64_Const(const char* str, uint64_t hash = 0xCBF29CE484222325ULL) noexcept {
    return *str ? ToStringId64_Const(str + 1, 0x100000001B3ULL * (hash ^ static_cast<uint64_t>(*str))) : hash;
}

// FNV-1a 32-bit
constexpr uint32_t ToStringId32_Const(const char* str, uint32_t hash = 0x811C9DC5) noexcept {
    return *str ? ToStringId32_Const(str + 1, 0x01000193 * (hash ^ static_cast<uint32_t>(*str))) : hash;
}

} // namespace util

#endif