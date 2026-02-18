#pragma once
#include "common/sooti/Object.hpp"
#include "common/type_system/TypeSpec.hpp"
#include <string>

namespace script {
struct RegisterAlias : NativeObject {
    Object name;           // Имя переменной
    Object reg;            // Ссылка на физический регистр
    Object type_name;      // Имя типа
    int    offset = 0;     // Смещение в БАЙТАХ
    int    bit_offset = 0; // Смещение в БИТАХ (0-7) внутри байта
    int    bit_size = 0;   // Размер поля в БИТАХ (если это битфилд)

    RegisterAlias() : name(Object::make_none()), reg(Object::make_none()), offset(0) {}

    std::string full_class_name() const override {
        return "RegisterAlias";
    }
    std::string class_name() const override {
        return "reg-alias";
    }

    bool is_class_name(std::string name) const override {
        return name == class_name() || NativeObject::is_class_name(name);
    }

    Object make_step_accessor(const Object &key) override;

    std::string print() const override;
    Object      inspect() const override;
};

} // namespace script