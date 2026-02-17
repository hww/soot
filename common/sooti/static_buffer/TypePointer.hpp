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
    Object                      get() override;
    void                        set(const Object &val) override;
    Object                      make_step_accessor(const Object &key) override;
    void                       *resolve_addr() const override;
    std::string                 print() const override;
    Object                      inspect() const override;
    Type                       *get_type();
    std::shared_ptr<HeapObject> get_owner() const {
        return m_owner;
    }

    size_t get_offset_in_buffer() const;

    std::string full_class_name() const override {
        return "TypePointer";
    }

    std::string class_name() const override {
        return "type-pointer";
    }

    bool is_class_name(std::string name) const override {
        return name == TypePointer::class_name() || Pointer::is_class_name(name);
    }
};
} // namespace script
