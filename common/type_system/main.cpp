#include "fmt/format.h"
#include "type.h"
#include "type_system.h"
#include "type_spec.h"

int main() {
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

    type_spec_destroy(float_spec);
    type_system_destroy(ts);

    fmt::print("=== Система полей готова ===\n");
    return 0;
}