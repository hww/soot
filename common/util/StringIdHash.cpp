#include "StringIdHash.hpp"

namespace util {

uint64_t ToStringId64(const char* str) noexcept {
    uint64_t hash = 0xCBF29CE484222325ULL;
    if (str) {
        while (*str) {
            hash = 0x100000001B3ULL * (hash ^ static_cast<uint64_t>(*str++));
        }
    }
    return hash;
}

uint64_t ToStringId64(const std::string& str) noexcept {
    return ToStringId64(str.c_str());
}

uint32_t ToStringId32(const char* str) noexcept {
    uint32_t hash = 0x811C9DC5;
    if (str) {
        while (*str) {
            hash = 0x01000193 * (hash ^ static_cast<uint32_t>(*str++));
        }
    }
    return hash;
}

uint32_t ToStringId32(const std::string& str) noexcept {
    return ToStringId32(str.c_str());
}

} // namespace util