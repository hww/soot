#pragma once

#include <fmt/format.h>

#include "StaticBuffer.hpp"
#include "common/sooti/Object.hpp"

namespace script {

class TypeCell : public MemoryCell {
  public:
    std::shared_ptr<HeapObject> m_owner;
    Object m_key;
    Type *m_type;
    std::string m_path;

    TypeCell() : MemoryCell(), m_owner(), m_key(), m_type(nullptr), m_path() {}

    TypeCell(void *ptr, Type *type, std::shared_ptr<HeapObject> owner = nullptr, Object key = {},
             std::string path = "")
        : MemoryCell(ptr), m_owner(owner), m_key(key), m_type(type), m_path(path) {
        this->m_kind = MemoryAccessKind::CUSTOM;
    }

    std::string get_type_name() const override {
        return m_type ? m_type->get_name() : "nullptr";
    }
    void *resolve_ptr() const;

    Type *get_type() const {
        return m_type;
    }

    Object get() override;
    void set(const Object &val) override;

    Object make_step_accessor(const Object &key) override;

    std::string print() const override;
    Object inspect() const override;
};
} // namespace script
