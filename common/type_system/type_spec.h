#pragma once

#include <cstddef>

struct TypeTag {
    const char* name;
    const char* value;
};

struct TypeSpec {
    const char* base_type;
    TypeSpec** arguments;
    size_t arg_count;
    TypeTag* tags;
    size_t tag_count;
};

// Базовые операции с TypeSpec
TypeSpec* type_spec_create(const char* base_type);
void type_spec_destroy(TypeSpec* ts);

void type_spec_add_arg(TypeSpec* ts, TypeSpec* arg);
void type_spec_add_tag(TypeSpec* ts, const char* name, const char* value);

// Утилиты
bool type_spec_has_single_arg(const TypeSpec* ts);
const TypeSpec* type_spec_get_single_arg(const TypeSpec* ts);
bool type_spec_is_empty(const TypeSpec* ts);
TypeSpec* type_spec_clone(const TypeSpec* src);