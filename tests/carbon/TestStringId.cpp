#include "gtest/gtest.h"
#include <fstream>
#include <filesystem>
#include <iostream>


#include "carbon/Export.hpp"

using namespace carbon::vm;
using namespace carbon::lib;
using namespace carbon::files;
using namespace carbon::modules;
using namespace carbon::kernel;

class StringIdTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Очищаем таблицу перед каждым тестом
        string_id::clear_table();

        // Создаём временный файл для тестов
        create_test_strings_file();
    }

    void TearDown() override {
        // Удаляем временный файл
        //if (std::filesystem::exists("test_strings.txt")) {
        //    std::filesystem::remove("test_strings.txt");
        //}
        string_id::clear_table();
    }

private:
    void create_test_strings_file() {
        std::ofstream file("test_strings.txt");
        file << "A1DE215E hello\n";
        file << "0B70ED28 world\n";
        file << "E1213FA8 test\n";
    }
};

TEST_F(StringIdTest, AllStandardTypes) {
    // Массив предопределённых типов и их CRC32
    struct TypeInfo {
        const char* name;
        uint32_t expected_crc;
    };
    
    std::vector<TypeInfo> types = {
        {"vector", 0x012f77fe},
        {"string", 0x0b3952e7},
        {"float", 0x0f182ec3},
        {"angle", 0x13812cd6},
        {"state", 0x2e6743e3},
        {"direction", 0x7194cbe7},
        {"color", 0x71e73c6c},
        {"boolean", 0x8b4e76ff},
        {"vec4", 0x93bd2e95},
        {"script-lambda", 0x9ed499e1},
        {"function", 0xab3eb31f},
        {"int32", 0xC7CB275C}
    };
    
    for (const auto& type : types) {
        StringId sid = string_id::register_string(type.name);
        EXPECT_EQ(sid, type.expected_crc) 
            << "Failed for type: " << type.name 
            << " expected: " << std::hex << type.expected_crc 
            << " got: " << std::hex << sid;
        
        // Проверяем обратное преобразование
        EXPECT_EQ(string_id::to_string(sid), type.name);
    }
}


TEST_F(StringIdTest, BasicOperations) {
    StringId sid1 = SID("hello");
    StringId sid2 = SID("hello");
    StringId sid3 = SID("world");

    EXPECT_EQ(sid1, sid2);
    EXPECT_NE(sid1, sid3);
    EXPECT_NE(sid1, 0u);
}

TEST_F(StringIdTest, TableLoadSave) {
    // Загружаем таблицу
    EXPECT_NO_THROW(string_id::load_table("test_strings.txt"));
    EXPECT_TRUE(string_id::is_table_loaded());
    EXPECT_EQ(string_id::get_string_count(), 3);

    // Проверяем поиск
    EXPECT_EQ(string_id::to_string(0xA1DE215E), "hello");
    EXPECT_EQ(string_id::to_string(0x0B70ED28), "world");
    EXPECT_EQ(string_id::to_string(0xE1213FA8), "test");

    // Сохраняем таблицу
    EXPECT_NO_THROW(string_id::save_table("test_save.txt"));

    // Проверяем что файл создан
    EXPECT_TRUE(std::filesystem::exists("test_save.txt"));

    // Очищаем и загружаем обратно
    string_id::clear_table();
    EXPECT_NO_THROW(string_id::load_table("test_save.txt"));
    EXPECT_EQ(string_id::to_string(0xA1DE215E), "hello");

    // Удаляем временный файл сохранения
    std::filesystem::remove("test_save.txt");
}

TEST_F(StringIdTest, UnknownStringId) {
    string_id::load_table("test_strings.txt");

    StringId unknown = 0xDEADBEEF;
    std::string result = string_id::to_string(unknown);

    // Должен вернуть форматированную строку с hex
    EXPECT_TRUE(result.find("unknown") != std::string::npos);
    EXPECT_TRUE(result.find("DEADBEEF") != std::string::npos);

    // C-string версия
    EXPECT_TRUE(std::string(string_id::to_cstring(unknown)).find("unknown") != std::string::npos);
}

TEST_F(StringIdTest, STLCompatibility) {
    string_id::load_table("test_strings.txt");

    std::unordered_map<StringId, int> map;
    map[SID("hello")] = 100;
    map[SID("world")] = 200;

    EXPECT_EQ(map[SID("hello")], 100);
    EXPECT_EQ(map[SID("world")], 200);

    // Проверяем что можем использовать как ключи
    EXPECT_TRUE(map.contains(SID("hello")));
    EXPECT_FALSE(map.contains(SID("nonexistent")));
}

TEST_F(StringIdTest, GlobalToString) {
    string_id::load_table("test_strings.txt");
    std::cout << "Table loaded. String count: " << string_id::inspect() << std::endl;
    StringId sid = SID("hello");
    std::string result = to_string(sid);  // глобальная функция

    EXPECT_EQ(result, "hello");
}

TEST_F(StringIdTest, InvalidFileHandling) {
    // Несуществующий файл
    EXPECT_THROW(string_id::load_table("nonexistent.txt"), std::runtime_error);

    // Файл с некорректным форматом
    std::ofstream bad_file("bad_format.txt");
    bad_file << "INVALID hello\n";  // не hex число
    bad_file.close();

    EXPECT_THROW(string_id::load_table("bad_format.txt"), std::runtime_error);

    std::filesystem::remove("bad_format.txt");
}

TEST_F(StringIdTest, EmptyTable) {
    EXPECT_FALSE(string_id::is_table_loaded());
    EXPECT_EQ(string_id::get_string_count(), 0);

    // Поиск в пустой таблице
    StringId sid = SID("anything");
    EXPECT_TRUE(string_id::to_string(sid).find("unknown") != std::string::npos);
}