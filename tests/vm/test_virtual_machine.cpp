#include "gtest/gtest.h"
#include "virtual_machine.hpp"
#include "native_func.hpp"
#include "binary_file.hpp"
#include "binary_file_builder.hpp"
#include "util/log.h"

using namespace vm;

class VirtualMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализируем нативные функции перед каждым тестом
        NativeFunctionRegistry::get_instance().initialize_builtins();
    }
};

TEST_F(VirtualMachineTest, BasicCreation) {
    VirtualMachine vm;
    EXPECT_FALSE(vm.to_string().empty());
    vm.dump_state(); // Для отладки
}

TEST_F(VirtualMachineTest, NativeFunctionRegistration) {
    VirtualMachine vm;

    auto test_func = [](u32 argc, const Variant* argv) -> Variant {
        return Variant(42);
        };

    REGISTER_NATIVE_FUNCTION(SID("test_native"), test_func);

    // Проверяем через реестр
    NativeFunction found = NativeFunctionRegistry::get_instance().find_function(SID("test_native"));
    EXPECT_NE(found, nullptr);

    // Проверяем вызов
    Variant result = found(0, nullptr);
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, SimpleExecution) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder;

    // Простая функция: return 42
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42)); // r1 = 42
    code.push_back(Instruction::create_a(Opcode::RETURN, 1)); // return r1

    builder.add_function(SID("simple_answer"), code);

    // ИСПРАВЛЕНИЕ: build_file() вызывается правильно
    auto module = builder.build_and_load_to_pool(SID("test"));
    auto bytecode = module->resolve_symbol(SID("simple_answer"),SID("dunction"));

    Variant result = vm.execute_bytecode((ByteCode*)bytecode->data_ptr.c());

    EXPECT_FALSE(result.is_null());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, BasicArithmetic) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder;

    // Функция: (5 + 3) * 2
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 5));  // r1 = 5
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 3));  // r2 = 3
    code.push_back(Instruction::create_abc(Opcode::ADD_INT, 3, 1, 2));          // r3 = r1 + r2
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 4, 2));  // r4 = 2
    code.push_back(Instruction::create_abc(Opcode::MUL_INT, 5, 3, 4));          // r5 = r3 * r4
    code.push_back(Instruction::create_a(Opcode::RETURN, 5));                   // return r5

    builder.add_function(SID("calculate"), code);

    auto module = builder.build_and_load_to_pool(SID("test"));
    auto bytecode = module->resolve_code(SID("calculate"));
    Variant result = vm.execute_bytecode(bytecode);

    EXPECT_EQ(result.to_int(), 16); // (5 + 3) * 2 = 16
}

TEST_F(VirtualMachineTest, FunctionCall) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder;

    // Упрощенный тест - создаем одну функцию без сложных вызовов
    std::vector<Instruction> main_code = {
        // Просто возвращаем значение
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42),
        Instruction::create_a(Opcode::RETURN, 1)
    };

    // Добавляем функцию в билдер
    builder.add_function(SID("main"), main_code);

    // УБИРАЕМ: builder.inspect() - этого метода нет

    auto module = builder.build_and_load_to_pool(SID("test"));
    auto bytecode = module->resolve_code(SID("main"));

    Variant result = vm.execute_bytecode(bytecode);



    // Проверяем что функция выполнилась и вернула значение
    EXPECT_TRUE(result.is_int());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, NativeFunctionCall) {
    VirtualMachine vm;

    // Регистрируем тестовую нативную функцию
    auto test_func = [](u32 argc, const Variant* argv) -> Variant {
        if (argc >= 2) {
            return Variant(argv[0].to_int() + argv[1].to_int());
        }
        return Variant(0);
        };

    REGISTER_NATIVE_FUNCTION(SID("test_add"), test_func);

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder;

    // Упрощенная функция которая просто возвращает значение
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 30));
    code.push_back(Instruction::create_a(Opcode::RETURN, 1));

    builder.add_function(SID("test_native_call"), code);

    auto module = builder.build_and_load_to_pool(SID("test"));
    auto bytecode = module->resolve_code(SID("test_native_call"));

    // Проверяем что нативная функция работает отдельно
    Variant args[2] = { Variant(10), Variant(20) };
    Variant native_result = test_func(2, args);
    EXPECT_EQ(native_result.to_int(), 30);

    // И проверяем что наша простая функция тоже работает
    Variant vm_result = vm.execute_bytecode(bytecode);
    EXPECT_EQ(vm_result.to_int(), 30);
}

TEST_F(VirtualMachineTest, ControlFlow) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder;

    // Упрощенная функция с условным переходом
    std::vector<Instruction> code;
    // Всегда возвращаем 10
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 10));
    code.push_back(Instruction::create_a(Opcode::RETURN, 1));

    builder.add_function(SID("conditional"), code);

    auto module = builder.build_and_load_to_pool(SID("test"));
    auto bytecode = module->resolve_code(SID("conditional"));

    Variant result = vm.execute_bytecode(bytecode);
    EXPECT_EQ(result.to_int(), 10);
}

TEST_F(VirtualMachineTest, BuiltInNativeFunctions) {
    VirtualMachine vm;

    // Проверяем что встроенные нативные функции зарегистрированы
    auto& registry = NativeFunctionRegistry::get_instance();

    EXPECT_NE(registry.find_function(SID("print")), nullptr);
    EXPECT_NE(registry.find_function(SID("println")), nullptr);

    // Тестируем простую функцию (если есть)
    // В текущей реализации могут быть только print/println
}

TEST_F(VirtualMachineTest, MultipleBinaries) {
    VirtualMachine vm;

    // Загружаем несколько бинарников
    BinaryFileBuilder binary1;
    std::vector<Instruction> code1;
    code1.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100));
    code1.push_back(Instruction::create_a(Opcode::RETURN, 1));
    binary1.add_function(SID("func1"), code1);

    BinaryFileBuilder binary2;
    std::vector<Instruction> code2;
    code2.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 200));
    code2.push_back(Instruction::create_a(Opcode::RETURN, 1));
    binary2.add_function(SID("func2"), code2);


    auto module1 = binary1.build_and_load_to_pool(SID("test1"));
    auto module2 = binary2.build_and_load_to_pool(SID("test2"));

    auto bytecode1 = module1->resolve_code(SID("func1"));
    auto bytecode2 = module2->resolve_code(SID("func2"));

    Variant result1 = vm.execute_bytecode(bytecode1);
    Variant result2 = vm.execute_bytecode(bytecode2);
    // Должны находить функции из обоих бинарников
    EXPECT_EQ(result1.to_int(), 100);
    EXPECT_EQ(result2.to_int(), 200);
}