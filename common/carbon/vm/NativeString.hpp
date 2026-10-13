#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"

namespace carbon {

    // ========================================================================
    // NativeString
    // ========================================================================
    //
    // GC-allocated C strings. The type SID is "cstr"; its descriptor has
    // size 0 (variable) and no pointers.
    //
    //   string-alloc  (size:i64)              -> pointer
    //   string-from-c (src:pointer)           -> pointer
    //   string-concat (a:pointer, b:pointer)  -> pointer
    //   string-len    (s:pointer)             -> i64
    //   string-eq     (a:pointer, b:pointer)  -> bool
    //   string-cstr   (s:pointer)             -> pointer (identity)
    //
    class NativeString {
    public:
        NativeString() = default;
        ~NativeString() = default;

        NativeString(const NativeString &) = delete;
        NativeString &operator=(const NativeString &) = delete;

        static void initialize_builtins();
        void        clear_all() noexcept {}
    };

    Variant native_string_alloc(u32 argc, const Variant *argv);
    Variant native_string_from_c(u32 argc, const Variant *argv);
    Variant native_string_concat(u32 argc, const Variant *argv);
    Variant native_string_len(u32 argc, const Variant *argv);
    Variant native_string_eq(u32 argc, const Variant *argv);
    Variant native_string_cstr(u32 argc, const Variant *argv);

} // namespace carbon