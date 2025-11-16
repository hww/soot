#pragma once

#include <cstdbool>
#include <cstddef>

// Forward declarations
struct Type;
struct ValueType;
struct ReferenceType;
struct StructureType;
struct BasicType;
struct BitFieldType;
struct EnumType;
struct Field;
struct MethodInfo;

// Ѕазовый тип - максимально минималистичный
struct Type {
    const char* name;
    const char* parent;
    bool is_boxed;
    int heap_base;
    bool allow_in_runtime;
    const char* runtime_name;

    bool (*is_reference)(const Type* self);
    int (*get_size_in_memory)(const Type* self);
    int (*get_load_size)(const Type* self);
    bool (*get_load_signed)(const Type* self);
    const char* (*print)(const Type* self);
};

// Value types (int, float, etc)
struct ValueType {
    Type base;
    int size;
    int offset;
    bool sign_extend;
    // RegClass reg_kind; // пока опустим
};

// Reference types (базовый дл€ структур)
struct ReferenceType {
    Type base;
    // пока пусто - общие дл€ всех reference types пол€
};

// Structure types
struct StructureType {
    ReferenceType base;
    Field* fields;
    int field_count;
    int size_in_mem;
    bool dynamic;
    bool pack;
    bool allow_misalign;
    int offset;
    bool always_stack_singleton;
    size_t idx_of_first_unique_field;
};

// Basic types (наследуют StructureType)
struct BasicType {
    StructureType base;
    bool final;
};

// ѕока заглушки дл€ остальных типов
struct BitFieldType {
    ValueType base;
    // позже добавим BitField* fields
};

struct EnumType {
    ValueType base;
    bool is_bitfield;
    // позже добавим entries
};

// Field structure (пока минимальна€)
struct Field {
    const char* name;
    // TypeSpec type; // позже
    int offset;
    bool inline_;
    bool dynamic;
    bool array;
    int array_size;
    int alignment;
    bool skip_in_static_decomp;
};

// MethodInfo (пока минимальна€)
struct MethodInfo {
    int id;
    const char* name;
    // TypeSpec type; // позже
    const char* defined_in_type;
    bool no_virtual;
    bool overrides_parent;
};