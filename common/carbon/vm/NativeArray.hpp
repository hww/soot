#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"

namespace carbon {

    // ========================================================================
    // RuntimeArray
    // ========================================================================
    //
    // GC-allocated dynamic array with an explicit element kind. Same
    // shape as gc::DArray<T> plus an element-size field so the GC knows
    // how to scan it.
    //
    //   +0x00  u32   m_size
    //   +0x04  u32   m_capacity
    //   +0x08  void* m_data        (GC heap, elements inline in the block)
    //   +0x10  u32   m_elem_size   (bytes per element: 4 / 4 / 8)
    //
    // Registered under three SIDs, one per element kind:
    //
    //   runtime-array-i32   -- elements are int32
    //   runtime-array-f32   -- elements are f32
    //   runtime-array-ptr   -- elements are GC pointers (marked)
    //
    struct RuntimeArray {
        u32   m_size;
        u32   m_capacity;
        void *m_data;
        u32   m_elem_size;
    };

    static_assert(sizeof(RuntimeArray) == 0x18, "RuntimeArray must be 0x18 bytes");

    // ========================================================================
    // NativeArray
    // ========================================================================
    //
    // Native functions for working with GC-allocated arrays.
    //
    //   (make-array kind)         -> array
    //   (array-size arr)          -> int
    //   (array-cap  arr)          -> int
    //   (array-data arr)          -> pointer   (raw element buffer)
    //   (array-clear arr)         -> null
    //   (array-push arr value)    -> arr
    //
    // kind is a C string: "i32", "f32", or anything else (treated as "ptr").
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

    // ========================================================================
    // Native function entry points
    // ========================================================================
    Variant native_make_array(u32 argc, const Variant *argv);
    Variant native_array_size(u32 argc, const Variant *argv);
    Variant native_array_cap(u32 argc, const Variant *argv);
    Variant native_array_data(u32 argc, const Variant *argv);
    Variant native_array_clear(u32 argc, const Variant *argv);
    Variant native_array_push(u32 argc, const Variant *argv);

} // namespace carbon