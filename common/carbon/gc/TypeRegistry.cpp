#include "gc/TypeRegistry.hpp"
#include "util/Log.hpp"

#include <cstring>

namespace carbon::gc {

    TypeRegistry::~TypeRegistry() { clear(); }

    u32 TypeRegistry::register_type(const TypeDescriptor &td) {
        // Slot 0 is the sentinel.
        if (m_types.empty()) {
            TypeDescriptor sentinel{};
            sentinel.name = 0;
            sentinel.size = 0;
            sentinel.align = 1;
            sentinel.num_ptr_fields = 0;
            sentinel.flags = 0;
            sentinel.ptr_fields = nullptr;
            sentinel.mark = nullptr;
            sentinel.destroy = nullptr;
            m_types.push_back(sentinel);
            m_ptr_storage.emplace_back();
        }

        // Idempotent: if SID already registered, return its index.
        auto it = m_by_sid.find(td.name);
        if (it != m_by_sid.end()) { return it->second; }

        TypeDescriptor copy = td;

        if (td.ptr_fields && td.num_ptr_fields > 0) {
            std::vector<PtrField> storage(td.ptr_fields, td.ptr_fields + td.num_ptr_fields);
            m_ptr_storage.push_back(std::move(storage));
            copy.ptr_fields = m_ptr_storage.back().data();
        } else {
            m_ptr_storage.emplace_back();
            copy.ptr_fields = nullptr;
            copy.num_ptr_fields = 0;
        }

        const u32 index = static_cast<u32>(m_types.size());
        m_types.push_back(copy);
        m_by_sid[td.name] = index;

        lg::debug("TypeRegistry: registered type 0x{:08X} at index {}", td.name, index);
        return index;
    }

    TypeDescriptor *TypeRegistry::find_by_index(u32 index) const noexcept {
        if (index == 0 || index >= m_types.size()) return nullptr;
        return const_cast<TypeDescriptor *>(&m_types[index]);
    }

    TypeDescriptor *TypeRegistry::find(sid64 sid) const noexcept {
        auto it = m_by_sid.find(sid);
        if (it == m_by_sid.end()) return nullptr;
        return const_cast<TypeDescriptor *>(&m_types[it->second]);
    }

    u32 TypeRegistry::find_index(sid64 sid) const noexcept {
        auto it = m_by_sid.find(sid);
        return it == m_by_sid.end() ? 0u : it->second;
    }

    void TypeRegistry::clear() {
        m_types.clear();
        m_ptr_storage.clear();
        m_by_sid.clear();
    }

    TypeRegistry &type_registry() {
        static TypeRegistry registry;
        return registry;
    }

} // namespace carbon::gc