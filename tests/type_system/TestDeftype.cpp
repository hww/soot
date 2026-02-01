#include <gtest/gtest.h>
#include "common/type_system/TypeSystem.hpp"
#include "common/type_system/Deftype.hpp"
#include "fmt/format.h"
#include "common/sooti/Export.hpp"

#define EXPECT_DEFTYPE_THROW(statement) \
    try { \
        statement; \
        FAIL() << "Expected exception but none was thrown"; \
    } catch (const std::exception& e) { \
        SUCCEED() << "Caught expected exception: " << typeid(e).name() << " - " << e.what(); \
    } catch (...) { \
        SUCCEED() << "Caught expected exception of unknown type"; \
    }
void expect_deftype_throws(const std::function<void()>& func) {
    try {
        func();
        FAIL() << "Expected exception but none was thrown";
    }
    catch (const std::exception& e) {
        SUCCEED() << "Caught expected exception: " << typeid(e).name() << " - " << e.what();
    }
    catch (...) {
        SUCCEED() << "Caught expected exception of unknown type";
    }
}
class DefTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = &TypeSystem::instance();
        ts->add_builtin_types();
    }

    void TearDown() override {
        ts = nullptr;
    }
    // Хелпер для парсинга как в оригинале
    DeftypeResult parse_deftype_string(const std::string& code) {
        auto obj = reader.read_from_string(code, "test");
        fmt::print("\n\nParsed: {}\n", script::pretty_print::to_string(obj));

        // Извлекаем форму deftype: (top-level (deftype name ...))
        // в просто (name ...) 
        auto& deftype_form = obj.as_pair()->cdr.as_pair()->car.as_pair()->cdr;
        return parse_deftype(deftype_form, ts, nullptr);
    }
    TypeSystem* ts;
    script::Reader reader;
};

TEST_F(DefTypeTest, BasicStructure) {
    std::string code = R"(
        (deftype test-structure
          (structure)
          ((x int32)
           (y int32)
           (z int32)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);
    EXPECT_EQ(result.type.print(), "test-structure");

    auto structure = dynamic_cast<StructureType*>(result.type_info);
    ASSERT_NE(structure, nullptr);
    EXPECT_EQ(structure->get_name(), "test-structure");
    EXPECT_EQ(structure->get_parent(), "structure");
}

TEST_F(DefTypeTest, BasicType) {
    std::string code = R"(
        (deftype test-basic
          (basic)
          ((id int32)
           (name string)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    auto basic = dynamic_cast<BasicType*>(result.type_info);
    ASSERT_NE(basic, nullptr);
    EXPECT_EQ(basic->get_name(), "test-basic");
    EXPECT_EQ(basic->get_parent(), "basic");
}

TEST_F(DefTypeTest, BitfieldType) {
    std::string code = R"(
        (deftype test-bitfield
          (integer)
          ((flag1 uint8 :offset 0 :size 1)
           (flag2 uint8 :offset 1 :size 2)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    auto bitfield = dynamic_cast<BitFieldType*>(result.type_info);
    ASSERT_NE(bitfield, nullptr);
    EXPECT_EQ(bitfield->get_name(), "test-bitfield");
    EXPECT_EQ(bitfield->get_parent(), "integer");
}

TEST_F(DefTypeTest, WithDocstring) {
    std::string code = R"(
        (deftype documented-type
          (structure)
          "This is a test type with documentation"
          ((data int32)
           (value float)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);
    EXPECT_EQ(result.type_info->get_name(), "documented-type");

    // Проверяем что докстринг сохранился
    EXPECT_TRUE(result.type_info->m_metadata.has_docstring());
    EXPECT_EQ(result.type_info->m_metadata.get_docstring_or_empty(),
        "This is a test type with documentation");
}

TEST_F(DefTypeTest, WithMethods) {
    std::string code = R"(
        (deftype method-type
          (structure)
          ((x int32)
           (y int32))
          (:methods
            (add (function int32 int32) int32)
            (multiply (function int32 int32) int32)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    // Проверяем что методы добавились
    EXPECT_GT(result.type_info->get_num_methods(), 0);
}

TEST_F(DefTypeTest, ArrayField) {
    std::string code = R"(
        (deftype array-type
          (structure)
          ((count int32)
           (data int32 10 :dynamic)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    auto structure = dynamic_cast<StructureType*>(result.type_info);
    ASSERT_NE(structure, nullptr);

    // Проверяем что поле с массивом создалось
    Field data_field;
    bool has_data = structure->lookup_field("data", &data_field);
    EXPECT_TRUE(has_data);
    EXPECT_TRUE(data_field.is_array());
}

TEST_F(DefTypeTest, InlineField) {
    std::string code = R"(
        (deftype inline-type
          (structure)
          ((transform matrix :inline)
           (id int32)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    auto structure = dynamic_cast<StructureType*>(result.type_info);
    ASSERT_NE(structure, nullptr);

    Field transform_field;
    bool has_transform = structure->lookup_field("transform", &transform_field);
    EXPECT_TRUE(has_transform);
    EXPECT_TRUE(transform_field.is_inline());
}

TEST_F(DefTypeTest, ComplexOptions) {
    std::string code = R"(
        (deftype complex-type
          (structure)
          ((a int32)
           (b float))
          :pack-me
          :no-inspect
          :heap-base 16)
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    auto structure = dynamic_cast<StructureType*>(result.type_info);
    ASSERT_NE(structure, nullptr);

    EXPECT_TRUE(structure->is_packed());
    EXPECT_FALSE(structure->gen_inspect());
    EXPECT_EQ(structure->heap_base(), 16);
}

TEST_F(DefTypeTest, InvalidParent) {
    std::string code = R"(
        (deftype invalid-type
          (non-existent-parent)
          ((field int32)))
    )";
    EXPECT_ANY_THROW(parse_deftype_string(code));
}

TEST_F(DefTypeTest, DuplicateFields) {
    std::string code = R"(
        (deftype duplicate-fields
          (structure)
          ((field int32)
           (field float)))
    )";
    EXPECT_ANY_THROW(parse_deftype_string(code));
}

TEST_F(DefTypeTest, InvalidFieldType) {
    std::string code = R"(
        (deftype invalid-field
          (structure)
          ((field non-existent-type)))
    )";
    EXPECT_ANY_THROW(parse_deftype_string(code));
}

TEST_F(DefTypeTest, SizeAssert) {
    std::string code = R"(
        (deftype sized-type
          (structure)
          ((a int32)
           (b int32))
          :size-assert 8)
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);
    // Должен пройти без ошибок если размер совпадает
}

TEST_F(DefTypeTest, FailedSizeAssert) {
    std::string code = R"(
        (deftype wrong-sized-type
          (structure)
          ((a int32)
           (b int32))
          :size-assert 4)
    )";
    EXPECT_ANY_THROW(parse_deftype_string(code));
}


TEST_F(DefTypeTest, WithStates) {
    std::string code = R"(
        (deftype stateful-type
          (structure)
          ((value int32))
          (:states
            idle
            running
            (finished float)))
    )";
    DeftypeResult result = parse_deftype_string(code);

    ASSERT_NE(result.type_info, nullptr);

    // Проверяем что states добавились
    auto& states = result.type_info->get_states_declared_for_type();
    EXPECT_GT(states.size(), 0);
}

TEST_F(DefTypeTest, Inheritance) {
    // Сначала создаем родительский тип
    std::string code1 = R"(
        (deftype parent-type
          (structure)
          ((parent-field int32)))
    )";
    DeftypeResult parent_result = parse_deftype_string(code1); // Сохраняем результат родителя

    // Затем дочерний тип  
    std::string code2 = R"(
        (deftype child-type
          (parent-type)
          ((child-field float)))
    )";
    DeftypeResult child_result = parse_deftype_string(code2); // Сохраняем результат ребенка

    ASSERT_NE(child_result.type_info, nullptr); // Проверяем child_result

    auto child = dynamic_cast<StructureType*>(child_result.type_info);
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->get_parent(), "parent-type"); // Должно работать

    // Проверяем что унаследовались поля
    Field parent_field, child_field;
    bool has_parent = child->lookup_field("parent-field", &parent_field);
    bool has_child = child->lookup_field("child-field", &child_field);

    EXPECT_TRUE(has_parent);
    EXPECT_TRUE(has_child);
}