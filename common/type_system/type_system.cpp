#include "type_system.h"
#include "fmt/format.h"
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
// Реализации для Structure Type
// ============================================================================

bool structure_type_is_reference(const Type* self) {
    return true; // Structure types are references
}

int structure_type_get_size_in_memory(const Type* self) {
    const StructureType* structure_type = (const StructureType*)self;
    return structure_type->size_in_mem;
}

int structure_type_get_load_size(const Type* self) {
    return 4; // Structures are loaded as pointers (4 bytes)
}

bool structure_type_get_load_signed(const Type* self) {
    return false; // Pointers are not sign-extended
}

const char* structure_type_print(const Type* self) {
    const StructureType* structure_type = (const StructureType*)self;
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "[StructureType] %s (size: %d, fields: %d)",
        self->name, structure_type->size_in_mem, structure_type->field_count);
    return buffer;
}

// ============================================================================
// Реализации виртуальных методов для BitFieldType
// ============================================================================

bool bitfield_type_is_reference(const Type* self) {
    return false; // BitField types are value types
}

int bitfield_type_get_size_in_memory(const Type* self) {
    const BitFieldType* bitfield_type = (const BitFieldType*)self;
    return bitfield_type->base.size; // наследуем размер от ValueType
}

int bitfield_type_get_load_size(const Type* self) {
    const BitFieldType* bitfield_type = (const BitFieldType*)self;
    return bitfield_type->base.size;
}

bool bitfield_type_get_load_signed(const Type* self) {
    const BitFieldType* bitfield_type = (const BitFieldType*)self;
    return bitfield_type->base.sign_extend;
}

const char* bitfield_type_print(const Type* self) {
    const BitFieldType* bitfield_type = (const BitFieldType*)self;
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "[BitFieldType] %s (size: %d bytes, fields: %d)",
        self->name, bitfield_type->base.size, bitfield_type->field_count);
    return buffer;
}

// ============================================================================
// Реализации виртуальных методов для EnumType
// ============================================================================

bool enum_type_is_reference(const Type* self) {
    return false; // Enum types are value types
}

int enum_type_get_size_in_memory(const Type* self) {
    const EnumType* enum_type = (const EnumType*)self;
    return enum_type->base.size;
}

int enum_type_get_load_size(const Type* self) {
    const EnumType* enum_type = (const EnumType*)self;
    return enum_type->base.size;
}

bool enum_type_get_load_signed(const Type* self) {
    const EnumType* enum_type = (const EnumType*)self;
    return enum_type->base.sign_extend;
}

const char* enum_type_print(const Type* self) {
    const EnumType* enum_type = (const EnumType*)self;
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "[EnumType] %s (size: %d, bitfield: %s)",
        self->name, enum_type->base.size, enum_type->is_bitfield ? "true" : "false");
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

    // Инициализацию полей методов
    type->base.methods = nullptr;
    type->base.method_count = 0;
    type->base.new_method_defined = false;

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

    // Инициализацию полей методов
    type->base.base.methods = nullptr;
    type->base.base.method_count = 0;
    type->base.base.new_method_defined = false;

    // Устанавливаем виртуальные методы
    type->base.base.is_reference = structure_type_is_reference;
    type->base.base.get_size_in_memory = structure_type_get_size_in_memory;
    type->base.base.get_load_size = structure_type_get_load_size;
    type->base.base.get_load_signed = structure_type_get_load_signed;
    type->base.base.print = structure_type_print;

    // Наследование полей   
    if (parent && strlen(parent) > 0) {
        Type* parent_type = type_system_lookup(ts, parent);
        if (parent_type) {
            StructureType* parent_structure = (StructureType*)parent_type;
            type_system_inherit_fields(type, parent_structure);
        }
    }

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

    // Инициализацию полей методов
    type->base.base.base.methods = nullptr;
    type->base.base.base.method_count = 0;
    type->base.base.base.new_method_defined = false;

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
// Фабрика BitFieldType
// ============================================================================

BitFieldType* type_system_create_bitfieldtype(TypeSystem* ts,
    const char* name,
    const char* parent,
    int size,
    bool sign_extend) {
    BitFieldType* type = new BitFieldType();

    // Инициализируем базовый ValueType
    type->base.base.name = name;
    type->base.base.parent = parent;
    type->base.base.is_boxed = false;
    type->base.base.heap_base = 0;
    type->base.base.allow_in_runtime = true;
    type->base.base.runtime_name = name;

    type->base.size = size;
    type->base.offset = 0;
    type->base.sign_extend = sign_extend;

    // Инициализируем специфичные для BitField поля
    type->fields = nullptr;
    type->field_count = 0;

    // Инициализацию полей методов
    type->base.base.methods = nullptr;
    type->base.base.method_count = 0;
    type->base.base.new_method_defined = false;

    // Устанавливаем виртуальные методы
    type->base.base.is_reference = bitfield_type_is_reference;
    type->base.base.get_size_in_memory = bitfield_type_get_size_in_memory;
    type->base.base.get_load_size = bitfield_type_get_load_size;
    type->base.base.get_load_signed = bitfield_type_get_load_signed;
    type->base.base.print = bitfield_type_print;

    type_system_add_type(ts, (Type*)type);
    return type;
}

// ============================================================================
// Фабрика EnumType
// ============================================================================

EnumType* type_system_create_enumtype(TypeSystem* ts,
    const char* name,
    const char* parent,
    bool is_bitfield) {

    EnumType* type = new EnumType();

    // УБЕДИСЬ что имя устанавливается правильно:
    type->base.base.name = strdup(name);  // КОПИРУЕМ строку!
    type->base.base.parent = parent;
    type->base.base.is_boxed = false;
    type->base.base.heap_base = 0;
    type->base.base.allow_in_runtime = true;
    type->base.base.runtime_name = strdup(name);  // Тоже копируем!

    // Наследуем от существующего типа
    Type* parent_type = type_system_lookup(ts, parent);
    if (!parent_type) {
        delete type;
        return nullptr;
    }

    ValueType* parent_value_type = (ValueType*)parent_type;
    type->base.size = parent_value_type->size;
    type->base.offset = 0;
    type->base.sign_extend = parent_value_type->sign_extend;

    // Специфичные для Enum поля
    type->is_bitfield = is_bitfield;

    // Инициализация методов
    type->base.base.methods = nullptr;
    type->base.base.method_count = 0;
    type->base.base.new_method_defined = false;

    // Устанавливаем виртуальные методы
    type->base.base.is_reference = enum_type_is_reference;
    type->base.base.get_size_in_memory = enum_type_get_size_in_memory;
    type->base.base.get_load_size = enum_type_get_load_size;
    type->base.base.get_load_signed = enum_type_get_load_signed;
    type->base.base.print = enum_type_print;

    fmt::print("DEBUG: Created enum '{}' with parent '{}'\n", name, parent);

    type_system_add_type(ts, (Type*)type);
    return type;
}


// ============================================================================
// Добавление полей в BitField
// ============================================================================

void type_system_add_field_to_bitfield(TypeSystem* ts,
    BitFieldType* bitfield,
    const char* field_name,
    TypeSpec* field_type,
    int offset,  // в битах!
    int size) {  // в битах!

    // Создаем поле битфилда
    BitField field;
    field.name = field_name;
    // field.type = field_type; // пока пропустим
    field.offset = offset;
    field.size = size;
    field.skip_in_static_decomp = false;

    // Добавляем поле в битфилд
    BitField* new_fields = new BitField[bitfield->field_count + 1];
    for (int i = 0; i < bitfield->field_count; i++) {
        new_fields[i] = bitfield->fields[i];
    }
    new_fields[bitfield->field_count] = field;

    delete[] bitfield->fields;
    bitfield->fields = new_fields;
    bitfield->field_count++;
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
    field.name = strdup(field_name);
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
// Система методов 
// ============================================================================

int type_system_get_next_method_id(TypeSystem* ts, const Type* type) {
    // Пока возвращаем просто количество методов + 1
    // Позже добавим логику наследования
    return type->method_count + 1;
}

MethodInfo* type_system_declare_method(TypeSystem* ts,
    Type* type,
    const char* method_name,
    TypeSpec* method_type,
    bool no_virtual) {

    // Проверяем, не объявлен ли метод уже
    MethodInfo existing;
    if (type_system_get_my_method(type, method_name, &existing)) {
        // Метод уже существует - возвращаем существующий
        return &type->methods[existing.id - 1];
    }

    // Создаем новый метод
    MethodInfo new_method;
    new_method.id = type_system_get_next_method_id(ts, type);
    new_method.name = method_name;
    new_method.type = method_type;
    new_method.defined_in_type = type->name;
    new_method.type_name = type->name;
    new_method.no_virtual = no_virtual;
    new_method.overrides_parent = false;
    new_method.only_overrides_docstring = false;
    new_method.docstring = nullptr;
    new_method.overlay_name = nullptr;

    // Добавляем в массив методов типа
    MethodInfo* new_methods = new MethodInfo[type->method_count + 1];
    for (int i = 0; i < type->method_count; i++) {
        new_methods[i] = type->methods[i];
    }
    new_methods[type->method_count] = new_method;

    delete[] type->methods;
    type->methods = new_methods;
    type->method_count++;

    return &type->methods[type->method_count - 1];
}

bool type_system_get_my_method(const Type* type,
    const char* method_name,
    MethodInfo* out) {

    for (int i = 0; i < type->method_count; i++) {
        if (strcmp(type->methods[i].name, method_name) == 0) {
            if (out) *out = type->methods[i];
            return true;
        }
    }
    return false;
}

bool type_system_lookup_method(TypeSystem* ts,
    const char* type_name,
    const char* method_name,
    MethodInfo* out) {

    Type* type = type_system_lookup(ts, type_name);
    if (!type) return false;

    // Ищем в текущем типе
    if (type_system_get_my_method(type, method_name, out)) {
        return true;
    }

    // Ищем в родительских типах (пока простое наследование)
    if (type->parent && strlen(type->parent) > 0) {
        return type_system_lookup_method(ts, type->parent, method_name, out);
    }

    return false;
}

MethodInfo* type_system_add_new_method(TypeSystem* ts,
    Type* type,
    TypeSpec* method_type,
    const char* docstring) {

    type->new_method_defined = true;
    type->new_method.id = 0; // специальный ID для new
    type->new_method.name = "new";
    type->new_method.type = method_type;
    type->new_method.defined_in_type = type->name;
    type->new_method.type_name = type->name;
    type->new_method.no_virtual = false;
    type->new_method.overrides_parent = false;
    type->new_method.only_overrides_docstring = false;
    type->new_method.docstring = docstring;
    type->new_method.overlay_name = nullptr;

    return &type->new_method;
}
// ============================================================================
// Наследование полей
// ============================================================================

// ============================================================================
// Наследование полей
// ============================================================================
void type_system_inherit_fields(StructureType* child, StructureType* parent) {
    if (!parent || parent->field_count == 0) {
        return;
    }

    // Создаем новый массив полей: родительские + дочерние
    int total_fields = parent->field_count + child->field_count;
    Field* inherited_fields = new Field[total_fields];

    // Копируем поля родителя (без изменений)
    for (int i = 0; i < parent->field_count; i++) {
        inherited_fields[i] = parent->fields[i];
    }

    // Копируем поля ребенка со смещением
    for (int i = 0; i < child->field_count; i++) {
        inherited_fields[parent->field_count + i] = child->fields[i];
        inherited_fields[parent->field_count + i].offset += parent->size_in_mem;
    }

    // Заменяем поля ребенка
    delete[] child->fields;
    child->fields = inherited_fields;
    child->field_count = total_fields;
    child->size_in_mem = parent->size_in_mem + child->size_in_mem;

    // Запоминаем где начинаются уникальные поля ребенка
    child->idx_of_first_unique_field = parent->field_count;
}
/*
void type_system_inherit_fields(StructureType* child, StructureType* parent) {
    if (!parent || parent->field_count == 0) {
        return; // Нечего наследовать
    }
    fmt::print("  Наследование: копируем {} полей от {}\n",
        parent->field_count, parent->base.base.name);
    // Создаем новый массив полей: родительские + дочерние
    int total_fields = parent->field_count + child->field_count;
    Field* inherited_fields = new Field[total_fields];

    // Копируем поля родителя (без изменений)
    for (int i = 0; i < parent->field_count; i++) {
        inherited_fields[i] = parent->fields[i];
    }

    // Копируем поля ребенка со смещением
    for (int i = 0; i < child->field_count; i++) {
        inherited_fields[parent->field_count + i] = child->fields[i];
        inherited_fields[parent->field_count + i].offset += parent->size_in_mem;
    }

    // Заменяем поля ребенка
    delete[] child->fields;
    child->fields = inherited_fields;
    child->field_count = total_fields;
    child->size_in_mem = parent->size_in_mem + child->size_in_mem;

    // Запоминаем где начинаются уникальные поля ребенка
    child->idx_of_first_unique_field = parent->field_count;
    fmt::print("  Наследование: итого {} полей, размер: {}\n",
        child->field_count, child->size_in_mem);
}
*/

// ============================================================================
// Type Checking
// ============================================================================

bool type_system_typecheck(TypeSystem* ts, const TypeSpec* expected, const TypeSpec* actual) {
    // Базовая проверка - типы равны
    if (strcmp(expected->base_type, actual->base_type) == 0) {
        return true;
    }

    // Проверка наследования - actual является потомком expected
    Type* actual_type = type_system_lookup(ts, actual->base_type);
    while (actual_type && actual_type->parent) {
        if (strcmp(actual_type->parent, expected->base_type) == 0) {
            return true;
        }
        actual_type = type_system_lookup(ts, actual_type->parent);
    }

    return false;
}

TypeSpec* type_system_lowest_common_ancestor(TypeSystem* ts, const TypeSpec* a, const TypeSpec* b) {
    // Пока простая реализация - возвращаем "object" для разных типов
    if (strcmp(a->base_type, b->base_type) == 0) {
        return type_spec_clone(a);
    }

    // TODO: Найти реального общего предка через иерархию типов
    return type_system_make_typespec(ts, "object");
}

// ============================================================================
// Reverse Field Lookup - поиск поля по смещению
// ============================================================================

FieldReverseLookupOutput type_system_reverse_lookup_field(TypeSystem* ts,
    FieldReverseLookupInput input) {

    FieldReverseLookupOutput result = { 0 };
    result.success = false;

    // ПРОВЕРЯЕМ что base_type не NULL
    if (!input.base_type) {
        return result;
    }

    Type* type = type_system_lookup(ts, input.base_type->base_type);
    if (!type) return result;

    // Пока работаем только со StructureType
    StructureType* structure = (StructureType*)type;

    // Ищем поле по точному совпадению смещения
    for (int i = 0; i < structure->field_count; i++) {
        Field* field = &structure->fields[i];

        if (field->offset == input.offset) {
            // Точное совпадение
            result.field_name = field->name;
            result.offset = field->offset;
            result.is_array = field->array;
            result.success = true;
            return result;
        }

        // Проверяем если поле массив и смещение внутри него
        if (field->array && !field->dynamic) {
            int field_size = field->array_size * 4; // упрощенный расчет
            if (input.offset >= field->offset &&
                input.offset < field->offset + field_size) {
                // Смещение внутри массива
                result.field_name = field->name;
                result.offset = field->offset;
                result.is_array = true;
                result.success = true;
                return result;
            }
        }
    }

    // Если не нашли и нужно искать в родителях
    if (input.include_parents && type->parent && strlen(type->parent) > 0) {
        FieldReverseLookupInput parent_input = input;
        parent_input.base_type = type_system_make_typespec(ts, type->parent);

        // ПРОВЕРЯЕМ что type_system_make_typespec не вернул NULL
        if (parent_input.base_type) {
            FieldReverseLookupOutput parent_result = type_system_reverse_lookup_field(ts, parent_input);
            type_spec_destroy(parent_input.base_type);

            if (parent_result.success) {
                return parent_result;
            }
        }
    }

    return result;
}

// ============================================================================
// 
// ============================================================================

// ============================================================================
// Встроенные типы
// ============================================================================
void type_system_initialize_builtin_types(TypeSystem* ts) {
    // Базовые типы
    type_system_create_valuetype(ts, "object", "", 4, false);

    // Числовая иерархия 
    type_system_create_valuetype(ts, "number", "object", 8, false);
    type_system_create_valuetype(ts, "float", "number", 4, false);

    // Integer hierarchy - ВСЕ наследуем от object или number
    type_system_create_valuetype(ts, "integer", "number", 8, true);
    type_system_create_valuetype(ts, "sinteger", "integer", 8, true);
    type_system_create_valuetype(ts, "int8", "sinteger", 1, true);
    type_system_create_valuetype(ts, "int16", "sinteger", 2, true);
    type_system_create_valuetype(ts, "int32", "sinteger", 4, true);
    type_system_create_valuetype(ts, "int64", "sinteger", 8, true);

    type_system_create_valuetype(ts, "uinteger", "integer", 8, false);
    type_system_create_valuetype(ts, "uint8", "uinteger", 1, false);
    type_system_create_valuetype(ts, "uint16", "uinteger", 2, false);
    type_system_create_valuetype(ts, "uint32", "uinteger", 4, false);
    type_system_create_valuetype(ts, "uint64", "uinteger", 8, false);

    // Псевдонимы для обратной совместимости - наследуем от object
    type_system_create_valuetype(ts, "int", "object", 4, true);

    // Остальные типы
    type_system_create_basictype(ts, "string", "basic", false, 0);
    type_system_create_structuretype(ts, "structure", "object", false, false, false, 0);
    type_system_create_basictype(ts, "basic", "structure", false, 0);
    type_system_create_bitfieldtype(ts, "bitfield", "object", 4, false);
    type_system_create_enumtype(ts, "enum", "int", false);
}