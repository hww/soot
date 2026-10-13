#include "carbon/vm/NativeString.hpp"

#include "carbon/gc/GC.hpp"
#include "carbon/gc/TypeRegistry.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "util/Log.hpp"

#include <cstring>

namespace carbon {

    namespace {

        inline char *alloc_cstr(std::size_t len) {
            void *p = gc::alloc(SID("cstr"), len, 1);
            return static_cast<char *>(p);
        }

        // cstr contains no GC pointers.
        void mark_cstr(void * /*payload*/, gc::MarkVisitor /*visitor*/, void * /*user*/) {}

        // cstr owns no external resources.
        void destroy_cstr(void * /*payload*/) {}

    } // namespace

    Variant native_string_alloc(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_int()) {
            lg::warn("string-alloc: expected 1 integer argument");
            return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        }
        const i64 size = argv[0].get_i64();
        if (size <= 0) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        char *p = alloc_cstr(static_cast<std::size_t>(size));
        if (!p) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        p[size - 1] = '\0';
        return Variant(static_cast<void *>(p), RuntimeType::Pointer);
    }

    Variant native_string_from_c(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_ptr()) {
            lg::warn("string-from-c: expected 1 pointer argument");
            return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        }
        const char *src = static_cast<const char *>(argv[0].get_ptr());
        if (!src) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        const std::size_t len = std::strlen(src) + 1;
        char             *dst = alloc_cstr(len);
        if (!dst) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        std::memcpy(dst, src, len);
        return Variant(static_cast<void *>(dst), RuntimeType::Pointer);
    }

    Variant native_string_concat(u32 argc, const Variant *argv) {
        if (argc < 2 || !argv[0].is_ptr() || !argv[1].is_ptr()) {
            lg::warn("string-concat: expected 2 pointer arguments");
            return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        }
        const char *a = static_cast<const char *>(argv[0].get_ptr());
        const char *b = static_cast<const char *>(argv[1].get_ptr());
        if (!a) a = "";
        if (!b) b = "";
        const std::size_t la = std::strlen(a);
        const std::size_t lb = std::strlen(b);
        char             *dst = alloc_cstr(la + lb + 1);
        if (!dst) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        std::memcpy(dst, a, la);
        std::memcpy(dst + la, b, lb);
        dst[la + lb] = '\0';
        return Variant(static_cast<void *>(dst), RuntimeType::Pointer);
    }

    Variant native_string_len(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_ptr()) {
            lg::warn("string-len: expected 1 pointer argument");
            return Variant(static_cast<i64>(0));
        }
        const char *s = static_cast<const char *>(argv[0].get_ptr());
        return Variant(static_cast<i64>(s ? std::strlen(s) : 0));
    }

    Variant native_string_eq(u32 argc, const Variant *argv) {
        if (argc < 2 || !argv[0].is_ptr() || !argv[1].is_ptr()) { return Variant(false); }
        const char *a = static_cast<const char *>(argv[0].get_ptr());
        const char *b = static_cast<const char *>(argv[1].get_ptr());
        if (a == b) return Variant(true);
        if (!a || !b) return Variant(false);
        return Variant(std::strcmp(a, b) == 0);
    }

    Variant native_string_cstr(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_ptr()) {
            return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        }
        return Variant(const_cast<void *>(argv[0].get_ptr()), RuntimeType::Pointer);
    }

    void NativeString::initialize_builtins() {
        // Register the cstr type. Variable-size, no pointers, nothing to
        // destroy. Both delegates are provided so the type descriptor is
        // complete.
        auto &reg = gc::type_registry();
        if (!reg.find(SID("cstr"))) {
            gc::TypeDescriptor td{};
            td.name = static_cast<u32>(static_cast<sid64>(SID("cstr")));
            td.size = 0;
            td.align = 1;
            td.num_ptr_fields = 0;
            td.flags = gc::TYPE_IS_SCALAR;
            td.ptr_fields = nullptr;
            td.mark = &mark_cstr;
            td.destroy = &destroy_cstr;
            reg.register_type(td);
        }

        auto &fn = NativeFunctionRegistry::get_instance();
        fn.register_function("string-alloc", &native_string_alloc);
        fn.register_function("string-from-c", &native_string_from_c);
        fn.register_function("string-concat", &native_string_concat);
        fn.register_function("string-len", &native_string_len);
        fn.register_function("string-eq", &native_string_eq);
        fn.register_function("string-cstr", &native_string_cstr);

        lg::info("NativeString: registered 6 string functions");
    }

} // namespace carbon