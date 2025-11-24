#include "gtest/gtest.h"
#include "vm/binary_file.hpp"
#include "vm/binary_file_builder.hpp"
#include "vm/instructions.hpp"
#include "vm/module.hpp"
#include <iostream>
#include "fmt/format.h"

using namespace vm;

class BinaryFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализируем пул для тестов
        BinaryFilePool::initialize(1024 * 1024); // 1MB для тестов
        string_id::initialize();
    }

    void TearDown() override {
        BinaryFilePool::shutdown();
    }

    // Вспомогательная функция для создания валидного BinaryFile в пуле
    std::shared_ptr<Module>  create_test_module(const std::string& name, u32 definitions_count = 0) {
        // Используем BinaryFileBuilder для создания модуля
        BinaryFileBuilder builder;
        auto name_id = string_id::register_string(name);
        builder.set_module_name(name_id);

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
        auto module = builder.build_and_load_to_pool();
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

TEST_F(BinaryFileTest, BinaryFileRelocation) {
    std::cout << "=== Starting BinaryFileRelocation ===" << std::endl;

    auto module = create_test_module("reloc_test", 1);
    ASSERT_NE(module, nullptr);

    // Сохраняем исходные значения
    u32 original_generation = module->generation;
    auto original_def = module->binary_file->get_definition(0);
    StringId original_name = original_def->name;

    // Вызываем релокацию (имитируем перемещение в пуле)
    // В реальности это должно вызываться из BinaryFilePool::compactify()
    void* new_base = BinaryFilePool::get_base_address();
    module->binary_file->relocate_pointers(new_base);

    // Проверяем что генерация увеличилась
    EXPECT_EQ(module->binary_file->generation, 2); // 1 (начальное) + 1

    // Определение все еще доступно
    auto relocated_def = module->binary_file->get_definition(0);
    EXPECT_EQ(relocated_def->name, original_name);

    std::cout << "=== BinaryFileRelocation completed ===" << std::endl;
}

TEST_F(BinaryFileTest, PoolCompaction) {
    std::cout << "=== Starting PoolCompaction ===" << std::endl;

    // Создаем несколько модулей
    auto module1 = create_test_module("compaction1");
    auto module2 = create_test_module("compaction2");

    ASSERT_NE(module1, nullptr);
    ASSERT_NE(module2, nullptr);

    // Сохраняем адрес второго модуля
    void* original_addr2 = module2->binary_file;

    // Выгружаем первый модуль
    BinaryFilePool::deallocate(module1->full_name);

    // Компактифицируем
    bool compact_result = BinaryFilePool::compactify();
    EXPECT_TRUE(compact_result);

    // Второй модуль все еще должен быть доступен
    EXPECT_TRUE(module2->is_binary_loaded());
    EXPECT_NE(module2->binary_file, nullptr);

    // Адрес должен измениться после компактификации
    EXPECT_NE(module2->binary_file, original_addr2);

    std::cout << "=== PoolCompaction completed ===" << std::endl;
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
    Definition* defs = reinterpret_cast<Definition*>(
        reinterpret_cast<u8*>(module->binary_file) + sizeof(BinaryFile));

    // Создаем ByteCode для второго определения
    ByteCode* bytecode = reinterpret_cast<ByteCode*>(
        reinterpret_cast<u8*>(defs) + 2 * sizeof(Definition));

    defs[1].data_ptr = Ptr<void>(reinterpret_cast<u8*>(bytecode) - reinterpret_cast<u8*>(module->binary_file));

    // Инициализируем ByteCode
    bytecode->code_count = 10;
    bytecode->data_size = 100;
    bytecode->debug_count = 5;

    // Ищем по имени
    ByteCode* found = module->binary_file->find_bytecode_by_name(SID("def_1"));
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->code_count, 10);
    EXPECT_EQ(found->data_size, 100);

    // Ищем несуществующее
    ByteCode* not_found = module->binary_file->find_bytecode_by_name(SID("nonexistent"));
    EXPECT_EQ(not_found, nullptr);
}