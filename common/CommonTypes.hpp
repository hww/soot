// common/util/CommonTypes.hpp
#pragma once

/*!
 * @file CommonTypes.hpp
 * Common types used across the entire project.
 */

#include <cstdint>
#include <stdexcept>
#include <string>
#include <limits>
#include <exception>
#include "fmt/format.h"

// ============================================================================
// Windows
// ============================================================================

#if _WIN32

typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;

#include <cstddef>
using ssize_t = ptrdiff_t;  // или long long

#endif

// ============================================================================
// Basic Integer Types
// ============================================================================

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using p64 = uintptr_t;

// ============================================================================
// Windows & Visual Studio
// ============================================================================

using sid64 = uint64_t;
using sid32 = uint32_t;

#ifdef _WIN32
    #include <io.h>
    #include <winsock2.h>

    // Вместо #define close _close используй это:
    inline int posix_close(int fd) { return _close(fd); }
    inline int socket_close(SOCKET s) { return closesocket(s); }
#endif

// ============================================================================
// 128-bit Types
// ============================================================================

struct u128 {
    union {
        u64 du64[2];
        i64 ds64[2];
        u32 du32[4];
        i32 ds32[4];
        u16 du16[8];
        i16 ds16[8];
        u8 du8[16];
        i8 ds8[16];
        float f[4];
    };
};
static_assert(sizeof(u128) == 16, "u128 must be 16 bytes");

// ============================================================================
// Constants
// ============================================================================

constexpr i32 INVALID_INDEX = -1;  // Было INDEX_NONE, переименовано чтобы избежать конфликта с макросом
constexpr u32 MAX_REGISTERS = 34;
constexpr u32 ARG_REGISTERS_OFFSET = 24;  // r24-r33: arguments
constexpr u32 LOCAL_REGISTERS_OFFSET = 0; // r0-r23: local variables
constexpr u32 MAX_LOCALS = ARG_REGISTERS_OFFSET - LOCAL_REGISTERS_OFFSET; // 24
constexpr u32 MAX_ARGS = MAX_REGISTERS - ARG_REGISTERS_OFFSET; // 10

// ============================================================================
// Verioning
// ============================================================================

enum class SootPlatform { Default, Z80 };

inline const char* version_to_game_name(SootPlatform v) {
    switch (v) {
        case SootPlatform::Default:
            return "default";
        case SootPlatform::Z80:
            return "z80";
    }
    throw std::runtime_error("unknown platform");
}
  

// ============================================================================
// Exceptions
// ============================================================================

class OverflowException : public std::exception {
public:
    OverflowException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
private:
    std::string message;
};

// ============================================================================
// Safe Casting
// ============================================================================

inline u32 safe_cast_u32(u64 value) {
    if (value > std::numeric_limits<u32>::max()) {
        throw OverflowException(fmt::format("Value {} too large for u32", value));
    }
    return static_cast<u32>(value);
}

inline i32 safe_cast_s32(i64 value) {
    if (value > std::numeric_limits<i32>::max() || value < std::numeric_limits<i32>::min()) {
        throw OverflowException(fmt::format("Value {} out of range for i32", value));
    }
    return static_cast<i32>(value);
}

inline f32 safe_cast_f32(f64 value) {
    constexpr f64 max_f32 = static_cast<f64>(std::numeric_limits<f32>::max());
    constexpr f64 min_f32 = -static_cast<f64>(std::numeric_limits<f32>::max());
    if (value > max_f32 || value < min_f32) {
        throw OverflowException(fmt::format("Value {} out of range for f32", value));
    }
    return static_cast<f32>(value);
}

// ============================================================================
// Utility Types
// ============================================================================

union U32Float {
    u32 as_u32;
    i32 as_s32;
    f32 as_f32;
};

union U64Float {
    u64 as_u64;
    i64 as_s64;
    f64 as_f64;
};

struct Vector4 {
    f32 x, y, z, w;
    Vector4() : x(0), y(0), z(0), w(0) {}
    Vector4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}

    std::string to_string() const {
        return fmt::format("({}, {}, {}, {})", x, y, z, w);
    }
};

// ============================================================================
// Enum Flags Support
// ============================================================================

template<typename T>
constexpr auto to_underlying(T value) -> std::underlying_type_t<T> {
    return static_cast<std::underlying_type_t<T>>(value);
}

#define ENUM_FLAG_OPERATORS(T) \
constexpr T operator~(T a) { return static_cast<T>(~to_underlying(a)); } \
constexpr T operator|(T a, T b) { return static_cast<T>(to_underlying(a) | to_underlying(b)); } \
constexpr T operator&(T a, T b) { return static_cast<T>(to_underlying(a) & to_underlying(b)); } \
constexpr T operator^(T a, T b) { return static_cast<T>(to_underlying(a) ^ to_underlying(b)); } \
constexpr T& operator|=(T& a, T b) { return a = a | b; } \
constexpr T& operator&=(T& a, T b) { return a = a & b; } \
constexpr T& operator^=(T& a, T b) { return a = a ^ b; }

// ============================================================================
// Platform Detection
// ============================================================================

#if defined __linux || defined __linux__ || defined __APPLE__
#define OS_POSIX
#endif