#include "NativeAllocators.hpp"

#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeString.hpp"
#include "carbon/vm/NativeMap.hpp"
#include "carbon/vm/NativeArray.hpp"
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
        // inside NativeAllocators::register_allocator_types()
        auto &reg = gc::type_registry();

        auto reg_scalar = [&](const char *name, std::size_t size, std::size_t align) {
            const sid64 sid = static_cast<sid64>(SID(name));
            if (reg.find(sid)) return;
            gc::TypeDescriptor td{};
            td.name = sid;
            td.size = static_cast<u32>(size);
            td.align = static_cast<u32>(align);
            td.num_ptr_fields = 0;
            td.flags = gc::TYPE_IS_SCALAR;
            td.ptr_fields = nullptr;
            td.mark = nullptr;
            td.destroy = nullptr;
            reg.register_type(td);
        };

        auto reg_ref = [&](const char *name, std::size_t size, std::size_t align) {
            const sid64 sid = static_cast<sid64>(SID(name));
            if (reg.find(sid)) return;
            gc::TypeDescriptor td{};
            td.name = sid;
            td.size = static_cast<u32>(size);
            td.align = static_cast<u32>(align);
            td.num_ptr_fields = 0;
            td.flags = gc::TYPE_IS_REFERENCE;
            td.ptr_fields = nullptr;
            td.mark = nullptr;    // no pointers inside
            td.destroy = nullptr; // nothing to free
            reg.register_type(td);
        };

        reg_scalar("i64", sizeof(i64), alignof(i64));
        reg_scalar("f32", sizeof(f32), alignof(f32));
        reg_scalar("sid64", sizeof(sid64), alignof(sid64));
        reg_scalar("variant", sizeof(Variant), alignof(Variant));

        reg_ref("vector", sizeof(vector), alignof(vector));
        reg_ref("cvector", sizeof(cvector), alignof(cvector));
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
        
        NativeString::initialize_builtins();
        NativeString::initialize_builtins();
        NativeString::initialize_builtins();

        lg::info("Initialized 5 allocator native functions");
    }

} // namespace carbon