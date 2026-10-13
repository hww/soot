#include "NativeAllocators.hpp"

#include "carbon/vm/NativeArray.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeMap.hpp"
#include "carbon/vm/NativeString.hpp"
#include "gc/GC.hpp"
#include "gc/TypeRegistry.hpp"
#include "util/Log.hpp"

#include <cstring>

namespace carbon {

    // ========================================================================
    // Universal allocator
    // ========================================================================
    //
    // allocate(where, type_id) -> pointer
    //
    // `where` is one of "static", "global", "heap". It is currently
    // accepted but ignored: all three allocate from the GC heap, because
    // the VM has no separate global or static region. This matches the
    // "static at top level is compile-time; at runtime it is just heap"
    // rule the compiler enforces on its side.
    //
    // `type_id` is the SID of the type to allocate. The allocator looks
    // it up in the GC type registry, takes the size and alignment from
    // the descriptor, and asks the GC for a zero-initialized block.
    //
    // A type without a registered GC descriptor (or with size 0, meaning
    // variable-size) is rejected: the allocator cannot guess how much
    // memory to reserve.
    //
    namespace {

        // ====================================================================
        // allocate_impl — the one real implementation.
        // ====================================================================
        //
        // `silent` controls logging only. The behaviour is identical:
        // a missing type or a bad argument list still returns a null
        // pointer Variant. `silent = true` is used by tests that exercise
        // the error path on purpose and do not want the log to shout.
        //
        Variant allocate_impl(u32 argc, const Variant *argv, bool silent) {
            if (argc < 2) {
                if (!silent) lg::error("allocate: expected (where, type-id), got {}", argc);
                return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
            }

            const sid64 type_sid = argv[1].get_sid();

            auto *td = gc::type_registry().find(type_sid);
            if (!td) {
                if (!silent) lg::error("allocate: unknown type 0x{:016X}", type_sid);
                return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
            }

            if (td->size == 0) {
                if (!silent) lg::error("allocate: type 0x{:016X} has variable size", type_sid);
                return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
            }

            void *ptr = gc::alloc(type_sid, td->size, td->align);
            if (!ptr) {
                if (!silent) lg::error("allocate: gc::alloc failed for type 0x{:016X}", type_sid);
                return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
            }
            return Variant(ptr, RuntimeType::Pointer);
        }



    } // namespace
      
      
    // ========================================================================
    // Public entry points
    // ========================================================================

    // Loud: this is what compiled code calls. Every error is logged.
    Variant allocate(u32 argc, const Variant *argv) {
        return allocate_impl(argc, argv, /*silent=*/false);
    }

    // Silent: same behaviour, no logging. Used by tests that deliberately
    // exercise the error path.
    Variant allocate_silent(u32 argc, const Variant *argv) {
        return allocate_impl(argc, argv, /*silent=*/true);
    } 

    // ========================================================================
    // Type registration
    // ========================================================================
    //
    // Register in the GC only the types that are allocated by name from
    // compiled code. `vector` and `cvector` are included here because the
    // compiler emits `allocate(where, SID("vector"))` for them. If a new
    // scalar or reference type is added to the compiler's type table and
    // is meant to be allocated by `new`, it must be registered here as
    // well.
    //
    void NativeAllocators::register_allocator_types() {
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
            td.mark = nullptr;
            td.destroy = nullptr;
            reg.register_type(td);
        };

        reg_scalar("i64", sizeof(i64), alignof(i64));
        reg_scalar("f32", sizeof(f32), alignof(f32));
        reg_scalar("sid64", sizeof(sid64), alignof(sid64));
        reg_scalar("variant", sizeof(Variant), alignof(Variant));

        reg_ref("vector", sizeof(vector), alignof(vector));
        reg_ref("cvector", sizeof(cvector), alignof(cvector));
    }

    // ========================================================================
    // Registration
    // ========================================================================
    //
    // Registers the universal allocator in the native function registry,
    // then hands off to the string, map, and array modules so that a
    // single call to initialize_builtins installs everything a VM
    // instance needs.
    //
    void NativeAllocators::initialize_builtins() {
        register_allocator_types();

        auto &reg = NativeFunctionRegistry::get_instance();
        reg.register_function("allocate", &allocate);
        reg.register_function("allocate-silent", &allocate_silent);

        NativeString::initialize_builtins();
        NativeMap::initialize_builtins();
        NativeArray::initialize_builtins();

        lg::info("NativeAllocators: registered 1 allocator, 3 modules");
    }

} // namespace carbon