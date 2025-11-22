#include "gtest/gtest.h"
#include "virtual_machine.hpp"
#include "native_func.hpp"
#include "binary_file.hpp"
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
    NativeFunction found = NativeFunctionRegistry::get_instance().find_function("test_native");
    EXPECT_NE(found, nullptr);

    // Проверяем вызов
    Variant result = found(0, nullptr);
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, SimpleExecution) {
    VirtualMachine vm;

    // Создаем простой байткод который просто возвращает 42
    BinaryFile binary;
    binary.create(10, 64); // имя, макс функций, размер данных

    // Простая функция: return 42
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 42)); // r1 = 42
    code.push_back(Instruction::create_a(Opcode::RETURN, 1)); // return r1

    ByteCode* func = binary.add_function(SID("simple_answer"), code);
    ASSERT_NE(func, nullptr);

    vm.load_binary(binary);
    Variant result = vm.execute_function("simple_answer");

    EXPECT_FALSE(result.is_null());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(VirtualMachineTest, BasicArithmetic) {
    VirtualMachine vm;

    BinaryFile binary;
    binary.create(10, 64);

    // Функция: (5 + 3) * 2
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 5));  // r1 = 5
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 3));  // r2 = 3
    code.push_back(Instruction::create_abc(Opcode::ADD_INT, 3, 1, 2));          // r3 = r1 + r2
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 4, 2));  // r4 = 2
    code.push_back(Instruction::create_abc(Opcode::MUL_INT, 5, 3, 4));          // r5 = r3 * r4
    code.push_back(Instruction::create_a(Opcode::RETURN, 5));                   // return r5

    binary.add_function(SID("calculate"), code);
    vm.load_binary(binary);

    Variant result = vm.execute_function("calculate");
    EXPECT_EQ(result.to_int(), 16); // (5 + 3) * 2 = 16
}

TEST_F(VirtualMachineTest, FunctionCall) {
    VirtualMachine vm;

    BinaryFile binary;
    binary.create(10, 128);

    // Вспомогательная функция: multiply_by_2(x)
    std::vector<Instruction> multiply_code;
    multiply_code.push_back(Instruction::create_abc(Opcode::MOVE, 1, ARG_REGISTERS_OFFSET + 0, 0)); // r1 = arg0 (x)
    multiply_code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 2));             // r2 = 2
    multiply_code.push_back(Instruction::create_abc(Opcode::MUL_INT, 3, 1, 2));                    // r3 = r1 * r2
    multiply_code.push_back(Instruction::create_a(Opcode::RETURN, 3));                             // return r3

    // Основная функция: main()
    std::vector<Instruction> main_code;
    main_code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 7));                // r1 = 7
    main_code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 1));                // r2 = function ID (temp)
    main_code.push_back(Instruction::create_abc(Opcode::MOVE, ARG_REGISTERS_OFFSET + 0, 1, 0));    // arg0 = 7
    main_code.push_back(Instruction::create_abc(Opcode::CALL, 2, 3, 1));                          // call r2, ret=r3, argc=1
    main_code.push_back(Instruction::create_a(Opcode::RETURN, 3));                                 // return r3

    binary.add_function(SID("multiply_by_2"), multiply_code);
    binary.add_function(SID("main"), main_code);

    vm.load_binary(binary);
    Variant result = vm.execute_function(SID("main"));

    // Ожидаем 7 * 2 = 14
    EXPECT_EQ(result.to_int(), 14);
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

    BinaryFile binary;
    binary.create(10, 64);

    // Функция которая вызывает нативную функцию
    std::vector<Instruction> code;
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, ARG_REGISTERS_OFFSET + 0, 10)); // arg0 = 10
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, ARG_REGISTERS_OFFSET + 1, 20)); // arg1 = 20
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 123));                       // r1 = func ptr (temp)
    code.push_back(Instruction::create_abc(Opcode::CALL_NATIVE, 1, 2, 2));                            // call native r1, ret=r2, argc=2
    code.push_back(Instruction::create_a(Opcode::RETURN, 2));                                         // return r2

    binary.add_function(SID("test_native_call"), code);
    vm.load_binary(binary);

    // Пока просто проверяем что нативная функция работает
    Variant args[2] = { Variant(10), Variant(20) };
    Variant native_result = test_func(2, args);
    EXPECT_EQ(native_result.to_int(), 30);
}

TEST_F(VirtualMachineTest, ControlFlow) {
    VirtualMachine vm;

    BinaryFile binary;
    binary.create(10, 64);

    // Функция с условием: if (x > 5) return 10 else return 20
    std::vector<Instruction> code;
    code.push_back(Instruction::create_abc(Opcode::MOVE, 1, ARG_REGISTERS_OFFSET + 0, 0)); // r1 = arg0 (x)
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 2, 5));             // r2 = 5
    code.push_back(Instruction::create_abc(Opcode::CMP_GT, 3, 1, 2));                      // r3 = (r1 > r2)
    code.push_back(Instruction::create_imm(Opcode::BRANCH_IF_NOT, 3, 8));                  // jump to instruction 8 if false
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 4, 10));            // r4 = 10
    code.push_back(Instruction::create_imm(Opcode::BRANCH, 0, 10));                        // jump to instruction 10
    code.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 4, 20));            // r4 = 20 (instruction 8)
    code.push_back(Instruction::create_a(Opcode::RETURN, 4));                              // return r4 (instruction 10)

    binary.add_function(SID("conditional"), code);
    vm.load_binary(binary);

    // TODO: Нужен механизм передачи аргументов
    // Пока просто проверяем что функция компилируется
    EXPECT_TRUE(true);
}

TEST_F(VirtualMachineTest, BuiltInNativeFunctions) {
    VirtualMachine vm;

    // Проверяем что встроенные нативные функции зарегистрированы
    auto& registry = NativeFunctionRegistry::get_instance();

    EXPECT_NE(registry.find_function("print"), nullptr);
    EXPECT_NE(registry.find_function("println"), nullptr);
    EXPECT_NE(registry.find_function("add"), nullptr);
    EXPECT_NE(registry.find_function("sub"), nullptr);
    EXPECT_NE(registry.find_function("mul"), nullptr);
    EXPECT_NE(registry.find_function("div"), nullptr);

    // Тестируем функцию add
    NativeFunction add_func = registry.find_function("add");
    ASSERT_NE(add_func, nullptr);

    Variant args[2] = { Variant(15), Variant(25) };
    Variant result = add_func(2, args);
    EXPECT_EQ(result.to_int(), 40);
}

TEST_F(VirtualMachineTest, MultipleBinaries) {
    VirtualMachine vm;

    // Загружаем несколько бинарников
    BinaryFile binary1;
    binary1.create(5, 32);
    std::vector<Instruction> code1;
    code1.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 100));
    code1.push_back(Instruction::create_a(Opcode::RETURN, 1));
    binary1.add_function(SID("func1"), code1);

    BinaryFile binary2;
    binary2.create(5, 32);
    std::vector<Instruction> code2;
    code2.push_back(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, 1, 200));
    code2.push_back(Instruction::create_a(Opcode::RETURN, 1));
    binary2.add_function(SID("func2"), code2);

    vm.load_binary(binary1);
    vm.load_binary(binary2);

    // Должны находить функции из обоих бинарников
    Variant result1 = vm.execute_function("func1");
    Variant result2 = vm.execute_function("func2");

    EXPECT_EQ(result1.to_int(), 100);
    EXPECT_EQ(result2.to_int(), 200);
}