#pragma once

#include "CommonTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace carbon::gc {

    // ========================================================================
    // GCHeader
    // ========================================================================
    //
    // 8 bytes without debug name, 16 bytes with.
    //
    //   +0x00  u32 type_index     index into TypeRegistry (0 = untracked)
    //   +0x04  u32 flags
    //   +0x08  u64 name           optional SID (CARBON_GC_DEBUG_NAME only)
    //
    // flags:
    //   bit  0     : MARKED
    //   bit  1     : DEAD
    //   bits 2..5  : pass_cycle   (1..15, 0 = "not initialised")
    //   bits 6..11 : size_class   (0..63)
    //   bits 12..31: reserved
    //
    // pass_cycle is 0 only for freshly memset blocks. alloc() never writes 0
    // there, so a live object always has pass_cycle in 1..15. The 0 value
    // means "do not touch in sweep": an object that has not yet participated
    // in any pass.
    //
    struct GCHeader {
        u32 type_index;
        u32 flags;
#if CARBON_GC_DEBUG_NAME
        u64 name;
#endif
    };

#if CARBON_GC_DEBUG_NAME
    static_assert(sizeof(GCHeader) == 16, "GCHeader must be 16 bytes with debug name");
#else
    static_assert(sizeof(GCHeader) == 8, "GCHeader must be 8 bytes without debug name");
#endif

    constexpr u32 GC_FLAG_MARKED = 1u << 0;
    constexpr u32 GC_FLAG_DEAD = 1u << 1;

    constexpr u32 GC_PASS_CYCLE_SHIFT = 2;
    constexpr u32 GC_PASS_CYCLE_MASK = 0xFu << GC_PASS_CYCLE_SHIFT;
    constexpr u32 GC_PASS_CYCLE_MOD = 15; // period: 1..15

    constexpr u32 GC_SIZE_CLASS_SHIFT = 6;
    constexpr u32 GC_SIZE_CLASS_MASK = 0x3Fu << GC_SIZE_CLASS_SHIFT;
    constexpr u32 GC_SIZE_CLASS_MAX = 63;

    inline u32 gc_get_pass_cycle(u32 f) noexcept {
        return (f & GC_PASS_CYCLE_MASK) >> GC_PASS_CYCLE_SHIFT;
    }
    inline void gc_set_pass_cycle(u32 &f, u32 pc) noexcept {
        f = (f & ~GC_PASS_CYCLE_MASK) | ((pc & 0xFu) << GC_PASS_CYCLE_SHIFT);
    }
    inline u32 gc_get_size_class(u32 f) noexcept {
        return (f & GC_SIZE_CLASS_MASK) >> GC_SIZE_CLASS_SHIFT;
    }
    inline void gc_set_size_class(u32 &f, u32 sc) noexcept {
        f = (f & ~GC_SIZE_CLASS_MASK) | ((sc & 0x3Fu) << GC_SIZE_CLASS_SHIFT);
    }

    // Advance a cycle counter, skipping 0. Never returns 0.
    inline u32 gc_next_cycle(u32 current) noexcept {
        u32 c = (current + 1) & GC_PASS_CYCLE_MOD;
        if (c == 0) c = 1;
        return c;
    }

    inline GCHeader *header_of(void *payload) noexcept {
        return reinterpret_cast<GCHeader *>(static_cast<std::byte *>(payload) - sizeof(GCHeader));
    }
    inline const GCHeader *header_of(const void *payload) noexcept {
        return reinterpret_cast<const GCHeader *>(static_cast<const std::byte *>(payload) -
                                                  sizeof(GCHeader));
    }
    inline void *payload_of(GCHeader *h) noexcept {
        return static_cast<std::byte *>(static_cast<void *>(h)) + sizeof(GCHeader);
    }

} // namespace carbon::gc