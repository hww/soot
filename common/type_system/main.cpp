#include "fmt/format.h"
#include "type.h"
#include "type_system.h"
#include "type_spec.h"

int main() {
    // НАЧАЛЬНЫЙ ТЕСТ МЕТОДОВ
    fmt::print("=== НАЧАЛО ТЕСТА МЕТОДОВ ===\n");
    TypeSystem* ts = type_system_create();
    type_system_initialize_builtin_types(ts);

    ValueType* test_type = type_system_create_valuetype(ts, "test-type", "object", 4, false);
    TypeSpec* method_spec = type_spec_create("function");

    MethodInfo* method = type_system_declare_method(ts, (Type*)test_type, "test-method", method_spec, false);

    if (method) {
        fmt::print("✓ МЕТОД РАБОТАЕТ: {} id: {}\n", method->name, method->id);
    }

    type_spec_destroy(method_spec);
    fmt::print("=== КОНЕЦ НАЧАЛЬНОГО ТЕСТА ===\n\n");

    // СУЩЕСТВУЮЩИЙ КОД создания vector_type
    fmt::print("=== Тестируем систему полей ===\n");
    StructureType* vector_type = type_system_create_structuretype(ts, "vector", "structure", false, false, false, 0);
    // ... остальной код создания vector_type

    // ТЕПЕРЬ тесты методов ПОСЛЕ создания vector_type
    fmt::print("\n=== ТЕСТ МЕТОДОВ ШАГ 2 ===\n");

    TypeSpec* update_method_type = type_spec_create("function");
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "vector"));
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "float"));
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "none"));

    MethodInfo* update_method = type_system_declare_method(ts, (Type*)vector_type, "update", update_method_type, false);

    if (update_method) {
        fmt::print("✓ Метод объявлен: {} id: {}\n", update_method->name, update_method->id);
    }

    // ============================================================================
    // Тестируем наследование полей
    // ============================================================================
    fmt::print("\n=== Тестируем наследование полей ===\n");
    TypeSpec* int_spec = type_system_make_typespec(ts, "int");
    // Создаем родительскую структуру
    StructureType* game_object_type = type_system_create_structuretype(ts, "game-object", "structure", false, false, false, 0);
    type_system_add_field_to_structure(ts, game_object_type, "id", int_spec, false, false, -1, 0);
    type_system_add_field_to_structure(ts, game_object_type, "name", type_system_make_typespec(ts, "string"), false, false, -1, 4);

    fmt::print("✓ Родитель 'game-object' создан с {} полями\n", game_object_type->field_count);
    fmt::print("✓ Размер родителя: {} байт\n", game_object_type->size_in_mem);

    // Создаем дочернюю структуру (автоматически наследует поля)
    StructureType* player_type = type_system_create_structuretype(ts, "player", "game-object", false, false, false, 0);
    type_system_add_field_to_structure(ts, player_type, "health", int_spec, false, false, -1, -1); // авто-размещение
    type_system_add_field_to_structure(ts, player_type, "score", int_spec, false, false, -1, -1);  // авто-размещение

    fmt::print("✓ Дочерний 'player' создан с {} полями\n", player_type->field_count);
    fmt::print("✓ Размер дочернего: {} байт\n", player_type->size_in_mem);

    // Выводим все поля player (родительские + дочерние)
    fmt::print("✓ Все поля 'player':\n");
    for (int i = 0; i < player_type->field_count; i++) {
        Field* field = &player_type->fields[i];
        const char* source = (i < player_type->idx_of_first_unique_field) ? "наследовано" : "уникальное";
        fmt::print("  - {} (смещение: {}) [{}]\n", field->name, field->offset, source);
    }

    // Проверяем поиск унаследованных полей
    Field* inherited_field = type_system_lookup_field(ts, "player", "id");
    if (inherited_field) {
        fmt::print("✓ Найдено унаследованное поле: {} (смещение: {})\n", inherited_field->name, inherited_field->offset);
    }

    type_spec_destroy(update_method_type);

    // ============================================================================
    // Тестируем Type Checking
    // ============================================================================
    fmt::print("\n=== Тестируем Type Checking ===\n");

    // Создаем TypeSpec для тестирования
    TypeSpec* object_spec = type_system_make_typespec(ts, "object");
    TypeSpec* int_spec2 = type_system_make_typespec(ts, "int");
    TypeSpec* float_spec2 = type_system_make_typespec(ts, "float");
    TypeSpec* vector_spec2 = type_system_make_typespec(ts, "vector");

    // Тестируем проверку типов
    fmt::print("✓ int <: object = {}\n", type_system_typecheck(ts, object_spec, int_spec2));
    fmt::print("✓ vector <: object = {}\n", type_system_typecheck(ts, object_spec, vector_spec2));
    fmt::print("✓ int <: int = {}\n", type_system_typecheck(ts, int_spec2, int_spec2));
    fmt::print("✓ int <: float = {}\n", type_system_typecheck(ts, int_spec2, float_spec2));

    // Тестируем LCA
    TypeSpec* lca1 = type_system_lowest_common_ancestor(ts, int_spec2, float_spec2);
    fmt::print("✓ LCA(int, float) = {}\n", lca1->base_type);

    TypeSpec* lca2 = type_system_lowest_common_ancestor(ts, int_spec2, int_spec2);
    fmt::print("✓ LCA(int, int) = {}\n", lca2->base_type);

    // ============================================================================
    // Тестируем Reverse Field Lookup
    // ============================================================================
    fmt::print("\n=== Тестируем Reverse Field Lookup ===\n");

    // Тестируем поиск по точному смещению
    FieldReverseLookupInput lookup_input;
    lookup_input.offset = 4;  // поле 'y' в vector
    lookup_input.base_type = type_system_make_typespec(ts, "vector");
    lookup_input.include_parents = true;

    FieldReverseLookupOutput lookup_result = type_system_reverse_lookup_field(ts, lookup_input);
    if (!lookup_input.base_type) {
        fmt::print("✗ Не удалось создать TypeSpec для 'vector'\n");
    }
    else {
        lookup_input.include_parents = true;
    }
    if (lookup_result.success) {
        fmt::print("✓ Найдено поле по смещению {}: {} (массив: {})\n",
            lookup_input.offset, lookup_result.field_name, lookup_result.is_array);
    }
    else {
        fmt::print("✗ Поле по смещению {} не найдено\n", lookup_input.offset);
    }

    // Тестируем поиск в массиве
    FieldReverseLookupInput array_lookup_input;
    array_lookup_input.offset = 8;  // третий элемент в массиве data[10]
    array_lookup_input.base_type = type_system_make_typespec(ts, "float-array");
    array_lookup_input.include_parents = false;
    if (!array_lookup_input.base_type) {
        fmt::print("✗ Не удалось создать TypeSpec для 'vector'\n");
    }
    else {
        array_lookup_input.include_parents = true;
    }
    FieldReverseLookupOutput array_lookup_result = type_system_reverse_lookup_field(ts, array_lookup_input);

    if (array_lookup_result.success) {
        fmt::print("✓ Найдено поле по смещению {}: {} (массив: {})\n",
            array_lookup_input.offset, array_lookup_result.field_name, array_lookup_result.is_array);
    }
    else {
        fmt::print("✗ Поле по смещению {} не найдено\n", array_lookup_input.offset);
    }

    // Тестируем поиск унаследованного поля
    FieldReverseLookupInput inherited_lookup_input;
    inherited_lookup_input.offset = 0;  // поле 'id' унаследованное от game-object
    inherited_lookup_input.base_type = type_system_make_typespec(ts, "player");
    inherited_lookup_input.include_parents = true;
    if (!inherited_lookup_input.base_type) {
        fmt::print("✗ Не удалось создать TypeSpec для 'vector'\n");
    }
    else {
        inherited_lookup_input.include_parents = true;
    }
    FieldReverseLookupOutput inherited_lookup_result = type_system_reverse_lookup_field(ts, inherited_lookup_input);

    if (inherited_lookup_result.success) {
        fmt::print("✓ Найдено унаследованное поле по смещению {}: {}\n",
            inherited_lookup_input.offset, inherited_lookup_result.field_name);
    }
    else {
        fmt::print("✗ Унаследованное поле по смещению {} не найдено\n", inherited_lookup_input.offset);
    }

    // Очистка
    type_spec_destroy(lookup_input.base_type);
    type_spec_destroy(array_lookup_input.base_type);
    type_spec_destroy(inherited_lookup_input.base_type);
    type_spec_destroy(object_spec);
    type_spec_destroy(int_spec2);
    type_spec_destroy(float_spec2);
    type_spec_destroy(vector_spec2);
    type_spec_destroy(lca1);
    type_spec_destroy(lca2);
}
int aaa(){

    
    fmt::print("=== Тестируем систему полей ===\n");

    TypeSystem* ts = type_system_create();
    type_system_initialize_builtin_types(ts);






    // Создаем структуру и добавляем поля
    StructureType* vector_type = type_system_create_structuretype(ts, "vector", "structure", false, false, false, 0);

    TypeSpec* float_spec = type_system_make_typespec(ts, "float");

    // Добавляем поля к вектору
    type_system_add_field_to_structure(ts, vector_type, "x", float_spec, false, false, -1, 0);
    type_system_add_field_to_structure(ts, vector_type, "y", float_spec, false, false, -1, 4);
    type_system_add_field_to_structure(ts, vector_type, "z", float_spec, false, false, -1, 8);

    fmt::print("✓ Структура 'vector' создана с {} полями\n", vector_type->field_count);
    fmt::print("✓ Размер структуры: {} байт\n", vector_type->size_in_mem);

    // Выводим информацию о полях (ИСПРАВЛЕНО)
    for (int i = 0; i < vector_type->field_count; i++) {
        Field* field = &vector_type->fields[i];  // берем адрес элемента массива структур
        fmt::print("  - Поле: {} (смещение: {})\n", field->name, field->offset);
    }

    // Тестируем поиск полей
    Field* found_field = type_system_lookup_field(ts, "vector", "y");
    if (found_field) {
        fmt::print("✓ Найдено поле: {} (смещение: {})\n", found_field->name, found_field->offset);
    }

    fmt::print("=== Тестируем виртуальные методы ===\n");

    Type* int_type = type_system_lookup(ts, "int");
    if (int_type) {
        fmt::print("✓ int.is_reference() = {}\n", int_type->is_reference(int_type));
        fmt::print("✓ int.get_size_in_memory() = {}\n", int_type->get_size_in_memory(int_type));
    }

    Type* vector_type_obj = type_system_lookup(ts, "vector");

    if (vector_type_obj) {
        fmt::print("✓ int.is_reference() = {}\n", int_type->is_reference(int_type));
        fmt::print("✓ int.get_size_in_memory() = {}\n", int_type->get_size_in_memory(int_type));
        if (vector_type_obj && vector_type_obj->is_reference) {
            fmt::print("✓ vector.is_reference() = {}\n", vector_type_obj->is_reference(vector_type_obj));
        }
        if (vector_type_obj && vector_type_obj->get_size_in_memory) {
            fmt::print("✓ vector.get_size_in_memory() = {}\n", vector_type_obj->get_size_in_memory(vector_type_obj));
        }
        if (vector_type_obj && vector_type_obj->print) {
            fmt::print("✓ vector.print() = {}\n", vector_type_obj->print(vector_type_obj));
        }
    }

    // Создаем массив
    StructureType* array_type = type_system_create_structuretype(ts, "float-array", "structure", false, false, false, 0);
    type_system_add_field_to_structure(ts, array_type, "data", float_spec, false, false, 10, 0);

    fmt::print("✓ Массив создан с полем data[10] (размер: {} байт)\n", array_type->size_in_mem);


    // ============================================================================
   // Тестируем BitField и Enum
   // ============================================================================
    fmt::print("\n=== Тестируем BitField и Enum ===\n");

    // Создаем битфилд (например, для флагов)
    BitFieldType* flags_type = type_system_create_bitfieldtype(ts, "flags", "bitfield", 4, false);

    TypeSpec* int_spec = type_system_make_typespec(ts, "int");

    // Добавляем битовые поля
    type_system_add_field_to_bitfield(ts, flags_type, "is_visible", int_spec, 0, 1);   // бит 0
    type_system_add_field_to_bitfield(ts, flags_type, "is_active", int_spec, 1, 1);    // бит 1  
    type_system_add_field_to_bitfield(ts, flags_type, "type", int_spec, 2, 3);         // биты 2-4 (3 бита)
    type_system_add_field_to_bitfield(ts, flags_type, "reserved", int_spec, 5, 27);    // биты 5-31

    fmt::print("✓ BitField 'flags' создан с {} полями\n", flags_type->field_count);

    // Выводим информацию о полях битфилда
    for (int i = 0; i < flags_type->field_count; i++) {
        BitField* field = &flags_type->fields[i];
        fmt::print("  - Бит поле: {} (смещение: {} бит, размер: {} бит)\n",
            field->name, field->offset, field->size);
    }

    // Тестируем виртуальные методы BitField
    Type* flags_type_obj = type_system_lookup(ts, "flags");
    if (flags_type_obj && flags_type_obj->print) {
        fmt::print("✓ flags.print() = {}\n", flags_type_obj->print(flags_type_obj));
    }

    // Создаем Enum
    EnumType* color_enum = type_system_create_enumtype(ts, "color", "int", false);

    // Тестируем виртуальные методы Enum
    Type* color_enum_obj = type_system_lookup(ts, "color");
    if (color_enum_obj && color_enum_obj->print) {
        fmt::print("✓ color.print() = {}\n", color_enum_obj->print(color_enum_obj));
    }

    // Создаем битфилд-Enum (для комбинаций флагов)
    EnumType* permission_enum = type_system_create_enumtype(ts, "permission", "int", true);
    Type* permission_enum_obj = type_system_lookup(ts, "permission");
    if (permission_enum_obj && permission_enum_obj->print) {
        fmt::print("✓ permission.print() = {}\n", permission_enum_obj->print(permission_enum_obj));
    }
    // ============================================================================
    // Тестируем систему методов
    // ============================================================================
    /*
    fmt::print("\n=== Тестируем систему методов ===\n");

    // Создаем function type для методов
    TypeSpec* update_method_type = type_spec_create("function");
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "vector")); // this
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "float"));  // dt
    type_spec_add_arg(update_method_type, type_system_make_typespec(ts, "none"));   // return

    TypeSpec* draw_method_type = type_spec_create("function");
    type_spec_add_arg(draw_method_type, type_system_make_typespec(ts, "vector")); // this
    type_spec_add_arg(draw_method_type, type_system_make_typespec(ts, "none"));   // return

    // Объявляем методы для vector типа
    MethodInfo* update_method = type_system_declare_method(ts, (Type*)vector_type,
        "update", update_method_type, false);
    MethodInfo* draw_method = type_system_declare_method(ts, (Type*)vector_type,
        "draw", draw_method_type, false);

    fmt::print("✓ Объявлен метод: {} (id: {})\n", update_method->name, update_method->id);
    fmt::print("✓ Объявлен метод: {} (id: {})\n", draw_method->name, draw_method->id);

    // Добавляем метод new для vector
    TypeSpec* new_method_type = type_spec_create("function");
    type_spec_add_arg(new_method_type, type_system_make_typespec(ts, "symbol")); // type
    type_spec_add_arg(new_method_type, type_system_make_typespec(ts, "type"));   // type
    type_spec_add_arg(new_method_type, type_system_make_typespec(ts, "vector")); // return

    MethodInfo* new_method = type_system_add_new_method(ts, (Type*)vector_type,
        new_method_type, "Constructor");

    fmt::print("✓ Добавлен метод new: {} (id: {})\n", new_method->name, new_method->id);

    // Тестируем поиск методов
    MethodInfo found_method;
    if (type_system_lookup_method(ts, "vector", "update", &found_method)) {
        fmt::print("✓ Найден метод: {} (id: {})\n", found_method.name, found_method.id);
    }

    if (type_system_lookup_method(ts, "vector", "new", &found_method)) {
        fmt::print("✓ Найден метод new: {} (id: {})\n", found_method.name, found_method.id);
    }

    // Выводим все методы vector
    Type* vector_type_obj1 = type_system_lookup(ts, "vector");
    fmt::print("✓ Тип 'vector' имеет {} методов:\n", vector_type_obj1->method_count);
    for (int i = 0; i < vector_type_obj->method_count; i++) {
        fmt::print("  - {} (id: {})\n",
            vector_type_obj1->methods[i].name,
            vector_type_obj1->methods[i].id);
    }

    fmt::print("\n=== Быстрый тест методов ===\n");

    // Просто объявляем один метод и проверяем
    TypeSpec* simple_method_type = type_spec_create("function");
    type_spec_add_arg(simple_method_type, type_system_make_typespec(ts, "vector"));
    type_spec_add_arg(simple_method_type, type_system_make_typespec(ts, "none"));

    MethodInfo* test_method = type_system_declare_method(ts, (Type*)vector_type, "test", simple_method_type, false);

    if (test_method) {
        fmt::print("✓ Метод объявлен: {} (id: {})\n", test_method->name, test_method->id);
        fmt::print("✓ Vector теперь имеет {} методов\n", ((Type*)vector_type)->method_count);
    }
    type_spec_destroy(simple_method_type);
    type_spec_destroy(update_method_type);
    type_spec_destroy(draw_method_type);
    type_spec_destroy(new_method_type);
    */
    // ============================================================================
    fmt::print("\n=== МИНИМАЛЬНЫЙ ТЕСТ МЕТОДОВ ===\n");

    // 1. Просто проверяем что Type имеет поля методов
    Type* test_type_obj = type_system_lookup(ts, "vector");
    if (test_type_obj) {
        fmt::print("✓ Vector type найден\n");
        fmt::print("  - method_count: {}\n", test_type_obj->method_count);
        fmt::print("  - methods pointer: {}\n", (void*)test_type_obj->methods);
    }
    else {
        fmt::print("✗ Vector type НЕ найден\n");
    }

    // 2. Пробуем создать простой TypeSpec
    TypeSpec* func_type = type_spec_create("function");
    if (func_type) {
        fmt::print("✓ TypeSpec создан\n");
        type_spec_destroy(func_type);
    }
    else {
        fmt::print("✗ TypeSpec НЕ создан\n");
    }

    fmt::print("=== КОНЕЦ МИНИМАЛЬНОГО ТЕСТА ===\n");

    // Очистка

    type_spec_destroy(int_spec);
    type_spec_destroy(float_spec);
    type_system_destroy(ts);

    fmt::print("=== Система полей готова ===\n");
    return 0;
}