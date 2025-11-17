// test_type_system.cpp
#include <gtest/gtest.h>
#include "common/type_system/export.h"
#include "fmt/format.h"

class TypeSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = type_system_create();
        type_system_initialize_builtin_types(ts);
    }

    void TearDown() override {
        type_system_destroy(ts);
    }

    TypeSystem* ts;
};

// Базовые тесты системы типов
TEST_F(TypeSystemTest, BasicTypeCreation) {
    EXPECT_TRUE(type_system_has_type(ts, "object"));
    EXPECT_TRUE(type_system_has_type(ts, "int"));
    EXPECT_TRUE(type_system_has_type(ts, "float"));

    Type* int_type = type_system_lookup(ts, "int");
    ASSERT_NE(int_type, nullptr);
    EXPECT_STREQ(int_type->name, "int");
    EXPECT_STREQ(int_type->parent, "object");
}

TEST_F(TypeSystemTest, ValueTypeOperations) {
    ValueType* custom_type = type_system_create_valuetype(ts, "custom-int", "int", 8, true);
    ASSERT_NE(custom_type, nullptr);

    EXPECT_FALSE(custom_type->base.is_reference(&custom_type->base));
    EXPECT_EQ(custom_type->base.get_size_in_memory(&custom_type->base), 8);
    EXPECT_TRUE(custom_type->base.get_load_signed(&custom_type->base));
}

TEST_F(TypeSystemTest, StructureTypeCreation) {
    StructureType* vector_type = type_system_create_structuretype(
        ts, "vector", "structure", false, false, false, 0);
    ASSERT_NE(vector_type, nullptr);

    EXPECT_TRUE(vector_type->base.base.is_reference(&vector_type->base.base));
    EXPECT_EQ(vector_type->field_count, 0);
    EXPECT_EQ(vector_type->size_in_mem, 0);
}

TEST_F(TypeSystemTest, FieldManagement) {
    StructureType* vector_type = type_system_create_structuretype(
        ts, "test-vector", "structure", false, false, false, 0);

    TypeSpec* float_spec = type_system_make_typespec(ts, "float");

    // Добавляем поля
    type_system_add_field_to_structure(ts, vector_type, "x", float_spec, false, false, -1, 0);
    type_system_add_field_to_structure(ts, vector_type, "y", float_spec, false, false, -1, 4);
    type_system_add_field_to_structure(ts, vector_type, "z", float_spec, false, false, -1, 8);

    EXPECT_EQ(vector_type->field_count, 3);
    EXPECT_EQ(vector_type->size_in_mem, 12);

    // Проверяем поиск полей
    Field* field_x = type_system_lookup_field(ts, "test-vector", "x");
    ASSERT_NE(field_x, nullptr);
    EXPECT_STREQ(field_x->name, "x");
    EXPECT_EQ(field_x->offset, 0);

    Field* field_y = type_system_lookup_field(ts, "test-vector", "y");
    ASSERT_NE(field_y, nullptr);
    EXPECT_EQ(field_y->offset, 4);

    type_spec_destroy(float_spec);
}

TEST_F(TypeSystemTest, Inheritance) {
    // Создаем родительскую структуру
    StructureType* game_object_type = type_system_create_structuretype(
        ts, "game-object", "structure", false, false, false, 0);

    TypeSpec* int_spec = type_system_make_typespec(ts, "int");
    TypeSpec* string_spec = type_system_make_typespec(ts, "string");

    type_system_add_field_to_structure(ts, game_object_type, "id", int_spec, false, false, -1, 0);
    type_system_add_field_to_structure(ts, game_object_type, "name", string_spec, false, false, -1, 4);

    // Создаем дочернюю структуру - должна унаследовать поля
    StructureType* player_type = type_system_create_structuretype(
        ts, "player", "game-object", false, false, false, 0);

    type_system_add_field_to_structure(ts, player_type, "health", int_spec, false, false, -1, -1);
    type_system_add_field_to_structure(ts, player_type, "score", int_spec, false, false, -1, -1);

    // Проверяем наследование
    EXPECT_GE(player_type->field_count, 4); // как минимум 2 родительских + 2 дочерних
    EXPECT_GT(player_type->size_in_mem, game_object_type->size_in_mem);

    // Проверяем что можем найти унаследованные поля
    Field* inherited_field = type_system_lookup_field(ts, "player", "id");
    ASSERT_NE(inherited_field, nullptr);
    EXPECT_STREQ(inherited_field->name, "id");

    type_spec_destroy(int_spec);
    type_spec_destroy(string_spec);
}

TEST_F(TypeSystemTest, MethodSystem) {
    ValueType* test_type = type_system_create_valuetype(ts, "method-test", "object", 4, false);

    TypeSpec* method_spec = type_spec_create("function");

    // Объявляем метод
    MethodInfo* method = type_system_declare_method(ts, (Type*)test_type, "test-method", method_spec, false);
    ASSERT_NE(method, nullptr);

    EXPECT_STREQ(method->name, "test-method");
    EXPECT_EQ(method->id, 1);
    EXPECT_FALSE(method->no_virtual);

    // Проверяем что метод добавлен в тип
    EXPECT_EQ(test_type->base.method_count, 1);
    EXPECT_NE(test_type->base.methods, nullptr);

    // Ищем метод
    MethodInfo found_method;
    bool found = type_system_lookup_method(ts, "method-test", "test-method", &found_method);
    EXPECT_TRUE(found);
    EXPECT_STREQ(found_method.name, "test-method");

    type_spec_destroy(method_spec);
}

TEST_F(TypeSystemTest, TypeChecking) {
    TypeSpec* object_spec = type_system_make_typespec(ts, "object");
    TypeSpec* int_spec = type_system_make_typespec(ts, "int");
    TypeSpec* float_spec = type_system_make_typespec(ts, "float");

    // int наследует от object
    EXPECT_TRUE(type_system_typecheck(ts, object_spec, int_spec));

    // int равен int
    EXPECT_TRUE(type_system_typecheck(ts, int_spec, int_spec));

    // int не является float
    EXPECT_FALSE(type_system_typecheck(ts, int_spec, float_spec));

    type_spec_destroy(object_spec);
    type_spec_destroy(int_spec);
    type_spec_destroy(float_spec);
}

TEST_F(TypeSystemTest, LowestCommonAncestor) {
    TypeSpec* int_spec = type_system_make_typespec(ts, "int");
    TypeSpec* float_spec = type_system_make_typespec(ts, "float");

    // LCA int и int = int
    TypeSpec* lca_same = type_system_lowest_common_ancestor(ts, int_spec, int_spec);
    ASSERT_NE(lca_same, nullptr);
    EXPECT_STREQ(lca_same->base_type, "int");

    // LCA int и float = object
    TypeSpec* lca_diff = type_system_lowest_common_ancestor(ts, int_spec, float_spec);
    ASSERT_NE(lca_diff, nullptr);
    EXPECT_STREQ(lca_diff->base_type, "object");

    type_spec_destroy(int_spec);
    type_spec_destroy(float_spec);
    type_spec_destroy(lca_same);
    type_spec_destroy(lca_diff);
}

TEST_F(TypeSystemTest, BitFieldType) {
    BitFieldType* flags_type = type_system_create_bitfieldtype(ts, "test-flags", "bitfield", 4, false);
    ASSERT_NE(flags_type, nullptr);

    TypeSpec* int_spec = type_system_make_typespec(ts, "int");

    type_system_add_field_to_bitfield(ts, flags_type, "flag1", int_spec, 0, 1);
    type_system_add_field_to_bitfield(ts, flags_type, "flag2", int_spec, 1, 2);

    EXPECT_EQ(flags_type->field_count, 2);
    EXPECT_FALSE(flags_type->base.base.is_reference(&flags_type->base.base));

    type_spec_destroy(int_spec);
}

TEST_F(TypeSystemTest, EnumType) {
    EnumType* color_enum = type_system_create_enumtype(ts, "test-color", "int", false);
    ASSERT_NE(color_enum, nullptr);

    EXPECT_FALSE(color_enum->base.base.is_reference(&color_enum->base.base));
    EXPECT_EQ(color_enum->base.base.get_size_in_memory(&color_enum->base.base), 4);
    EXPECT_FALSE(color_enum->is_bitfield);

    EnumType* flags_enum = type_system_create_enumtype(ts, "test-flags", "int", true);
    ASSERT_NE(flags_enum, nullptr);
    EXPECT_TRUE(flags_enum->is_bitfield);
}

TEST_F(TypeSystemTest, ReverseFieldLookup) {
    // Создаем тестовую структуру
    StructureType* test_struct = type_system_create_structuretype(
        ts, "reverse-test", "structure", false, false, false, 0);

    TypeSpec* int_spec = type_system_make_typespec(ts, "int");
    type_system_add_field_to_structure(ts, test_struct, "field1", int_spec, false, false, -1, 0);
    type_system_add_field_to_structure(ts, test_struct, "field2", int_spec, false, false, -1, 4);

    // Тестируем обратный поиск
    FieldReverseLookupInput input;
    input.offset = 4;
    input.base_type = type_system_make_typespec(ts, "reverse-test");
    input.include_parents = false;

    FieldReverseLookupOutput output = type_system_reverse_lookup_field(ts, input);

    EXPECT_TRUE(output.success);
    EXPECT_STREQ(output.field_name, "field2");
    EXPECT_EQ(output.offset, 4);

    type_spec_destroy(int_spec);
    type_spec_destroy(input.base_type);
}

TEST_F(TypeSystemTest, ArrayFields) {
    StructureType* array_type = type_system_create_structuretype(
        ts, "test-array", "structure", false, false, false, 0);

    TypeSpec* float_spec = type_system_make_typespec(ts, "float");

    // Добавляем массивное поле
    type_system_add_field_to_structure(ts, array_type, "data", float_spec, false, false, 10, 0);

    EXPECT_EQ(array_type->field_count, 1);
    EXPECT_EQ(array_type->size_in_mem, 40); // 10 * 4 bytes

    Field* data_field = type_system_lookup_field(ts, "test-array", "data");
    ASSERT_NE(data_field, nullptr);
    EXPECT_TRUE(data_field->array);
    EXPECT_EQ(data_field->array_size, 10);

    type_spec_destroy(float_spec);
}

TEST_F(TypeSystemTest, TypeSpecOperations) {
    // Тестируем базовые операции TypeSpec
    TypeSpec* base_spec = type_spec_create("object");
    ASSERT_NE(base_spec, nullptr);
    EXPECT_STREQ(base_spec->base_type, "object");
    EXPECT_TRUE(type_spec_is_empty(base_spec));

    // Добавляем аргументы
    TypeSpec* arg1 = type_spec_create("int");
    type_spec_add_arg(base_spec, arg1);

    EXPECT_FALSE(type_spec_is_empty(base_spec));
    EXPECT_TRUE(type_spec_has_single_arg(base_spec));

    // Клонирование
    TypeSpec* cloned = type_spec_clone(base_spec);
    ASSERT_NE(cloned, nullptr);
    EXPECT_STREQ(cloned->base_type, "object");
    EXPECT_EQ(cloned->arg_count, 1);

    type_spec_destroy(base_spec);
    type_spec_destroy(cloned);
}

TEST_F(TypeSystemTest, VirtualMethodInheritance) {
    // Создаем иерархию типов с методами
    StructureType* base_type = type_system_create_structuretype(
        ts, "base-type", "structure", false, false, false, 0);

    TypeSpec* base_method_spec = type_spec_create("function");
    type_spec_add_arg(base_method_spec, type_system_make_typespec(ts, "base-type"));
    type_spec_add_arg(base_method_spec, type_system_make_typespec(ts, "none"));

    MethodInfo* base_method = type_system_declare_method(ts, (Type*)base_type,
        "base-method", base_method_spec, false);

    // Создаем дочерний тип
    StructureType* derived_type = type_system_create_structuretype(
        ts, "derived-type", "base-type", false, false, false, 0);

    // Должны найти метод родителя
    MethodInfo found_method;
    bool found = type_system_lookup_method(ts, "derived-type", "base-method", &found_method);
    EXPECT_TRUE(found);
    EXPECT_STREQ(found_method.name, "base-method");

    type_spec_destroy(base_method_spec);
}

// Тест для проверки обработки ошибок
TEST_F(TypeSystemTest, ErrorConditions) {
    // Поиск несуществующего типа
    Type* non_existent = type_system_lookup(ts, "non-existent-type");
    EXPECT_EQ(non_existent, nullptr);

    // Поиск несуществующего поля
    Field* non_existent_field = type_system_lookup_field(ts, "int", "non-existent-field");
    EXPECT_EQ(non_existent_field, nullptr);

    // Поиск несуществующего метода
    MethodInfo method_info;
    bool found = type_system_lookup_method(ts, "int", "non-existent-method", &method_info);
    EXPECT_FALSE(found);
}

// Тест производительности для больших структур
TEST_F(TypeSystemTest, PerformanceTest) {
    StructureType* large_struct = type_system_create_structuretype(
        ts, "large-struct", "structure", false, false, false, 0);

    TypeSpec* int_spec = type_system_make_typespec(ts, "int");

    // Добавляем поля и сразу проверяем их наличие в структуре
    for (int i = 0; i < 10; i++) {
        std::string field_name = fmt::format("field_{}", i);
        type_system_add_field_to_structure(ts, large_struct, field_name.c_str(),
            int_spec, false, false, -1, -1);

        // НЕМЕДЛЕННАЯ ПРОВЕРКА: смотрим прямо в структуру
        bool field_exists_in_memory = false;
        for (int j = 0; j < large_struct->field_count; j++) {
            if (strcmp(large_struct->fields[j].name, field_name.c_str()) == 0) {
                field_exists_in_memory = true;
                break;
            }
        }
        EXPECT_TRUE(field_exists_in_memory) << "Field " << field_name << " should be in memory structure";
    }

    // Теперь проверяем через lookup
    for (int i = 0; i < 10; i++) {
        std::string field_name = fmt::format("field_{}", i);
        Field* field = type_system_lookup_field(ts, "large-struct", field_name.c_str());

        // ДИАГНОСТИКА: если поле не найдено, смотрим что вообще есть в структуре
        if (field == nullptr) {
            fmt::print("DEBUG: Field '{}' not found via lookup. Structure has {} fields:\n",
                field_name, large_struct->field_count);
            for (int j = 0; j < large_struct->field_count; j++) {
                fmt::print("  [{}] '{}' (offset: {})\n",
                    j, large_struct->fields[j].name, large_struct->fields[j].offset);
            }
        }

        EXPECT_NE(field, nullptr) << "Field " << field_name << " should be found via lookup";
        if (field) {
            EXPECT_STREQ(field->name, field_name.c_str());
        }
    }

    type_spec_destroy(int_spec);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}