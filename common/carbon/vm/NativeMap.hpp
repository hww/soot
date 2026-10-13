#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"

namespace carbon {

    // ========================================================================
    // RuntimeMap
    // ========================================================================
    //
    // Runtime representation of `map`.
    //
    //   +0x00  u32       m_size
    //   +0x04  u32       m_capacity
    //   +0x08  sid64*    m_pKeys       (GC heap)
    //   +0x10  Variant*  m_pValues     (GC heap)
    //
    // The key array holds scalars. The value array holds Variants; any
    // Variant whose type is Pointer or String contributes a GC pointer,
    // and the map's mark delegate forwards it to the GC.
    //
    struct RuntimeMap {
        u32      m_size;
        u32      m_capacity;
        sid64   *m_pKeys;
        Variant *m_pValues;
    };

    static_assert(sizeof(RuntimeMap) == 0x18, "RuntimeMap must be 0x18 bytes");
    
    // ========================================================================
    // Native function entry points
    // ========================================================================
    Variant native_make_map(u32 argc, const Variant *argv);
    Variant native_map_count(u32 argc, const Variant *argv);
    Variant native_map_get(u32 argc, const Variant *argv);
    Variant native_map_set(u32 argc, const Variant *argv);
    Variant native_map_del(u32 argc, const Variant *argv);
    Variant native_map_has(u32 argc, const Variant *argv);

    class NativeMap {
    public:
        NativeMap() = default;
        ~NativeMap() = default;

        NativeMap(const NativeMap &) = delete;
        NativeMap &operator=(const NativeMap &) = delete;

        static void initialize_builtins();
        void        clear_all() noexcept {}
    };

} // namespace carbon