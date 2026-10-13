#include "NativeAllocators.hpp"

#include "carbon/vm/NativeFunc.hpp"
#include "gc/GC.hpp"
#include "gc/TypeRegistry.hpp"
#include "util/Log.hpp"

#include <cstring>

namespace carbon {

    // =========================================================================
    // Native functions
    // =========================================================================
    //
    // Each function receives argc/argv from the VM and returns a Variant
    // holding a pointer to a freshly allocated object in the GC heap.
    //
    // If gc::alloc returns nullptr (OOM or unknown type), we return a
    // null pointer Variant.

    namespace {
        [[nodiscard]] Variant alloc_or_null(sid64 sid, std::size_t size, std::size_t align) {
            void *ptr = gc::alloc(sid, size, align);
            if (!ptr) {
                lg::warn("allocate: gc::alloc failed for sid=0x{:016X}", sid);
                return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
            }
            return Variant(ptr, RuntimeType::Pointer);
        }
    } // namespace

    Variant allocate_i64(u32 /*argc*/, const Variant * /*argv*/) {
        return alloc_or_null(SID("i64"), sizeof(i64), alignof(i64));
    }

    Variant allocate_f32(u32 /*argc*/, const Variant * /*argv*/) {
        return alloc_or_null(SID("f32"), sizeof(f32), alignof(f32));
    }

    Variant allocate_sid(u32 /*argc*/, const Variant * /*argv*/) {
        return alloc_or_null(SID("sid64"), sizeof(sid64), alignof(sid64));
    }

    Variant allocate_vector(u32 /*argc*/, const Variant * /*argv*/) {
        Variant v = alloc_or_null(SID("vector"), sizeof(vector), alignof(vector));
        if (v.get_ptr() != nullptr) { std::memset(v.get_ptr(), 0, sizeof(vector)); }
        return v;
    }

    Variant allocate_cvector(u32 /*argc*/, const Variant * /*argv*/) {
        Variant v = alloc_or_null(SID("cvector"), sizeof(cvector), alignof(cvector));
        if (v.get_ptr() != nullptr) { std::memset(v.get_ptr(), 0, sizeof(cvector)); }
        return v;
    }

    // =========================================================================
    // Types
    // =========================================================================

    void NativeAllocators::register_allocator_types() {
        auto &reg = gc::type_registry();

        // ---- i64 ----
        {
            gc::TypeDescriptor td{};
            td.name = SID("i64");
            td.size = sizeof(i64);
            td.align = alignof(i64);
            td.num_fields = 0;
            td.flags = gc::TYPE_IS_SCALAR;
            td.fields = nullptr;
            reg.register_type(td);
        }

        // ---- f32 ----
        {
            gc::TypeDescriptor td{};
            td.name = SID("f32");
            td.size = sizeof(f32);
            td.align = alignof(f32);
            td.num_fields = 0;
            td.flags = gc::TYPE_IS_SCALAR;
            td.fields = nullptr;
            reg.register_type(td);
        }

        // ---- sid64 ----
        {
            gc::TypeDescriptor td{};
            td.name = SID("sid64");
            td.size = sizeof(sid64);
            td.align = alignof(sid64);
            td.num_fields = 0;
            td.flags = gc::TYPE_IS_SCALAR;
            td.fields = nullptr;
            reg.register_type(td);
        }

        // ---- vector ----
        // Four floats, 16 bytes. No pointers, nothing to scan.
        {
            gc::TypeDescriptor td{};
            td.name = SID("vector");
            td.size = sizeof(vector);
            td.align = alignof(vector);
            td.num_fields = 0;
            td.flags = gc::TYPE_IS_REFERENCE;
            td.fields = nullptr;
            reg.register_type(td);
        }

        // ---- cvector ----
        {
            gc::TypeDescriptor td{};
            td.name = SID("cvector");
            td.size = sizeof(cvector);
            td.align = alignof(cvector);
            td.num_fields = 0;
            td.flags = gc::TYPE_IS_REFERENCE;
            td.fields = nullptr;
            reg.register_type(td);
        }
    }

    // =========================================================================
    // Registry
    // =========================================================================

    void NativeAllocators::initialize_builtins() {
        // First register the types in the GC.
        register_allocator_types();

        // Then register the native functions.
        auto &reg = NativeFunctionRegistry::get_instance();
        reg.register_function("allocate-i64", &allocate_i64);
        reg.register_function("allocate-f32", &allocate_f32);
        reg.register_function("allocate-sid", &allocate_sid);
        reg.register_function("allocate-vector", &allocate_vector);
        reg.register_function("allocate-cvector", &allocate_cvector);

        lg::info("Initialized 5 allocator native functions");
    }

} // namespace carbon