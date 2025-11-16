#include "type_system.h"
#include <cstdlib>
#include <cstring>
#include <stdio.h>

// ============================================================================
// Конструктор деструктор системы
// ============================================================================

TypeSystem* type_system_create() {
    TypeSystem* ts = new TypeSystem();
    ts->type_count = 0;
    for (int i = 0; i < MAX_TYPES; i++) {
        ts->types[i] = nullptr;
    }
    return ts;
}

void type_system_destroy(TypeSystem* ts) {
    delete ts;
}

// ============================================================================
// Реализации списка типов
// ============================================================================

Type* type_system_add_type(TypeSystem* ts, Type* type) {
    if (ts->type_count >= MAX_TYPES) return nullptr;

    // Простая проверка на дубликаты
    for (int i = 0; i < ts->type_count; i++) {
        if (std::strcmp(ts->types[i]->name, type->name) == 0) {
            return ts->types[i]; // уже существует
        }
    }

    ts->types[ts->type_count++] = type;
    return type;
}

Type* type_system_lookup(TypeSystem* ts, const char* name) {
    for (int i = 0; i < ts->type_count; i++) {
        if (std::strcmp(ts->types[i]->name, name) == 0) {
            return ts->types[i];
        }
    }
    return nullptr;
}

bool type_system_has_type(TypeSystem* ts, const char* name) {
    return type_system_lookup(ts, name) != nullptr;
}



// ============================================================================
// Реализации методов TypeSpec
// ============================================================================

TypeSpec* type_system_make_typespec(TypeSystem* ts, const char* name) {
    // Проверяем, что тип существует
    if (!type_system_has_type(ts, name)) {
        return nullptr; // или можно создать "forward declared" тип
    }
    return type_spec_create(name);
}

TypeSpec* type_system_make_pointer_typespec(TypeSystem* ts, TypeSpec* element_type) {
    TypeSpec* pointer_ts = type_spec_create("pointer");
    type_spec_add_arg(pointer_ts, type_spec_clone(element_type));  // КЛОНИРУЕМ!
    return pointer_ts;
}

TypeSpec* type_system_make_inline_array_typespec(TypeSystem* ts, TypeSpec* element_type) {
    TypeSpec* array_ts = type_spec_create("inline-array");
    type_spec_add_arg(array_ts, type_spec_clone(element_type));  // КЛОНИРУЕМ!
    return array_ts;
}


// ============================================================================
// Реализации виртуальных методов для ValueType
// ============================================================================

bool value_type_is_reference(const Type* self) {
    return false; // Value types are not references
}

int value_type_get_size_in_memory(const Type* self) {
    const ValueType* value_type = (const ValueType*)self;
    return value_type->size;
}

int value_type_get_load_size(const Type* self) {
    const ValueType* value_type = (const ValueType*)self;
    return value_type->size;
}

bool value_type_get_load_signed(const Type* self) {
    const ValueType* value_type = (const ValueType*)self;
    return value_type->sign_extend;
}

const char* value_type_print(const Type* self) {
    static char buffer[256];
    const ValueType* value_type = (const ValueType*)self;
    snprintf(buffer, sizeof(buffer), "[ValueType] %s (size: %d, signed: %s)",
        self->name, value_type->size, value_type->sign_extend ? "true" : "false");
    return buffer;
}

// ============================================================================
// Реализации для BasicType (наследуют StructureType)
// ============================================================================

bool basic_type_is_reference(const Type* self) {
    return true; // Basic types are references (inherit from StructureType)
}

int basic_type_get_size_in_memory(const Type* self) {
    const BasicType* basic_type = (const BasicType*)self;
    return basic_type->base.size_in_mem;
}

int basic_type_get_load_size(const Type* self) {
    return 4; // Basic types are loaded as pointers
}

bool basic_type_get_load_signed(const Type* self) {
    return false; // Pointers are not sign-extended
}

const char* basic_type_print(const Type* self) {
    const BasicType* basic_type = (const BasicType*)self;
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "[BasicType] %s (size: %d, final: %s)",
        self->name, basic_type->base.size_in_mem, basic_type->final ? "true" : "false");
    return buffer;
}

// ============================================================================
// Фабрики типов
// ============================================================================

ValueType* type_system_create_valuetype(TypeSystem* ts,
    const char* name,
    const char* parent,
    int size,
    bool sign_extend) {
    ValueType* type = new ValueType();
    type->base.name = name;
    type->base.parent = parent;
    type->base.is_boxed = false;
    type->base.heap_base = 0;
    type->base.allow_in_runtime = true;
    type->base.runtime_name = name;

    type->size = size;
    type->offset = 0;
    type->sign_extend = sign_extend;


    // Устанавливаем виртуальные методы
    type->base.is_reference = value_type_is_reference;
    type->base.get_size_in_memory = value_type_get_size_in_memory;
    type->base.get_load_size = value_type_get_load_size;
    type->base.get_load_signed = value_type_get_load_signed;
    type->base.print = value_type_print;

    type_system_add_type(ts, (Type*)type);
    return type;
}

StructureType* type_system_create_structuretype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool is_boxed,
    bool dynamic,
    bool pack,
    int heap_base) {
    StructureType* type = new StructureType();
    type->base.base.name = name;
    type->base.base.parent = parent;
    type->base.base.is_boxed = is_boxed;
    type->base.base.heap_base = heap_base;
    type->base.base.allow_in_runtime = true;
    type->base.base.runtime_name = name;

    type->fields = nullptr;
    type->field_count = 0;
    type->size_in_mem = 0;
    type->dynamic = dynamic;
    type->pack = pack;
    type->allow_misalign = false;
    type->offset = 0;
    type->always_stack_singleton = false;
    type->idx_of_first_unique_field = 0;

    // Устанавливаем виртуальные методы
    type->base.base.is_reference = structure_type_is_reference;
    type->base.base.get_size_in_memory = structure_type_get_size_in_memory;
    type->base.base.get_load_size = structure_type_get_load_size;
    type->base.base.get_load_signed = structure_type_get_load_signed;
    type->base.base.print = structure_type_print;

    type_system_add_type(ts, (Type*)type);
    return type;
}

BasicType* type_system_create_basictype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool dynamic,
    int heap_base) {
    BasicType* type = new BasicType();
    type->base.base.base.name = name;
    type->base.base.base.parent = parent;
    type->base.base.base.is_boxed = true;
    type->base.base.base.heap_base = heap_base;
    type->base.base.base.allow_in_runtime = true;
    type->base.base.base.runtime_name = name;

    type->base.fields = nullptr;
    type->base.field_count = 0;
    type->base.size_in_mem = 0;
    type->base.dynamic = dynamic;
    type->base.pack = false;
    type->base.allow_misalign = false;
    type->base.offset = 0;
    type->base.always_stack_singleton = false;
    type->base.idx_of_first_unique_field = 0;

    type->final = false;

    // Устанавливаем виртуальные методы для BasicType
    type->base.base.base.is_reference = basic_type_is_reference;
    type->base.base.base.get_size_in_memory = basic_type_get_size_in_memory;
    type->base.base.base.get_load_size = basic_type_get_load_size;
    type->base.base.base.get_load_signed = basic_type_get_load_signed;
    type->base.base.base.print = basic_type_print;

    type_system_add_type(ts, (Type*)type);
    return type;
}


// ============================================================================
// Fields
// ============================================================================

void type_system_add_field_to_structure(TypeSystem* ts,
    StructureType* structure,
    const char* field_name,
    TypeSpec* field_type,
    bool is_inline,
    bool is_dynamic,
    int array_size,
    int offset_override) {
    // Простой расчет смещения (пока без сложного выравнивания)
    int offset = offset_override;
    if (offset == -1) {
        // Автоматическое размещение - после последнего поля
        offset = structure->size_in_mem;
    }

    // Создаем поле как структуру
    Field field;
    field.name = field_name;
    // field.type = field_type;  // пока пропустим
    field.offset = offset;
    field.inline_ = is_inline;
    field.dynamic = is_dynamic;
    field.array = (array_size != -1);
    field.array_size = array_size;
    field.alignment = 4;
    field.skip_in_static_decomp = false;

    // Увеличиваем размер структуры
    int field_size = 4;  // упрощенный расчет размера
    if (array_size != -1) {
        field_size = array_size * 4;
    }

    structure->size_in_mem = offset + field_size;

    // Добавляем поле в структуру (Field* fields - массив структур)
    Field* new_fields = new Field[structure->field_count + 1];
    for (int i = 0; i < structure->field_count; i++) {
        new_fields[i] = structure->fields[i];  // копируем существующие структуры
    }
    new_fields[structure->field_count] = field;  // добавляем новую структуру

    delete[] structure->fields;
    structure->fields = new_fields;
    structure->field_count++;
}


// Поиск полей
Field* type_system_lookup_field(TypeSystem* ts,
    const char* type_name,
    const char* field_name) {
    Type* type = type_system_lookup(ts, type_name);
    if (!type) return nullptr;

    // Пока ищем только в StructureType
    StructureType* structure = (StructureType*)type;
    for (int i = 0; i < structure->field_count; i++) {
        if (strcmp(structure->fields[i].name, field_name) == 0) {
            return &structure->fields[i];
        }
    }

    return nullptr;
}

// ============================================================================
// Встроенные типы
// ============================================================================

void type_system_initialize_builtin_types(TypeSystem* ts) {
    // Создаем базовые встроенные типы
    type_system_create_valuetype(ts, "object", "", 4, false);
    type_system_create_valuetype(ts, "int", "object", 4, true);
    type_system_create_valuetype(ts, "float", "object", 4, false);
    type_system_create_structuretype(ts, "structure", "object", false, false, false, 0);
    type_system_create_basictype(ts, "basic", "structure", false, 0);
}
