#include "gtest/gtest.h"
#include "carbon/Export.hpp"
#include <iostream>
#include "fmt/format.h"

using namespace runtime::vm;
using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;



class BinaryFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализируем пул для тестов 1MB для тестов
        string_id::initialize();
    }

    void TearDown() override {
    }

    // Вспомогательная функция для создания валидного BinaryFile в пуле
    std::shared_ptr<Module>  create_test_module(const std::string& name, u32 definitions_count = 0) {
        // Используем BinaryFileBuilder для создания модуля
        BinaryFileBuilder builder(name);

        // Добавляем определения если нужно
        for (u32 i = 0; i < definitions_count; i++) {
            std::string def_name = "def_" + std::to_string(i);
            auto def_name_id = string_id::register_string(def_name);

            // Создаем простую функцию с минимальным кодом
            std::vector<Instruction> code = {
                Instruction::create_a(Opcode::RETURN,0),
                Instruction::create_a(Opcode::RETURN,0)
            };

            builder.add_function(
                def_name_id,
                code,
                {}, // пустые данные
                {}  // пустая отладочная информация
            );
        }
        builder.debug_print_input();
        builder.debug_full_inspect(builder.build());
        // Строим и загружаем модуль в пул
        auto module = builder.build_module();
        fmt::print("Module {}", module->inspect());

        fmt::print("Strings {}", string_id::inspect());

        return module;
    }
};

TEST_F(BinaryFileTest, ByteCodeBasicStructure) {
    EXPECT_GE(sizeof(ByteCode), 16U);
}

TEST_F(BinaryFileTest, BinaryFileValidation) {
    // Создаем валидный файл
    auto module = create_test_module("validation_test");
    ASSERT_NE(module, nullptr);

    EXPECT_TRUE(module->is_binary_loaded());
    EXPECT_TRUE(module->binary_file->is_valid());

    // Проверяем невалидный magic
    module->binary_file->magic = 0x12345678;
    EXPECT_FALSE(module->binary_file->is_valid());
}

TEST_F(BinaryFileTest, BinaryFileCreation) {
    auto module = create_test_module("creation_test");
    ASSERT_NE(module, nullptr);

    EXPECT_TRUE(module->is_binary_loaded());
    EXPECT_NE(module->binary_file, nullptr);
    EXPECT_EQ(module->binary_file->definitions_count, 0);
    EXPECT_TRUE(module->binary_file->is_valid());
}

TEST_F(BinaryFileTest, AddDefinition) {
    auto module = create_test_module("def_test", 2);

    // Получаем определения из модуля
    auto def0 = module->binary_file->get_definition(0);
    auto def1 = module->binary_file->get_definition(1);

    // ВАЖНО: используем string_id::register_string вместо compute_crc32_constexpr
    auto def0_name_expected = string_id::register_string("def_0");
    auto def1_name_expected = string_id::register_string("def_1");
    auto function_type_expected = string_id::register_string("function");

    lg::info("=== TEST EXPECTATIONS ===");
    lg::info("def0_name_expected: {} ({})", string_id::to_string(def0_name_expected), def0_name_expected);
    lg::info("def1_name_expected: {} ({})", string_id::to_string(def1_name_expected), def1_name_expected);
    lg::info("function_type_expected: {} ({})", string_id::to_string(function_type_expected), function_type_expected);
    lg::info("def0->name: {} ({})", string_id::to_string(def0->name), def0->name);
    lg::info("def1->name: {} ({})", string_id::to_string(def1->name), def1->name);
    lg::info("def0->type: {} ({})", string_id::to_string(def0->type), def0->type);
    lg::info("def1->type: {} ({})", string_id::to_string(def1->type), def1->type);

    EXPECT_EQ(def0->name, def0_name_expected);
    EXPECT_EQ(def0->type, function_type_expected);
    EXPECT_EQ(def1->name, def1_name_expected);
    EXPECT_EQ(def1->type, function_type_expected);
}

TEST_F(BinaryFileTest, DefinitionBoundsChecking) {
    std::cout << "=== Starting DefinitionBoundsChecking ===" << std::endl;

    auto module = create_test_module("bounds_test", 2);
    ASSERT_NE(module, nullptr);

    // Проверяем границы
    EXPECT_NO_THROW(module->binary_file->get_definition(0));
    EXPECT_NO_THROW(module->binary_file->get_definition(1));
    EXPECT_THROW(module->binary_file->get_definition(2), std::runtime_error);

    std::cout << "=== DefinitionBoundsChecking completed ===" << std::endl;
}

TEST_F(BinaryFileTest, ModuleExportResolution) {
    std::cout << "=== Starting ModuleExportResolution ===" << std::endl;

    auto module = create_test_module("export_test", 1);
    ASSERT_NE(module, nullptr);

    // Получаем определение из binary file
    auto def = module->binary_file->get_definition(0);

    // Добавляем экспорт в модуль
    module->add_export(def->name, def);

    // Проверяем разрешение символа
    EXPECT_TRUE(module->has_export(def->name));
    auto export_def = module->get_export(def->name);
    EXPECT_EQ(export_def->name, def->name);

    std::cout << "=== ModuleExportResolution completed ===" << std::endl;
}

TEST_F(BinaryFileTest, InvalidBinaryFile) {
    // Тестируем невалидные файлы
    BinaryFile invalid_file;
    invalid_file.magic = 0xDEADBEEF;
    EXPECT_FALSE(invalid_file.is_valid());

    // Пустой файл (магия не установлена)
    BinaryFile empty_file;
    EXPECT_FALSE(empty_file.is_valid());

    // Файл с неправильным размером
    BinaryFile wrong_size_file;
    wrong_size_file.magic = BinaryFile::MAGIC;
    wrong_size_file.file_size = BinaryFile::HEADER_SIZE - 1; // Меньше минимального
    EXPECT_FALSE(wrong_size_file.is_valid());
}

TEST_F(BinaryFileTest, FindByteCodeByName) {
    auto module = create_test_module("find_test", 2);
    ASSERT_NE(module, nullptr);

    // Добавляем ByteCode определение
    Definition* def1 = module->binary_file->get_definition(1);

    // Создаем ByteCode для второго определения
    ByteCode* bytecode1 = reinterpret_cast<ByteCode*>(def1->data_ptr.c());
     
    // Инициализируем ByteCode
    bytecode1->code_count = 10;
    bytecode1->data_size = 100;
    bytecode1->debug_count = 5;

    // Ищем по имени
    ByteCode* found = module->binary_file->find_bytecode_by_name(SID("def_1"));
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->code_count, 10);
    EXPECT_EQ(found->data_size, 100);
    EXPECT_EQ(found->debug_count, 5);

    // Ищем несуществующее
    ByteCode* not_found = module->binary_file->find_bytecode_by_name(SID("nonexistent"));
    EXPECT_EQ(not_found, nullptr);
}