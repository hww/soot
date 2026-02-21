#pragma once
#include "common/sooti/Object.hpp"
#include "common/type_system/TypeSpec.hpp"
#include <string>

namespace script {
struct Register : NativeObject {
    Object name;           // Имя переменной
    Object reg;            // Ссылка на физический регистр
    Object type_name;      // Имя типа
    int    offset = 0;     // Смещение в БАЙТАХ
    int    bit_offset = 0; // Смещение в БИТАХ (0-7) внутри байта
    int    bit_size = 0;   // Размер поля в БИТАХ (если это битфилд)
    int    arg_index;      // Используется в rlet для порядка

    Register() : name(Object::make_none()), reg(Object::make_none()), offset(0) {}

    std::string full_class_name() const override {
        return "Register";
    }
    std::string class_name() const override {
        return "register";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == Register::type_name_obj() || NativeObject::is_class_name(name);
    }

    Object get_at(const Object &key) override;

    std::string print() const override;
    Object      inspect() const override;
};

} // namespace script