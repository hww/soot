#include "carbon/vm/NativeArray.hpp"

#include "carbon/gc/GC.hpp"
#include "carbon/gc/TypeRegistry.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "util/Log.hpp"

namespace carbon {

    namespace {

        struct DArrayHeader {
            u32   size;
            u32   capacity;
            void *data;
        };

        inline const DArrayHeader *as_array(const Variant &v) {
            if (!v.is_ptr()) return nullptr;
            return static_cast<const DArrayHeader *>(v.get_ptr());
        }

        // mark delegate: walk the element array if it holds GC pointers.
        // Without a per-element type, we cannot decide statically; the
        // caller must register a specialised array type if the elements
        // contain pointers. The generic runtime-array type has no pointers.
        void mark_runtime_array(void * /*payload*/, gc::MarkVisitor /*visitor*/, void * /*user*/) {}

        void destroy_runtime_array(void * /*payload*/) {}

        void register_array_types() {
            auto &reg = gc::type_registry();

            auto reg_one = [&](const char *name) {
                const sid64 sid = static_cast<sid64>(SID(name));
                if (reg.find(sid)) return;
                gc::TypeDescriptor td{};
                td.name = sid;
                td.size = sizeof(DArrayHeader);
                td.align = alignof(DArrayHeader);
                td.num_ptr_fields = 0;
                td.flags = gc::TYPE_IS_REFERENCE;
                td.ptr_fields = nullptr;
                td.mark = &mark_runtime_array;
                td.destroy = &destroy_runtime_array;
                reg.register_type(td);
            };

            reg_one("runtime-array");
            reg_one("runtime-carray");
        }

    } // namespace

    Variant native_array_size(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<i64>(0));
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(a->size));
    }

    Variant native_array_cap(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<i64>(0));
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(a->capacity));
    }

    Variant native_array_data(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        const auto *a = as_array(argv[0]);
        if (!a) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        return Variant(a->data, RuntimeType::Pointer);
    }

    Variant native_array_clear(u32 argc, const Variant *argv) {
        if (argc < 1) return Variant();
        auto *a = const_cast<DArrayHeader *>(as_array(argv[0]));
        if (!a) return Variant();
        a->size = 0;
        return Variant();
    }

    void NativeArray::initialize_builtins() {
        register_array_types();

        auto &fn = NativeFunctionRegistry::get_instance();
        fn.register_function("array-size", &native_array_size);
        fn.register_function("array-cap", &native_array_cap);
        fn.register_function("array-data", &native_array_data);
        fn.register_function("array-clear", &native_array_clear);

        lg::info("NativeArray: registered 4 array functions");
    }

} // namespace carbon