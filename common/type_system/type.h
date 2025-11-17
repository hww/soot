#pragma once

#include "type_spec.h"
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

// MethodInfo (пока минимальная)
struct MethodInfo {
    int id;
    const char* name;
    TypeSpec* type;
    const char* defined_in_type;
    const char* type_name;
    bool no_virtual;
    bool overrides_parent;
    bool only_overrides_docstring;
    const char* docstring;
    const char* overlay_name;
};

// Базовый тип - максимально минималистичный
struct Type {
    const char* name;
    const char* parent;
    bool is_boxed;
    int heap_base;
    bool allow_in_runtime;
    const char* runtime_name;

    // Существующие виртуальные методы
    bool (*is_reference)(const Type* self);
    int (*get_size_in_memory)(const Type* self);
    int (*get_load_size)(const Type* self);
    bool (*get_load_signed)(const Type* self);
    const char* (*print)(const Type* self);

    // НОВОЕ: Система методов 
    MethodInfo* methods;        // массив методов этого типа
    int method_count;           // количество методов
    MethodInfo new_method;      // специальный метод "new"
    bool new_method_defined;    // определен ли метод "new"

};

// Value types (int, float, etc)
struct ValueType {
    Type base;
    int size;
    int offset;
    bool sign_extend;
    // RegClass reg_kind; // пока опустим
};

// Reference types (базовый для структур)
struct ReferenceType {
    Type base;
    // пока пусто - общие для всех reference types поля
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

// Field structure (пока минимальная)
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


struct BitField {
    // TypeSpec type;  // пока пропустим, добавим когда TypeSpec будет связан с Type
    const char* name;
    int offset;  // в битах
    int size;    // в битах
    bool skip_in_static_decomp;
};

struct BitFieldType {
    ValueType base;
    BitField* fields;
    int field_count;
};

struct EnumType {
    ValueType base;
    bool is_bitfield;
    // позже добавим записи enum
};