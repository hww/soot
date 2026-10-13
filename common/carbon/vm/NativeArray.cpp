#include "carbon/vm/NativeArray.hpp"

#include "carbon/gc/GC.hpp"
#include "carbon/gc/TypeRegistry.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "util/Log.hpp"

#include <cstring>

namespace carbon {

    namespace {

        // ====================================================================
        // Mark / destroy delegates
        // ====================================================================

        // Scalar element arrays hold no GC pointers.
        void mark_array_scalar(void * /*payload*/, gc::MarkVisitor /*visitor*/, void * /*user*/) {}
        void destroy_array_scalar(void * /*payload*/) {}

        // Pointer element arrays: forward every live element to the visitor.
        void mark_array_ptr(void *payload, gc::MarkVisitor visitor, void *user) {
            auto *a = static_cast<RuntimeArray *>(payload);
            if (!a || !a->m_data) return;
            void **elems = static_cast<void **>(a->m_data);
            for (u32 i = 0; i < a->m_size; ++i) {
                if (elems[i]) visitor(0, elems[i], 0, user);
            }
        }

        void destroy_array_ptr(void * /*payload*/) {
            // Element pointers are GC-allocated; sweep reclaims them.
        }

        // ====================================================================
        // Type registration
        // ====================================================================

        void register_one(const char *name, gc::MarkFn mark, gc::DestroyFn destroy) {
            auto       &reg = gc::type_registry();
            const sid64 sid = static_cast<sid64>(SID(name));
            if (reg.find(sid)) return;

            gc::TypeDescriptor td{};
            td.name = sid;
            td.size = sizeof(RuntimeArray);
            td.align = alignof(RuntimeArray);
            td.num_ptr_fields = 0;
            td.flags = gc::TYPE_IS_REFERENCE;
            td.ptr_fields = nullptr;
            td.mark = mark;
            td.destroy = destroy;
            reg.register_type(td);
        }

        void register_array_types() {
            register_one("runtime-array-i32", &mark_array_scalar, &destroy_array_scalar);
            register_one("runtime-array-f32", &mark_array_scalar, &destroy_array_scalar);
            register_one("runtime-array-ptr", &mark_array_ptr, &destroy_array_ptr);
        }

        // ====================================================================
        // Helpers
        // ====================================================================

        const RuntimeArray *as_array(const Variant &v) {
            if (!v.is_ptr()) return nullptr;
            return static_cast<const RuntimeArray *>(v.get_ptr());
        }

        RuntimeArray *as_array_mut(const Variant &v) {
            return const_cast<RuntimeArray *>(as_array(v));
        }

        sid64 array_sid_for(const char *elem_kind) {
            if (!elem_kind) return static_cast<sid64>(SID("runtime-array-ptr"));
            if (std::strcmp(elem_kind, "i32") == 0) {
                return static_cast<sid64>(SID("runtime-array-i32"));
            }
            if (std::strcmp(elem_kind, "f32") == 0) {
                return static_cast<sid64>(SID("runtime-array-f32"));
            }
            return static_cast<sid64>(SID("runtime-array-ptr"));
        }

        std::size_t elem_size_for(sid64 array_sid) {
            if (array_sid == static_cast<sid64>(SID("runtime-array-i32"))) return sizeof(i32);
            if (array_sid == static_cast<sid64>(SID("runtime-array-f32"))) return sizeof(f32);
            return sizeof(void *);
        }

    } // namespace

    // ========================================================================
    // Native functions
    // ========================================================================

    // (make-array kind) -> array
    Variant native_make_array(u32 argc, const Variant *argv) {
        const char *kind = nullptr;
        if (argc >= 1 && argv[0].is_ptr()) { kind = static_cast<const char *>(argv[0].get_ptr()); }

        const sid64 arr_sid = array_sid_for(kind);

        void *payload = gc::alloc(arr_sid, sizeof(RuntimeArray), alignof(RuntimeArray));
        if (!payload) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }

        auto *a = static_cast<RuntimeArray *>(payload);
        a->m_size = 0;
        a->m_capacity = 0;
        a->m_data = nullptr;
        a->m_elem_size = static_cast<u32>(elem_size_for(arr_sid));

        return Variant(static_cast<void *>(a), RuntimeType::Pointer);
    }

    // (array-size arr) -> int
    Variant native_array_size(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<i64>(0));
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(a->m_size));
    }

    // (array-cap arr) -> int
    Variant native_array_cap(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<i64>(0));
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(a->m_capacity));
    }

    // (array-data arr) -> pointer
    Variant native_array_data(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        return Variant(a->m_data, RuntimeType::Pointer);
    }

    // (array-clear arr) -> null
    Variant native_array_clear(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant();
        auto *a = as_array_mut(argv[0]);
        if (!a) return Variant();
        a->m_size = 0;
        return Variant();
    }

    // (array-push arr value) -> arr
    Variant native_array_push(u32 argc, const Variant *argv) {
        if (argc < 2) return Variant();
        auto *a = as_array_mut(argv[0]);
        if (!a) return Variant();

        const std::size_t elem_size = (a->m_elem_size != 0) ? a->m_elem_size : sizeof(void *);

        if (a->m_size >= a->m_capacity) {
            const u32 new_cap = (a->m_capacity == 0) ? 8u : (a->m_capacity * 2u);

            // Pick the concrete array SID from the element size.
            sid64 arr_sid = static_cast<sid64>(SID("runtime-array-ptr"));
            if (elem_size == sizeof(i32)) {
                arr_sid = static_cast<sid64>(SID("runtime-array-i32"));
            } else if (elem_size == sizeof(f32)) {
                arr_sid = static_cast<sid64>(SID("runtime-array-f32"));
            }

            void *new_data = gc::alloc(arr_sid, new_cap * elem_size, alignof(void *));
            if (!new_data) return Variant();

            if (a->m_data && a->m_size > 0) {
                std::memcpy(new_data, a->m_data, a->m_size * elem_size);
            }

            a->m_data = new_data;
            a->m_capacity = new_cap;
        }

        std::byte *slot = static_cast<std::byte *>(a->m_data) + (a->m_size * elem_size);

        const Variant &val = argv[1];
        if (elem_size == sizeof(i32)) {
            *reinterpret_cast<i32 *>(slot) = static_cast<i32>(val.to_int());
        } else if (elem_size == sizeof(f32)) {
            *reinterpret_cast<f32 *>(slot) = static_cast<f32>(val.to_float());
        } else {
            void *p = nullptr;
            if (val.is_ptr()) p = const_cast<void *>(val.get_ptr());
            *reinterpret_cast<void **>(slot) = p;
        }

        a->m_size++;
        return argv[0];
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void NativeArray::initialize_builtins() {
        register_array_types();

        auto &fn = NativeFunctionRegistry::get_instance();
        fn.register_function("make-array", &native_make_array);
        fn.register_function("array-size", &native_array_size);
        fn.register_function("array-cap", &native_array_cap);
        fn.register_function("array-data", &native_array_data);
        fn.register_function("array-clear", &native_array_clear);
        fn.register_function("array-push", &native_array_push);

        lg::info("NativeArray: registered 6 array functions");
    }

} // namespace carbon