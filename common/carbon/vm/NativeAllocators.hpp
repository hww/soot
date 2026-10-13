#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "gc/GC.hpp"

#include "generated/vector.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace carbon {

    // ========================================================================
    // NativeAllocators
    // ========================================================================
    //
    // Native functions for allocating objects in the GC heap. Each function
    // is a thin wrapper over gc::alloc with the correct type sid.
    //
    // Types are registered once in initialize_builtins() via
    // register_allocator_types(). If a type is already registered (for
    // example, loaded from a .bin), register_type is idempotent.
    //
    // The allocated objects live in the GC heap. They are reclaimed
    // automatically by the GC sweep, or explicitly via gc::release.
    //
    class NativeAllocators {
    public:
        NativeAllocators() = default;
        ~NativeAllocators() = default;

        NativeAllocators(const NativeAllocators &) = delete;
        NativeAllocators &operator=(const NativeAllocators &) = delete;

        // Register the allocate-* native functions in the registry.
        // Also register the corresponding types in the GC if not present.
        void initialize_builtins();

        // Used to clear the old per-frame pools. Now a no-op, kept for
        // compatibility with VirtualMachine::purge.
        void clear_all() noexcept {}

    private:
        // Register i64, f32, sid64, vector, cvector in the TypeRegistry.
        static void register_allocator_types();
    };

} // namespace carbon