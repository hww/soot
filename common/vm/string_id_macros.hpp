#pragma once

#include "util/crc32.h"

namespace vm {
    /**
     * Main macro for creating StringId from string literals
     * Used in code, generator tool finds these calls
     * Example: SID("player") -> CRC32 of "player"
     */
#define SID(str) (::vm::compute_crc32_constexpr(str))

     /**
      * Constexpr version for compile-time calculations
      * Usage: constexpr StringID MY_ID = constexpr_sid("hello");
      */
    constexpr uint32_t compute_crc32_constexpr(const char* str) {
        return util::compute_crc32(str);
    }
}