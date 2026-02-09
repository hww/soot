#pragma once

#include <fmt/format.h>

#include "common/sooti/Object.hpp"

class Type;

namespace script {

class TypePointer : public Pointer {
  public:
    std::shared_ptr<HeapObject> m_owner;
    TypePointer() : Pointer(), m_owner() {}

    TypePointer(void *ptr, std::string type, std::shared_ptr<HeapObject> owner)
        : Pointer(ptr, type), m_owner(owner) {}

    TypePointer(void *ptr, Type *type, std::shared_ptr<HeapObject> owner);
    Object      get() override;
    void        set(const Object &val) override;
    Object      make_step_accessor(const Object &key) override;
    void       *resolve_addr() const override;
    std::string print() const override;
    Object      inspect() const override;
    Type       *get_type();
};
} // namespace script
