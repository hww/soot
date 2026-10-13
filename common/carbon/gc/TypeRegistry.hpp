#pragma once

#include "CommonTypes.hpp"
#include "gc/GCHeader.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace carbon::gc {

    // ========================================================================
    // Type flags
    // ========================================================================
    enum GCTypeFlags : u32 {
        TYPE_IS_SCALAR = 1u << 0,
        TYPE_IS_REFERENCE = 1u << 1,
    };

    // ========================================================================
    // PtrField
    // ========================================================================
    //
    // Describes one pointer field inside a type. Only pointers are listed;
    // scalars are not registered with the GC at all.
    //
    //   type_index : index into TypeRegistry of the field's type
    //                (0 = unknown / opaque pointer)
    //   offset     : byte offset of the pointer within the payload
    //
    // array_count and array_inline are optional metadata for inline arrays
    // of pointers (e.g. Vector* arr[4] inside a struct). If array_count == 0,
    // the field is a single pointer.
    //
    struct PtrField {
        u32 type_index;
        u32 offset;
        u32 array_count; // 0 = single pointer, N = inline array of N
        u32 flags;       // reserved
    };

    // ========================================================================
    // Delegates
    // ========================================================================
    //
    // mark(payload, visitor, user):
    //   The type walks its own pointers and calls visitor(type_sid, ptr, size,
    //   user) for every GC pointer inside payload. The GC passes its own
    //   mark_object trampoline as the visitor, so recursion goes through the
    //   same code path.
    //
    // destroy(payload):
    //   The type releases non-GC resources. GC memory itself is reclaimed by
    //   sweep; do not call gc::release from here.
    //
    using MarkVisitor = void (*)(sid64 type_sid, void *payload, std::size_t size, void *user);
    using MarkFn = void      (*)(void *payload, MarkVisitor visitor, void *user);
    using DestroyFn = void   (*)(void *payload);

    // ========================================================================
    // TypeDescriptor
    // ========================================================================
    struct TypeDescriptor {
        sid64     name;           // SID hash (truncated sid64)
        u32       size;           // sizeof(payload)
        u32       align;          // alignof(payload)
        u32       num_ptr_fields; // number of entries in ptr_fields
        u32       flags;          // GCTypeFlags
        PtrField *ptr_fields;     // pointer fields only
        MarkFn    mark;           // traversal delegate (may be null = no pointers)
        DestroyFn destroy;        // deletion delegate (may be null = nothing to do)
    };

    // ========================================================================
    // TypeRegistry
    // ========================================================================
    //
    // Index 0 is a sentinel ("no type"). Real types start at index 1.
    // Indices are stable for the lifetime of the process.
    //
    class TypeRegistry {
    public:
        TypeRegistry() = default;
        ~TypeRegistry();

        TypeRegistry(const TypeRegistry &) = delete;
        TypeRegistry &operator=(const TypeRegistry &) = delete;

        // Register a type, return its stable index. Idempotent by SID:
        // a second call with the same SID returns the existing index.
        u32 register_type(const TypeDescriptor &td);

        [[nodiscard]] TypeDescriptor *find_by_index(u32 index) const noexcept;
        [[nodiscard]] TypeDescriptor *find(sid64 sid) const noexcept;
        [[nodiscard]] u32             find_index(sid64 sid) const noexcept;

        void                      clear();
        [[nodiscard]] std::size_t size() const noexcept { return m_types.size(); }

    private:
        std::vector<TypeDescriptor>        m_types;       // [0] is the sentinel
        std::vector<std::vector<PtrField>> m_ptr_storage; // stable backing for ptr_fields
        std::unordered_map<sid64, u32>     m_by_sid;
    };

    TypeRegistry &type_registry();

} // namespace carbon::gc