#include "gc/TypeRegistry.hpp"
#include "util/Log.hpp"

#include <cstring>
#include <new>

namespace carbon::gc {

    TypeRegistry::~TypeRegistry() { clear(); }

    TypeDescriptor *TypeRegistry::register_type(const TypeDescriptor &td) {
        auto it = m_types.find(td.name);
        if (it != m_types.end()) {
            return it->second; // уже есть
        }

        // Копируем дескриптор.
        auto *copy = new TypeDescriptor(td);
        copy->fields = nullptr; // fields копируются отдельно
        copy->num_fields = 0;

        m_types[td.name] = copy;
        return copy;
    }

    TypeDescriptor *TypeRegistry::register_type_with_fields(const TypeDescriptor &td,
                                                            const FieldDesc      *fields,
                                                            u32                   num_fields) {
        auto it = m_types.find(td.name);
        if (it != m_types.end()) {
            return it->second; // уже есть
        }

        auto *copy = new TypeDescriptor(td);
        copy->fields = nullptr;
        copy->num_fields = 0;

        if (fields && num_fields > 0) {
            auto *arr = new FieldDesc[num_fields];
            std::memcpy(arr, fields, sizeof(FieldDesc) * num_fields);
            m_field_arrays.push_back(arr);
            copy->fields = arr;
            copy->num_fields = num_fields;
        }

        m_types[td.name] = copy;
        return copy;
    }

    TypeDescriptor *TypeRegistry::find(sid64 sid) const noexcept {
        auto it = m_types.find(sid);
        return it != m_types.end() ? it->second : nullptr;
    }

    void TypeRegistry::clear() {
        for (auto &[sid, td] : m_types) { delete td; }
        m_types.clear();

        for (auto *arr : m_field_arrays) { delete[] arr; }
        m_field_arrays.clear();
    }

    TypeRegistry &type_registry() {
        static TypeRegistry registry;
        return registry;
    }

} // namespace carbon::gc