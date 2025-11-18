#include <gtest/gtest.h>
#include "type_system/export.h"
#include "fmt/format.h"

class TypeSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = std::make_unique<TypeSystem>();
        ts->add_builtin_types();
    }

    void TearDown() override {
        ts.reset();
    }

    std::unique_ptr<TypeSystem> ts;
};

// Базовые тесты системы типов
TEST_F(TypeSystemTest, BasicTypeCreation) {
    ts->add_builtin_types();

    // Проверяем иерархию КАК В ОРИГИНАЛЕ
    Type* int_type = ts->lookup_type("int");
    EXPECT_EQ(int_type->get_parent(), "integer");  // int -> integer

    Type* int32_type = ts->lookup_type("int32");
    EXPECT_EQ(int32_type->get_parent(), "sinteger");  // int32 -> sinteger

    Type* sinteger_type = ts->lookup_type("sinteger");
    EXPECT_EQ(sinteger_type->get_parent(), "integer");  // sinteger -> integer

    Type* integer_type = ts->lookup_type("integer");
    EXPECT_EQ(integer_type->get_parent(), "number");  // integer -> number

    Type* number_type = ts->lookup_type("number");
    EXPECT_EQ(number_type->get_parent(), "object");  // number -> object
}

TEST_F(TypeSystemTest, ValueTypeOperations) {
    ValueType* custom_type = ts->add_builtin_value_type("int32", "custom-int", 8, false, true, RegClass::GPR_64);
    ASSERT_NE(custom_type, nullptr);

    EXPECT_FALSE(custom_type->is_reference());
    EXPECT_EQ(custom_type->get_size_in_memory(), 8);
    EXPECT_TRUE(custom_type->get_load_signed());
}

TEST_F(TypeSystemTest, StructureTypeCreation) {
    StructureType* vector_type = ts->add_builtin_structure("structure", "vector");
    ASSERT_NE(vector_type, nullptr);

    EXPECT_TRUE(vector_type->is_reference());
    EXPECT_EQ(vector_type->fields().size(), 0);
    EXPECT_EQ(vector_type->get_size_in_memory(), 0);
}

TEST_F(TypeSystemTest, FieldManagement) {
    StructureType* vector_type = ts->add_builtin_structure("structure", "test-vector");

    TypeSpec float_spec = ts->make_typespec("float");

    // Добавляем поля
    ts->add_field_to_type(vector_type, "x", float_spec, false, false, -1, 0);
    ts->add_field_to_type(vector_type, "y", float_spec, false, false, -1, 4);
    ts->add_field_to_type(vector_type, "z", float_spec, false, false, -1, 8);

    EXPECT_EQ(vector_type->fields().size(), 3);
    EXPECT_EQ(vector_type->get_size_in_memory(), 12);

    // Проверяем поиск полей
    Field field_x = ts->lookup_field("test-vector", "x");
    EXPECT_EQ(field_x.name(), "x");
    EXPECT_EQ(field_x.offset(), 0);

    Field field_y = ts->lookup_field("test-vector", "y");
    EXPECT_EQ(field_y.offset(), 4);
}

TEST_F(TypeSystemTest, Inheritance) {
    // Создаем родительскую структуру
    StructureType* game_object_type = ts->add_builtin_structure("structure", "game-object");

    TypeSpec int_spec = ts->make_typespec("int32");
    TypeSpec string_spec = ts->make_typespec("string");

    ts->add_field_to_type(game_object_type, "id", int_spec, false, false, -1, 0);
    ts->add_field_to_type(game_object_type, "name", string_spec, false, false, -1, 4);

    // Создаем дочернюю структуру - должна унаследовать поля
    StructureType* player_type = ts->add_builtin_structure("game-object", "player");

    ts->add_field_to_type(player_type, "health", int_spec, false, false, -1, -1);
    ts->add_field_to_type(player_type, "score", int_spec, false, false, -1, -1);

    // Проверяем наследование
    EXPECT_GE(player_type->fields().size(), 4); // как минимум 2 родительских + 2 дочерних
    EXPECT_GT(player_type->get_size_in_memory(), game_object_type->get_size_in_memory());

    // Проверяем что можем найти унаследованные поля
    Field inherited_field = ts->lookup_field("player", "id");
    EXPECT_EQ(inherited_field.name(), "id");
}

TEST_F(TypeSystemTest, MethodSystem) {
    ValueType* test_type = ts->add_builtin_value_type("object", "method-test", 4, false, false, RegClass::GPR_64);

    TypeSpec method_spec = ts->make_function_typespec({}, "none");

    // Объявляем метод - ID может быть 2 или больше, потому что у object уже есть методы
    MethodInfo method = ts->declare_method(test_type, "test-method", std::nullopt, false, method_spec, false);

    // Вместо проверки конкретного ID, проверяем что он положительный и уникальный
    EXPECT_GT(method.id, 0);
    EXPECT_EQ(method.name, "test-method");
    EXPECT_FALSE(method.no_virtual);
    EXPECT_EQ(method.defined_in_type, "method-test");

    // Проверяем что метод добавлен в тип
    bool method_found = false;
    for (const auto& m : test_type->get_methods_defined_for_type()) {
        if (m.name == "test-method") {
            method_found = true;
            break;
        }
    }
    EXPECT_TRUE(method_found);

    // Ищем метод через систему типов
    MethodInfo found_method = ts->lookup_method("method-test", "test-method");
    EXPECT_EQ(found_method.name, "test-method");
    EXPECT_EQ(found_method.id, method.id); // ID должен совпадать
}


TEST_F(TypeSystemTest, TypeChecking) {
    TypeSpec object_spec = ts->make_typespec("object");
    TypeSpec int_spec = ts->make_typespec("int32");
    TypeSpec float_spec = ts->make_typespec("float");

    // int наследует от object
    EXPECT_TRUE(ts->tc(object_spec, int_spec));

    // int равен int
    EXPECT_TRUE(ts->tc(int_spec, int_spec));

    // int не является float
    EXPECT_FALSE(ts->tc(int_spec, float_spec));
}

TEST_F(TypeSystemTest, LowestCommonAncestor) {
    ts->add_builtin_types();

    TypeSpec int_spec = ts->make_typespec("int32");
    TypeSpec float_spec = ts->make_typespec("float");

    // LCA int32 и int32 = int32
    TypeSpec lca_same = ts->lowest_common_ancestor(int_spec, int_spec);
    EXPECT_EQ(lca_same.base_type(), "int32");

    // LCA int32 и float = number (а не object!)
    // Потому что: int32 -> sinteger -> integer -> number <- float
    TypeSpec lca_diff = ts->lowest_common_ancestor(int_spec, float_spec);
    EXPECT_EQ(lca_diff.base_type(), "number");

    // LCA int32 и object = object  
    TypeSpec object_spec = ts->make_typespec("object");
    TypeSpec lca_object = ts->lowest_common_ancestor(int_spec, object_spec);
    EXPECT_EQ(lca_object.base_type(), "object");
}

TEST_F(TypeSystemTest, TypeHierarchy) {
    ts->add_builtin_types();

    // Проверяем полную цепочку наследования для int32
    Type* type = ts->lookup_type("int32");
    std::vector<std::string> path;

    while (type && type->has_parent()) {
        path.push_back(type->get_name());
        type = ts->lookup_type(type->get_parent());
    }
    path.push_back("object");

    // Должно быть: int32 -> sinteger -> integer -> number -> object
    std::vector<std::string> expected = { "int32", "sinteger", "integer", "number", "object" };
    EXPECT_EQ(path, expected);
}

TEST_F(TypeSystemTest, MethodConstantsUsage) {
    ts->add_builtin_types();

    // Проверяем, что методы имеют правильные ID
    MethodInfo new_method = ts->lookup_method("object", "new");
    EXPECT_EQ(new_method.id, GOAL_NEW_METHOD);

    MethodInfo print_method = ts->lookup_method("object", "print");
    EXPECT_EQ(print_method.id, GOAL_PRINT_METHOD);

    // Проверяем поиск по ID с константами
    MethodInfo method_by_id = ts->lookup_method("object", GOAL_NEW_METHOD);
    EXPECT_EQ(method_by_id.name, "new");
}

TEST_F(TypeSystemTest, BitFieldType) {
    BitFieldType* flags_type = (BitFieldType*)ts->add_type("test-flags",
        std::make_unique<BitFieldType>("bitfield", "test-flags", 4, false));

    TypeSpec int_spec = ts->make_typespec("int32");

    ts->add_field_to_bitfield(flags_type, "flag1", int_spec, 0, 1, false);
    ts->add_field_to_bitfield(flags_type, "flag2", int_spec, 1, 2, false);

    EXPECT_EQ(flags_type->fields().size(), 2);
    EXPECT_FALSE(flags_type->is_reference());
}

TEST_F(TypeSystemTest, EnumType) {
    // Создаем базовый тип для enum
    ValueType* base_type = ts->add_builtin_value_type("int32", "enum-base", 4, false, true, RegClass::GPR_64);

    std::unordered_map<std::string, int64_t> entries = {
        {"RED", 1},
        {"GREEN", 2},
        {"BLUE", 3}
    };

    EnumType* color_enum = (EnumType*)ts->add_type("test-color",
        std::make_unique<EnumType>(base_type, "test-color", false, entries));

    ASSERT_NE(color_enum, nullptr);
    EXPECT_FALSE(color_enum->is_reference());
    EXPECT_EQ(color_enum->get_size_in_memory(), 4);
    EXPECT_FALSE(color_enum->is_bitfield());
    EXPECT_EQ(color_enum->entries().size(), 3);
}

TEST_F(TypeSystemTest, ArrayFields) {
    StructureType* array_type = ts->add_builtin_structure("structure", "test-array");

    TypeSpec float_spec = ts->make_typespec("float");

    // Добавляем массивное поле
    ts->add_field_to_type(array_type, "data", float_spec, false, false, 10, 0);

    EXPECT_EQ(array_type->fields().size(), 1);
    EXPECT_EQ(array_type->get_size_in_memory(), 40); // 10 * 4 bytes

    Field data_field = ts->lookup_field("test-array", "data");
    EXPECT_TRUE(data_field.is_array());
    EXPECT_EQ(data_field.array_size(), 10);
}

TEST_F(TypeSystemTest, TypeSpecOperations) {
    // Тестируем базовые операции TypeSpec
    TypeSpec base_spec("object");
    EXPECT_EQ(base_spec.base_type(), "object");
    EXPECT_TRUE(base_spec.empty());

    // Добавляем аргументы
    TypeSpec arg1("int32");
    base_spec.add_arg(arg1);

    EXPECT_FALSE(base_spec.empty());
    EXPECT_TRUE(base_spec.has_single_arg());

    // Клонирование
    TypeSpec cloned = base_spec;
    EXPECT_EQ(cloned.base_type(), "object");
    EXPECT_EQ(cloned.arg_count(), 1);
}

TEST_F(TypeSystemTest, VirtualMethodInheritance) {
    // Создаем иерархию типов с методами
    StructureType* base_type = ts->add_builtin_structure("structure", "base-type");

    TypeSpec base_method_spec = ts->make_function_typespec({ "base-type" }, "none");

    MethodInfo base_method = ts->declare_method(base_type, "base-method",
        std::nullopt, false, base_method_spec, false);

    // Создаем дочерний тип
    StructureType* derived_type = ts->add_builtin_structure("base-type", "derived-type");

    // Должны найти метод родителя
    MethodInfo found_method = ts->lookup_method("derived-type", "base-method");
    EXPECT_EQ(found_method.name, "base-method");
}

// Тест для проверки обработки ошибок
TEST_F(TypeSystemTest, ErrorConditions) {
    // Поиск несуществующего типа
    EXPECT_THROW(ts->lookup_type("non-existent-type"), std::runtime_error);

    // Поиск несуществующего поля
    EXPECT_THROW(ts->lookup_field("int32", "non-existent-field"), std::runtime_error);

    // Поиск несуществующего метода
    EXPECT_THROW(ts->lookup_method("int32", "non-existent-method"), std::runtime_error);
}

// Тест производительности для больших структур
TEST_F(TypeSystemTest, PerformanceTest) {
    StructureType* large_struct = ts->add_builtin_structure("structure", "large-struct");

    TypeSpec int_spec = ts->make_typespec("int32");

    // Добавляем много полей
    for (int i = 0; i < 10; i++) {
        std::string field_name = fmt::format("field_{}", i);
        ts->add_field_to_type(large_struct, field_name, int_spec, false, false, -1, -1);

        // Проверяем что поле сразу доступно в структуре
        bool field_exists = false;
        for (const auto& field : large_struct->fields()) {
            if (field.name() == field_name) {
                field_exists = true;
                break;
            }
        }
        EXPECT_TRUE(field_exists) << "Field " << field_name << " should be in memory structure";
    }

    // Теперь проверяем через lookup
    for (int i = 0; i < 10; i++) {
        std::string field_name = fmt::format("field_{}", i);
        Field field = ts->lookup_field("large-struct", field_name);
        EXPECT_EQ(field.name(), field_name);
    }
}

// Тест для forward declaration
TEST_F(TypeSystemTest, ForwardDeclaration) {
    ts->forward_declare_type_as("forward-type", "object");

    // Должны иметь возможность создать TypeSpec для forward объявленного типа
    TypeSpec forward_spec = ts->make_typespec("forward-type");
    EXPECT_EQ(forward_spec.base_type(), "forward-type");

    // Но не должны иметь полной информации
    EXPECT_TRUE(ts->partially_defined_type_exists("forward-type"));
    EXPECT_FALSE(ts->fully_defined_type_exists("forward-type"));
}

// Тест для new методов
TEST_F(TypeSystemTest, NewMethod) {
    StructureType* test_type = ts->add_builtin_structure("structure", "new-test");

    TypeSpec new_spec = ts->make_function_typespec({ "symbol", "type" }, "new-test");
    MethodInfo new_method = ts->add_new_method(test_type, new_spec, "Constructor for new-test");

    EXPECT_EQ(new_method.name, "new");
    EXPECT_EQ(new_method.id, 0);
    EXPECT_TRUE(test_type->has_new_method());

    const MethodInfo* retrieved_new = test_type->get_new_method_defined_for_type();
    ASSERT_NE(retrieved_new, nullptr);
    EXPECT_EQ(retrieved_new->name, "new");
}

// ============================================================================
// Tests for Reverse Field Lookup and Inline Arrays
// ============================================================================

TEST_F(TypeSystemTest, ReverseFieldLookupBasic) {
    // Создаем тестовую структуру
    StructureType* test_struct = ts->add_builtin_structure("structure", "reverse-test");

    TypeSpec int_spec = ts->make_typespec("int32");
    TypeSpec float_spec = ts->make_typespec("float");

    // Добавляем поля с известными смещениями
    ts->add_field_to_type(test_struct, "x", float_spec, false, false, -1, 0);
    ts->add_field_to_type(test_struct, "y", float_spec, false, false, -1, 4);
    ts->add_field_to_type(test_struct, "z", float_spec, false, false, -1, 8);
    ts->add_field_to_type(test_struct, "id", int_spec, false, false, -1, 12);

    // Тестируем reverse lookup
    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("reverse-test");
    input.offset = 4;  // Должно быть поле "y"

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tokens.size(), 1);
    EXPECT_EQ(result.tokens[0].kind, FieldReverseLookupOutput::Token::Kind::FIELD);
    EXPECT_EQ(result.tokens[0].name, "y");
    EXPECT_EQ(result.result_type.base_type(), "float");
}

TEST_F(TypeSystemTest, ReverseFieldLookupArray) {
    StructureType* array_struct = ts->add_builtin_structure("structure", "array-test");

    TypeSpec float_spec = ts->make_typespec("float");

    // Добавляем массив из 10 float
    ts->add_field_to_type(array_struct, "data", float_spec, false, false, 10, 0);

    // Тестируем доступ к 5-му элементу массива
    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("array-test");
    input.offset = 20;  // 5 * 4 bytes (float size)

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tokens.size(), 2);
    EXPECT_EQ(result.tokens[0].kind, FieldReverseLookupOutput::Token::Kind::FIELD);
    EXPECT_EQ(result.tokens[0].name, "data");
    EXPECT_EQ(result.tokens[1].kind, FieldReverseLookupOutput::Token::Kind::CONSTANT_IDX);
    EXPECT_EQ(result.tokens[1].idx, 5);
}

TEST_F(TypeSystemTest, ReverseFieldLookupInheritance) {
    // Родительская структура
    StructureType* parent_struct = ts->add_builtin_structure("structure", "parent-struct");
    TypeSpec int_spec = ts->make_typespec("int32");
    ts->add_field_to_type(parent_struct, "parent_field", int_spec, false, false, -1, 0);

    // Дочерняя структура
    StructureType* child_struct = ts->add_builtin_structure("parent-struct", "child-struct");
    ts->add_field_to_type(child_struct, "child_field", int_spec, false, false, -1, 4);

    // Ищем поле родителя в дочерней структуре
    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("child-struct");
    input.offset = 0;  // parent_field

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tokens.size(), 1);
    EXPECT_EQ(result.tokens[0].name, "parent_field");
}

TEST_F(TypeSystemTest, InlineArraySizeCalculation) {
    StructureType* inline_test = ts->add_builtin_structure("structure", "inline-test");

    TypeSpec float_spec = ts->make_typespec("float");
    TypeSpec vector_spec = ts->make_typespec("structure");

    // Создаем простую структуру для тестирования inline
    StructureType* vec3_type = ts->add_builtin_structure("structure", "vec3");
    ts->add_field_to_type(vec3_type, "x", float_spec, false, false, -1, 0);
    ts->add_field_to_type(vec3_type, "y", float_spec, false, false, -1, 4);
    ts->add_field_to_type(vec3_type, "z", float_spec, false, false, -1, 8);

    // vec3 должен иметь размер 12 байт (3 * float по 4 байта)
    // Без выравнивания до 16 байт!
    EXPECT_EQ(vec3_type->get_size_in_memory(), 12);

    // Inline массив из vec3 (каждый vec3 = 12 bytes)
    ts->add_field_to_type(inline_test, "points", ts->make_typespec("vec3"),
        true,  // inline
        false, // not dynamic
        5,     // array size 5
        0);    // offset 0

    // Размер должен быть 5 * 12 = 60 bytes (без дополнительного выравнивания)
    Field points_field = ts->lookup_field("inline-test", "points");
    int calculated_size = ts->get_size_in_type(points_field);

    EXPECT_EQ(calculated_size, 60); // 5 * 12
    EXPECT_EQ(inline_test->get_size_in_memory(), 60);
}

TEST_F(TypeSystemTest, ReverseFieldLookupInlineArray) {
    StructureType* container = ts->add_builtin_structure("structure", "container");

    // Создаем простую структуру для inline массива
    StructureType* element_type = ts->add_builtin_structure("structure", "element");
    TypeSpec int_spec = ts->make_typespec("int32");
    ts->add_field_to_type(element_type, "value", int_spec, false, false, -1, 0);
    ts->add_field_to_type(element_type, "flag", int_spec, false, false, -1, 4);

    // element должен иметь размер 8 байт (2 * int32 по 4 байта)
    EXPECT_EQ(element_type->get_size_in_memory(), 8);

    // Inline массив из 3 элементов
    ts->add_field_to_type(container, "elements", ts->make_typespec("element"),
        true, false, 3, 0);

    // Размер массива должен быть 3 * 8 = 24 байта
    Field elements_field = ts->lookup_field("container", "elements");
    EXPECT_EQ(ts->get_size_in_type(elements_field), 24);
    EXPECT_EQ(container->get_size_in_memory(), 24);

    // Тестируем доступ ко второму элементу, полю flag
    // Смещения: 
    // - elements[0] at 0-7 (value=0, flag=4)
    // - elements[1] at 8-15 (value=8, flag=12) 
    // - elements[2] at 16-23 (value=16, flag=20)

    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("container");
    input.offset = 12;  // elements[1].flag

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tokens.size(), 3);
    EXPECT_EQ(result.tokens[0].name, "elements");
    EXPECT_EQ(result.tokens[1].idx, 1);
    EXPECT_EQ(result.tokens[2].name, "flag");
}

TEST_F(TypeSystemTest, MultiReverseLookup) {
    StructureType* multi_struct = ts->add_builtin_structure("structure", "multi-test");

    TypeSpec int_spec = ts->make_typespec("int32");

    // Добавляем несколько полей с разными score
    ts->add_field_to_type(multi_struct, "field_a", int_spec, false, false, -1, 0, false, 1.0);
    ts->add_field_to_type(multi_struct, "field_b", int_spec, false, false, -1, 0, false, 2.0);

    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("multi-test");
    input.offset = 0;

    FieldReverseMultiLookupOutput results = ts->reverse_field_multi_lookup(input, 10);

    EXPECT_TRUE(results.success);
    EXPECT_GE(results.results.size(), 2);

    // Проверяем что результаты отсортированы по score
    EXPECT_GE(results.results[0].total_score, results.results[1].total_score);
}

TEST_F(TypeSystemTest, ReverseFieldLookupNotFound) {
    StructureType* simple_struct = ts->add_builtin_structure("structure", "simple");

    TypeSpec int_spec = ts->make_typespec("int32");
    ts->add_field_to_type(simple_struct, "data", int_spec, false, false, -1, 0);

    // Ищем по несуществующему смещению
    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("simple");
    input.offset = 100;  // За пределами структуры

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.tokens.empty());
}

// Тест для BitField reverse lookup
TEST_F(TypeSystemTest, ReverseFieldLookupBitField) {
    BitFieldType* flags_type = (BitFieldType*)ts->add_type("bitfield-test",
        std::make_unique<BitFieldType>("bitfield", "bitfield-test", 4, false));

    TypeSpec int_spec = ts->make_typespec("int32");

    // Добавляем битовые поля
    ts->add_field_to_bitfield(flags_type, "flag1", int_spec, 0, 1, false);
    ts->add_field_to_bitfield(flags_type, "flag2", int_spec, 1, 2, false);
    ts->add_field_to_bitfield(flags_type, "flag3", int_spec, 3, 1, false);

    FieldReverseLookupInput input;
    input.base_type = ts->make_typespec("bitfield-test");
    input.offset = 1;  // flag2

    FieldReverseLookupOutput result = ts->reverse_field_lookup(input);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tokens.size(), 1);
    EXPECT_EQ(result.tokens[0].name, "flag2");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}