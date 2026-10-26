#include "carbon/vm/NativeMap.hpp"

#include "carbon/gc/GC.hpp"
#include "carbon/gc/TypeRegistry.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/vm/NativeFunc.hpp"

#include "util/Log.hpp"

namespace carbon {

    namespace {

        RuntimeMap *make_empty_map() {
            void *payload = gc::alloc(SID("runtime-map"), sizeof(RuntimeMap), alignof(RuntimeMap));
            if (!payload) return nullptr;
            RuntimeMap *m = static_cast<RuntimeMap *>(payload);
            m->m_size = 0;
            m->m_capacity = 0;
            m->m_pKeys = nullptr;
            m->m_pValues = nullptr;
            return m;
        }

        i32 find_key(const RuntimeMap *m, sid64 key) {
            for (u32 i = 0; i < m->m_size; ++i) {
                if (m->m_pKeys[i] == key) return static_cast<i32>(i);
            }
            return -1;
        }

        bool grow_map(RuntimeMap *m) {
            const u32 new_cap = (m->m_capacity == 0) ? 8u : (m->m_capacity * 2u);

            sid64 *new_keys = static_cast<sid64 *>(
                gc::alloc(SID("sid64"), new_cap * sizeof(sid64), alignof(sid64)));
            if (!new_keys) return false;

            Variant *new_values = static_cast<Variant *>(
                gc::alloc(SID("variant"), new_cap * sizeof(Variant), alignof(Variant)));
            if (!new_values) return false;

            for (u32 i = 0; i < m->m_size; ++i) {
                new_keys[i] = m->m_pKeys[i];
                new_values[i] = m->m_pValues[i];
            }

            m->m_pKeys = new_keys;
            m->m_pValues = new_values;
            m->m_capacity = new_cap;
            return true;
        }

        // mark delegate: walk the value array and forward every GC pointer.
        // The GC does not need to know about Variant; only this delegate does.
        void mark_runtime_map(void *payload, gc::MarkVisitor visitor, void *user) {
            auto *m = static_cast<RuntimeMap *>(payload);
            if (!m || !m->m_pValues) return;
            for (u32 i = 0; i < m->m_size; ++i) {
                const Variant &v = m->m_pValues[i];
                if (v.get_type() == RuntimeType::Pointer) {
                    void *p = const_cast<void *>(v.get_ptr());
                    if (p) visitor(0, p, 0, user);
                }
            }
        }

        // destroy delegate: nothing to do. Keys/values live in the GC heap
        // and are reclaimed by sweep itself.
        void destroy_runtime_map(void * /*payload*/) {}

        void register_map_type() {
            auto &reg = gc::type_registry();
            if (reg.find(SID("runtime-map"))) return; // idempotent

            gc::TypeDescriptor td{};
            td.name = SID("runtime-map");
            td.size = sizeof(RuntimeMap);
            td.align = alignof(RuntimeMap);
            td.num_ptr_fields = 0;
            td.flags = gc::TYPE_IS_REFERENCE;
            td.ptr_fields = nullptr;
            td.mark = &mark_runtime_map;
            td.destroy = &destroy_runtime_map;
            reg.register_type(td);
        }

    } // namespace

    // ========================================================================
    // Native functions
    // ========================================================================

    Variant native_make_map(u32 /*argc*/, const Variant * /*argv*/) {
        RuntimeMap *m = make_empty_map();
        if (!m) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        return Variant(static_cast<void *>(m), RuntimeType::Pointer);
    }

    Variant native_map_count(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_ptr()) return Variant(static_cast<i64>(0));
        const auto *m = static_cast<const RuntimeMap *>(argv[0].get_ptr());
        if (!m) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(m->m_size));
    }

    Variant native_map_get(u32 argc, const Variant *argv) {
        if (argc < 2 || !argv[0].is_ptr()) return Variant();
        const auto *m = static_cast<const RuntimeMap *>(argv[0].get_ptr());
        if (!m) return Variant();

        const sid64 key = argv[1].get_sid();
        const i32   idx = find_key(m, key);
        if (idx >= 0) return m->m_pValues[idx];
        if (argc >= 3) return argv[2];
        return Variant();
    }

    Variant native_map_set(u32 argc, const Variant *argv) {
        if (argc < 3 || !argv[0].is_ptr()) return Variant();
        auto *m = argv[0].get_mut_ptr<RuntimeMap>();
        if (!m) return Variant();

        const sid64    key = argv[1].get_sid();
        const Variant &val = argv[2];

        const i32 idx = find_key(m, key);
        if (idx >= 0) {
            m->m_pValues[idx] = val;
            return argv[0];
        }

        if (m->m_size >= m->m_capacity) {
            if (!grow_map(m)) return Variant();
        }

        m->m_pKeys[m->m_size] = key;
        m->m_pValues[m->m_size] = val;
        m->m_size++;
        return argv[0];
    }

    Variant native_map_del(u32 argc, const Variant *argv) {
        if (argc < 2 || !argv[0].is_ptr()) return Variant(false);
        auto *m = argv[0].get_mut_ptr<RuntimeMap>();
        if (!m) return Variant(false);

        const sid64 key = argv[1].get_sid();
        const i32   idx = find_key(m, key);
        if (idx < 0) return Variant(false);

        for (u32 j = static_cast<u32>(idx); j + 1 < m->m_size; ++j) {
            m->m_pKeys[j] = m->m_pKeys[j + 1];
            m->m_pValues[j] = m->m_pValues[j + 1];
        }
        // Clear the tail slot so the GC does not see a stale Variant.
        m->m_pKeys[m->m_size - 1] = 0;
        m->m_pValues[m->m_size - 1] = Variant();
        m->m_size--;
        return Variant(true);
    }

    Variant native_map_has(u32 argc, const Variant *argv) {
        if (argc < 2 || !argv[0].is_ptr()) return Variant(false);
        const auto *m = static_cast<const RuntimeMap *>(argv[0].get_ptr());
        if (!m) return Variant(false);
        return Variant(find_key(m, argv[1].get_sid()) >= 0);
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void NativeMap::initialize_builtins() {
        register_map_type();

        auto &fn = NativeFunctionRegistry::get_instance();
        fn.register_function("make-map", &native_make_map);
        fn.register_function("map-count", &native_map_count);
        fn.register_function("map-get", &native_map_get);
        fn.register_function("map-set", &native_map_set);
        fn.register_function("map-del", &native_map_del);
        fn.register_function("map-has", &native_map_has);

        lg::info("NativeMap: registered 6 map functions");
    }

} // namespace carbon