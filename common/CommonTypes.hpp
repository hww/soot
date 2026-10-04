// common/util/CommonTypes.hpp
#pragma once

/*!
 * @file CommonTypes.hpp
 * Common types used across the entire project.
 */

// ============================================================================
// Platform macros MUST come before any system header
// ============================================================================

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

// OS_POSIX must be defined before any header uses it
#if defined(__linux) || defined(__linux__) || defined(__APPLE__)
#ifndef OS_POSIX
#define OS_POSIX
#endif
#endif

// ============================================================================
// System headers
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>

#include "fmt/format.h"

// ============================================================================
// Windows specifics
// ============================================================================

#ifdef _WIN32
#include <BaseTsd.h>
#include <io.h>

// winsock2.h MUST come before Windows.h
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Defensive: some SDKs still leak these even with NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

using ssize_t = SSIZE_T;

// POSIX-like shims
inline int posix_close(int fd) { return _close(fd); }
inline int socket_close(SOCKET s) { return closesocket(s); }
#endif

// ============================================================================
// Basic Integer Types
// ============================================================================

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using p64 = uintptr_t;

// ============================================================================
// Legacy aliases (used by Interpreter / Errors / other older code)
// ============================================================================

using uint  = unsigned int;
using ushort = unsigned short;
using ulong  = unsigned long;

// ============================================================================
// Windows & Visual Studio compat aliases
// ============================================================================

using sid64 = uint64_t;
using sid32 = uint32_t;

// ============================================================================
// 128-bit Types
// ============================================================================

struct u128 {
    union {
        u64   du64[2];
        i64   ds64[2];
        u32   du32[4];
        i32   ds32[4];
        u16   du16[8];
        i16   ds16[8];
        u8    du8[16];
        i8    ds8[16];
        float f[4];
    };
};
static_assert(sizeof(u128) == 16, "u128 must be 16 bytes");

// ============================================================================
// Constants
// ============================================================================

constexpr i32 INVALID_INDEX = -1;
constexpr u32 MAX_REGISTERS = 34;
constexpr u32 ARG_REGISTERS_OFFSET = 24;                                  // r24-r33: arguments
constexpr u32 LOCAL_REGISTERS_OFFSET = 0;                                 // r0-r23: local variables
constexpr u32 MAX_LOCALS = ARG_REGISTERS_OFFSET - LOCAL_REGISTERS_OFFSET; // 24
constexpr u32 MAX_ARGS = MAX_REGISTERS - ARG_REGISTERS_OFFSET;            // 10

// ============================================================================
// Versioning
// ============================================================================

enum class SootPlatform { Default, Z80 };

inline const char *soot_plaform_to_game_name(SootPlatform v) {
    switch (v) {
    case SootPlatform::Default: return "default";
    case SootPlatform::Z80: return "z80";
    }
    throw std::runtime_error("unknown platform");
}

// ============================================================================
// Exceptions
// ============================================================================

class OverflowException : public std::exception {
public:
    explicit OverflowException(const std::string &msg) : message(msg) {}
    const char *what() const noexcept override { return message.c_str(); }

private:
    std::string message;
};

// ============================================================================
// Safe Casting
// ============================================================================

inline u32 safe_cast_u32(u64 value) {
    if (value > (std::numeric_limits<u32>::max)()) {
        throw OverflowException(fmt::format("Value {} too large for u32", value));
    }
    return static_cast<u32>(value);
}

inline i32 safe_cast_s32(i64 value) {
    if (value > (std::numeric_limits<i32>::max)() || value < (std::numeric_limits<i32>::min)()) {
        throw OverflowException(fmt::format("Value {} out of range for i32", value));
    }
    return static_cast<i32>(value);
}

inline f32 safe_cast_f32(f64 value) {
    constexpr f64 max_f32 = static_cast<f64>((std::numeric_limits<f32>::max)());
    constexpr f64 min_f32 = -static_cast<f64>((std::numeric_limits<f32>::max)());
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
    Vector4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}

    std::string to_string() const { return fmt::format("({}, {}, {}, {})", x, y, z, w); }
};

// ============================================================================
// Enum Flags Support
// ============================================================================

template <typename T> constexpr auto to_underlying(T value) -> std::underlying_type_t<T> {
    return static_cast<std::underlying_type_t<T>>(value);
}

#define ENUM_FLAG_OPERATORS(T)                                                                     \
    constexpr T operator~(T a) { return static_cast<T>(~to_underlying(a)); }                       \
    constexpr T operator|(T a, T b) {                                                              \
        return static_cast<T>(to_underlying(a) | to_underlying(b));                                \
    }                                                                                              \
    constexpr T operator&(T a, T b) {                                                              \
        return static_cast<T>(to_underlying(a) & to_underlying(b));                                \
    }                                                                                              \
    constexpr T operator^(T a, T b) {                                                              \
        return static_cast<T>(to_underlying(a) ^ to_underlying(b));                                \
    }                                                                                              \
    constexpr T &operator|=(T &a, T b) { return a = a | b; }                                       \
    constexpr T &operator&=(T &a, T b) { return a = a & b; }                                       \
    constexpr T &operator^=(T &a, T b) { return a = a ^ b; }