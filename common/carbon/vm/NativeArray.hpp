#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"

namespace carbon {

    // ========================================================================
    // NativeArray
    // ========================================================================
    //
    // GC-allocated arrays. The header layout is shared with gc::DArray:
    //
    //   +0x00  u32   size
    //   +0x04  u32   capacity
    //   +0x08  void* data     (GC heap)
    //
    //   array-size  (a:pointer)  -> i64
    //   array-cap   (a:pointer)  -> i64
    //   array-data  (a:pointer)  -> pointer
    //   array-clear (a:pointer)  -> null
    //
    class NativeArray {
    public:
        NativeArray() = default;
        ~NativeArray() = default;

        NativeArray(const NativeArray &) = delete;
        NativeArray &operator=(const NativeArray &) = delete;

        static void initialize_builtins();
        void        clear_all() noexcept {}
    };

    Variant native_array_size(u32 argc, const Variant *argv);
    Variant native_array_cap(u32 argc, const Variant *argv);
    Variant native_array_data(u32 argc, const Variant *argv);
    Variant native_array_clear(u32 argc, const Variant *argv);

} // namespace carbon