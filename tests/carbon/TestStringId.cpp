// TestStringId.cpp
#include "gtest/gtest.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

#include "carbon/Export.hpp"
#include "lib/StringId.hpp"
#include "lib/StringIdManager.hpp"

using namespace carbon::lib;

class StringIdTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringIdManager::instance().clear();
        create_test_strings_file();
    }

    void TearDown() override {
        if (std::filesystem::exists("test_strings.txt")) {
            std::filesystem::remove("test_strings.txt");
        }
        if (std::filesystem::exists("test_save.txt")) {
            std::filesystem::remove("test_save.txt");
        }
        if (std::filesystem::exists("bad_format.txt")) {
            std::filesystem::remove("bad_format.txt");
        }
        StringIdManager::instance().clear();
    }

private:
    void create_test_strings_file() {
        std::ofstream file("test_strings.txt");
        file << "A1DE215E hello\n";
        file << "0B70ED28 world\n";
        file << "E1213FA8 test\n";
        file.close();
    }
};

TEST_F(StringIdTest, AllStandardTypes) {
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
        StringId sid = StringId(type.name);
        EXPECT_EQ(sid.value, type.expected_crc) 
            << "Failed for type: " << type.name;
        
        // Проверяем обратное преобразование - используем to_string(), а не to_cstring()
        EXPECT_EQ(sid.to_string(), type.name);
    }
}

TEST_F(StringIdTest, BasicOperations) {
    StringId sid1 = SID("hello");
    StringId sid2 = SID("hello");
    StringId sid3 = SID("world");

    EXPECT_EQ(sid1, sid2);
    EXPECT_NE(sid1, sid3);
    EXPECT_NE(sid1.value, 0u);
}

TEST_F(StringIdTest, TableLoadSave) {
    auto& man = StringIdManager::instance();
    bool loaded = man.load_table("test_strings.txt");
    EXPECT_TRUE(loaded);
    EXPECT_EQ(man.size(), 3);

    // Используем to_string() вместо to_cstring()
    EXPECT_EQ(StringId(0xA1DE215E).to_string(), "hello");
    EXPECT_EQ(StringId(0x0B70ED28).to_string(), "world");
    EXPECT_EQ(StringId(0xE1213FA8).to_string(), "test");

    bool saved = man.save_table("test_save.txt");
    EXPECT_TRUE(saved);
    EXPECT_TRUE(std::filesystem::exists("test_save.txt"));

    man.clear();
    EXPECT_EQ(man.size(), 0);
    
    loaded = man.load_table("test_save.txt");
    EXPECT_TRUE(loaded);
    EXPECT_EQ(man.size(), 3);
    EXPECT_EQ(StringId(0xA1DE215E).to_string(), "hello");
}

TEST_F(StringIdTest, UnknownStringId) {
    auto& man = StringIdManager::instance();
    man.load_table("test_strings.txt");

    StringId unknown(0xDEADBEEF);
    std::string result = unknown.to_string();

    // Проверяем формат <unknown:0xDEADBEEF>
    EXPECT_EQ(result, "<unknown:0xDEADBEEF>");
    
    // C-string версия
    EXPECT_STREQ(unknown.to_cstring(), "<unknown:0xDEADBEEF>");
}

// TestStringId.cpp - исправленная версия
TEST_F(StringIdTest, STLCompatibility) {
    auto& man = StringIdManager::instance();
    man.load_table("test_strings.txt");

    // Используем стандартный хеш (уже определен в специализации std::hash)
    std::unordered_map<StringId, int> map;
    map[SID("hello")] = 100;
    map[SID("world")] = 200;

    EXPECT_EQ(map[SID("hello")], 100);
    EXPECT_EQ(map[SID("world")], 200);
    EXPECT_TRUE(map.contains(SID("hello")));
    EXPECT_FALSE(map.contains(SID("nonexistent")));
}

TEST_F(StringIdTest, GlobalToString) {
    auto& man = StringIdManager::instance();
    man.load_table("test_strings.txt");
    
    // SID("hello") зарегистрирует строку с CRC32
    // Но чтобы получить "hello", нужно сначала загрузить таблицу
    StringId sid = SID("hello");
    std::string result = sid.to_string();
    
    // После загрузки таблицы, to_string() должна вернуть "hello"
    EXPECT_EQ(result, "hello");
}

TEST_F(StringIdTest, HexFormat) {
    auto& man = StringIdManager::instance();
    man.clear();
    
    // Проверяем формат для разных ID
    EXPECT_EQ(StringId(0xDEADBEEF).to_string(), "<unknown:0xDEADBEEF>");
    EXPECT_EQ(StringId(0x12345678).to_string(), "<unknown:0x12345678>");
    EXPECT_EQ(StringId(0x1).to_string(), "<unknown:0x1>");
    EXPECT_EQ(StringId(0x0).to_string(), "");
}

TEST_F(StringIdTest, InvalidFileHandling) {
    auto& man = StringIdManager::instance();
    
    // Несуществующий файл - возвращает false
    bool loaded = man.load_table("nonexistent.txt");
    EXPECT_FALSE(loaded);  // <-- Ожидаем false
    EXPECT_EQ(man.size(), 0);

    // Файл с некорректным форматом
    std::ofstream bad_file("bad_format.txt");
    bad_file << "INVALID hello\n";  // не hex число
    bad_file << "12345678\n";       // без имени
    bad_file.close();

    // Должен вернуть false, так как нет валидных записей
    loaded = man.load_table("bad_format.txt");
    EXPECT_FALSE(loaded);  // <-- Ожидаем false
    EXPECT_EQ(man.size(), 0);
    
    // Файл с одной валидной строкой - должен вернуть true
    std::ofstream good_file("good_format.txt");
    good_file << "A1DE215E hello\n";
    good_file << "invalid line\n";
    good_file.close();
    
    loaded = man.load_table("good_format.txt");
    EXPECT_TRUE(loaded);  // <-- Ожидаем true (есть валидная строка)
    EXPECT_EQ(man.size(), 1);
    EXPECT_EQ(StringId(0xA1DE215E).to_string(), "hello");
    
    std::filesystem::remove("good_format.txt");
}

TEST_F(StringIdTest, EmptyTable) {
    auto& man = StringIdManager::instance();
    EXPECT_EQ(man.size(), 0);

    StringId sid = SID("anything");
    std::string result = sid.to_string();
    
    // Форматируем ID как hex с 8 цифрами
    std::ostringstream oss;
    oss << "<unknown:0x" << std::hex << std::uppercase << sid.value << ">";
    std::string expected = oss.str();
    
    EXPECT_EQ(result, expected);
}

TEST_F(StringIdTest, FormatOutput) {
    auto& man = StringIdManager::instance();
    man.load_table("test_strings.txt");
    
    StringId sid(0xDEADBEEF);
    std::string result = sid.to_string();
    
    EXPECT_EQ(result, "<unknown:0xDEADBEEF>");
}

TEST_F(StringIdTest, StringIdComparison) {
    StringId sid1 = SID("hello");
    StringId sid2 = SID("hello");
    StringId sid3 = SID("world");
    
    EXPECT_TRUE(sid1 == sid2);
    EXPECT_TRUE(sid1 != sid3);
    EXPECT_TRUE(sid1 == "hello");
    EXPECT_TRUE(sid1 != "world");
}

TEST_F(StringIdTest, SIDRegistration) {
    auto& man = StringIdManager::instance();
    man.clear();
    
    // SID не регистрирует строку автоматически
    StringId sid1 = SID("test_string");
    
    // Строка не зарегистрирована, поэтому to_string() вернет <unknown:0xHEX>
    EXPECT_FALSE(man.has_string(sid1.value));
    
    // Форматируем ожидаемый результат в hex
    std::ostringstream oss;
    oss << "<unknown:0x" << std::hex << std::uppercase << sid1.value << ">";
    std::string expected = oss.str();
    
    EXPECT_EQ(sid1.to_string(), expected);
    
    // Регистрируем строку вручную
    u32 registered_id = man.register_string("test_string");
    EXPECT_EQ(sid1.value, registered_id);  // ID совпадает
    
    // Теперь строка найдется
    EXPECT_TRUE(man.has_string(sid1.value));
    EXPECT_EQ(sid1.to_string(), "test_string");
    
    // Повторная регистрация возвращает тот же ID
    u32 registered_id2 = man.register_string("test_string");
    EXPECT_EQ(sid1.value, registered_id2);
    
    // Другая строка - другой ID
    StringId sid2 = SID("different");
    u32 registered_id3 = man.register_string("different");
    EXPECT_EQ(sid2.value, registered_id3);
    EXPECT_NE(sid1, sid2);
}