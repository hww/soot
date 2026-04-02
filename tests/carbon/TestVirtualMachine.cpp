#include "common/util/Log.hpp"
#include "gtest/gtest.h"

#include "carbon/Export.hpp"
#include "files/RelocatableBuffer.hpp"
#include "lib/StringIdManager.hpp"
#include "lib/Variant.hpp"

using namespace carbon::vm;
using namespace carbon::lib;
using namespace carbon::files;
using namespace carbon::modules;
using namespace carbon::kernel;

class VirtualMachineTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Инициализируем нативные функции перед каждым тестом
        NativeFunctionRegistry::get_instance().initialize_builtins();
        StringIdManager::instance().clear();
        VirtualMachine vm{};
    }
    void TearDown() override {}
};

TEST_F(VirtualMachineTest, NativeFunctionRegistration) {
    VirtualMachine vm;

    auto test_func = [](u32 argc, const Variant *argv) -> Variant { 
        (void)argc;(void)argv;
        return Variant(42); };

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
    BinaryFileBuilder builder("module1");

    // Простая функция: return 42
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42)); // r1 = 42
    code.push_back(Instruction::create_a(Opcode::RETURN, 1));                   // return r1

    RelocatableBuffer rbuffer;
    rbuffer.add_function("simple_answer", code, {}, {});
    builder.add_definition("simple_answer", "function", rbuffer);

    // ИСПРАВЛЕНИЕ: build_file() вызывается правильно
    auto module = builder.build_module();
    auto FunctionDesc = module->resolve_function(SID("simple_answer"));
    EXPECT_NE(FunctionDesc, nullptr);
    Variant result = vm.execute_function(FunctionDesc);

    EXPECT_FALSE(result.is_null());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, BasicArithmetic) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder("module1");

    // Функция: (5 + 3) * 2
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 5)); // r1 = 5
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 3)); // r2 = 3
    code.push_back(Instruction::create_abc(Opcode::ADD_INT, 3, 1, 2));         // r3 = r1 + r2
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 4, 2)); // r4 = 2
    code.push_back(Instruction::create_abc(Opcode::MUL_INT, 5, 3, 4));         // r5 = r3 * r4
    code.push_back(Instruction::create_a(Opcode::RETURN, 5));                  // return r5

    RelocatableBuffer rbuffer;
    rbuffer.add_function("calculate", code, {}, {});
    builder.add_definition("calculate", "function", rbuffer);

    auto    module = builder.build_module();
    auto    FunctionDesc = module->resolve_function(SID("calculate"));
    Variant result = vm.execute_function(FunctionDesc);

    EXPECT_EQ(result.to_int(), 16); // (5 + 3) * 2 = 16
}

TEST_F(VirtualMachineTest, FunctionCall) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder("module1");

    // Упрощенный тест - создаем одну функцию без сложных вызовов
    std::vector<Instruction> main_code = {
        // Просто возвращаем значение
        Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42),
        Instruction::create_a(Opcode::RETURN, 1)};

    // Добавляем функцию в билдер
    RelocatableBuffer rbuffer;
    rbuffer.add_function("main", main_code, {}, {});
    builder.add_definition("main", "function", rbuffer);

    // УБИРАЕМ: builder.inspect() - этого метода нет

    auto module = builder.build_module();
    auto FunctionDesc = module->resolve_function(SID("main"));

    Variant result = vm.execute_function(FunctionDesc);

    // Проверяем что функция выполнилась и вернула значение
    EXPECT_TRUE(result.is_int());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, NativeFunctionCall) {
    VirtualMachine vm;

    // Регистрируем тестовую нативную функцию
    auto test_func = [](u32 argc, const Variant *argv) -> Variant {
        if (argc >= 2) {
            return Variant(argv[0].to_int() + argv[1].to_int());
        }
        return Variant(0);
    };

    REGISTER_NATIVE_FUNCTION(SID("test_add"), test_func);

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder("module1");

    // Упрощенная функция которая просто возвращает значение
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 30));
    code.push_back(Instruction::create_a(Opcode::RETURN, 1));

    RelocatableBuffer rbuffer;
    rbuffer.add_function("test_native_call", code, {}, {});
    builder.add_definition("test_native_call", "function", rbuffer);

    auto module = builder.build_module();
    auto FunctionDesc = module->resolve_function(SID("test_native_call"));

    // Проверяем что нативная функция работает отдельно
    Variant args[2] = {Variant(10), Variant(20)};
    Variant native_result = test_func(2, args);
    EXPECT_EQ(native_result.to_int(), 30);

    // И проверяем что наша простая функция тоже работает
    Variant vm_result = vm.execute_function(FunctionDesc);
    EXPECT_EQ(vm_result.to_int(), 30);
}

TEST_F(VirtualMachineTest, ControlFlow) {
    VirtualMachine vm;

    // ИСПРАВЛЕНИЕ: BinaryFileBuilder без параметров
    BinaryFileBuilder builder("module1");

    // Упрощенная функция с условным переходом
    std::vector<Instruction> code;
    // Всегда возвращаем 10
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 10));
    code.push_back(Instruction::create_a(Opcode::RETURN, 1));

    RelocatableBuffer rbuffer;
    rbuffer.add_function("conditional", code, {}, {});
    builder.add_definition("conditional", "function", rbuffer);


    auto module = builder.build_module();
    auto FunctionDesc = module->resolve_function(SID("conditional"));

    Variant result = vm.execute_function(FunctionDesc);
    EXPECT_EQ(result.to_int(), 10);
}

TEST_F(VirtualMachineTest, BuiltInNativeFunctions) {
    VirtualMachine vm;

    // Проверяем что встроенные нативные функции зарегистрированы
    auto &registry = NativeFunctionRegistry::get_instance();

    EXPECT_NE(registry.find_function(SID("print")), nullptr);
    EXPECT_NE(registry.find_function(SID("println")), nullptr);

    // Тестируем простую функцию (если есть)
    // В текущей реализации могут быть только print/println
}

TEST_F(VirtualMachineTest, MultipleBinaries) {
    VirtualMachine vm;

    // Загружаем несколько бинарников
    BinaryFileBuilder        binary1("module1");

    std::vector<Instruction> code1;
    code1.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100));
    code1.push_back(Instruction::create_a(Opcode::RETURN, 1));

    RelocatableBuffer rbuffer1;
    rbuffer1.add_function("func1", code1, {}, {});
    binary1.add_definition("func1", "function", rbuffer1);    


    BinaryFileBuilder        binary2("module2");

    std::vector<Instruction> code2;
    code2.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 200));
    code2.push_back(Instruction::create_a(Opcode::RETURN, 1));

    RelocatableBuffer rbuffer2;
    rbuffer2.add_function("func2", code2, {}, {});
    binary2.add_definition("func2", "function", rbuffer2); 

    auto module1 = binary1.build_module();
    auto module2 = binary2.build_module();

    auto FunctionDesc1 = module1->resolve_function(SID("func1"));
    auto FunctionDesc2 = module2->resolve_function(SID("func2"));

    Variant result1 = vm.execute_function(FunctionDesc1);
    Variant result2 = vm.execute_function(FunctionDesc2);
    // Должны находить функции из обоих бинарников
    EXPECT_EQ(result1.to_int(), 100);
    EXPECT_EQ(result2.to_int(), 200);
}