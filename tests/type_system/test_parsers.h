// test_parsers.h
#pragma once
#include "type_system/export.h"
#include "script/export.h"
#include <gtest/gtest.h>

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = std::make_unique<TypeSystem>();
        // Добавляем базовые типы для тестов
        ts->add_builtin_types(GameVersion::Jak1);
    }

    void TearDown() override {
        ts.reset();
    }

    std::unique_ptr<TypeSystem> ts;

    // Вспомогательные методы для парсинга
    script::Object parse_goos(const std::string& input);
    bool defenum_parses_successfully(const std::string& input);
    bool deftype_parses_successfully(const std::string& input);

    // Валидация результатов
    void validate_enum_contents(const std::string& enum_name,
        const std::unordered_map<std::string, s64>& expected_entries);
};

// Макрос для удобного тестирования парсеров
#define TEST_DEFENUM(name, input) \
    TEST_F(ParserTest, name) { \
        ASSERT_TRUE(defenum_parses_successfully(input)); \
    }