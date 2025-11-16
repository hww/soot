#pragma once

#include "type.h"
#include "type_spec.h"

#define MAX_TYPES 256

typedef struct TypeSystem TypeSystem;

struct TypeSystem {
    Type* types[MAX_TYPES];
    int type_count;
};

// ============================================================================
// Жизненый цикл
// ============================================================================

TypeSystem* type_system_create();
void type_system_destroy(TypeSystem* ts);

// ============================================================================
// Базовые операции
// ============================================================================

Type* type_system_add_type(TypeSystem* ts, Type* type);
Type* type_system_lookup(TypeSystem* ts, const char* name);
bool type_system_has_type(TypeSystem* ts, const char* name);

// ============================================================================
// Type Spec 
// ============================================================================

TypeSpec* type_system_make_typespec(TypeSystem* ts, const char* name);
TypeSpec* type_system_make_pointer_typespec(TypeSystem* ts, TypeSpec* element_type);
TypeSpec* type_system_make_inline_array_typespec(TypeSystem* ts, TypeSpec* element_type);

// ============================================================================
// Фабрики типов
// ============================================================================

ValueType* type_system_create_valuetype(TypeSystem* ts,
    const char* name,
    const char* parent,
    int size,
    bool sign_extend);

StructureType* type_system_create_structuretype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool is_boxed,
    bool dynamic,
    bool pack,
    int heap_base);

BasicType* type_system_create_basictype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool dynamic,
    int heap_base);

// Базовые встроенные типы
void type_system_initialize_builtin_types(TypeSystem* ts);

BitFieldType* type_system_create_bitfieldtype(TypeSystem* ts,
    const char* name,
    const char* parent,
    int size,
    bool sign_extend);

EnumType* type_system_create_enumtype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool is_bitfield);

void type_system_add_field_to_bitfield(TypeSystem* ts,
    BitFieldType* bitfield,
    const char* field_name,
    TypeSpec* field_type,
    int offset,  // в битах!
    int size);   // в битах!

// ============================================================================
// Система полей
// ============================================================================
 
Field* field_create(const char* name, TypeSpec* type, int offset);

void field_destroy(Field* field);

void type_system_add_field_to_structure(TypeSystem* ts,
    StructureType* structure,
    const char* field_name,
    TypeSpec* field_type,
    bool is_inline,
    bool is_dynamic,
    int array_size,
    int offset_override);

// Поиск полей
Field* type_system_lookup_field(TypeSystem* ts,
    const char* type_name,
    const char* field_name);


// ============================================================================
//  полей
// ============================================================================

