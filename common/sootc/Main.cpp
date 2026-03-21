// test_sootc.cpp
#include "common/carbon/modules/ModuleManager.hpp"
#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/util/Log.hpp"

using namespace runtime::modules;
using namespace runtime::kernel;
using namespace runtime::vm;

int main() {
    lg::info("=== Testing SOOTC compiled module ===");
    
    // 1. Инициализируем модульную систему
    ModuleRegistry& registry = ModuleRegistry::instance();
    registry.add_search_path("."); // Ищем модули в текущей папке
    
    // 2. Сканируем и загружаем модуль add
    registry.scan_and_index(true);
    
    auto module = registry.find_module(SID("math/add"));
    if (!module) {
        lg::error("Module 'math/add' not found");
        return 1;
    }
    
    lg::info("Module loaded: {}", module->to_string());
    
    // 3. Загружаем бинарные данные модуля
    ModuleManager& mm = ModuleManager::instance();
    auto loaded_module = mm.load_module(SID("math/add"));
    if (!loaded_module) {
        lg::error("Failed to load module");
        return 1;
    }
    
    // 4. Инициализируем ядро
    Kernel& kernel = Kernel::instance();
    if (!kernel.initialize()) {
        lg::error("Failed to initialize kernel");
        return 1;
    }
    
    // 5. Создаем процесс с self pointer (согласно нашему базису)

    Process* process = ProcessRunner::spawn(
        SID("test_process"),
        kernel.root(),
        loaded_module->resolve_code(SID("add")),
        nullptr  // stack_top
    );
    
    if (!process) {
        lg::error("Failed to create process");
        return 1;
    }
    
    // Устанавливаем self pointer
    process->self = nullptr;
    
    lg::info("Process created: {}", process->to_string());
    
    // 6. Выполняем функцию
    VirtualMachine& vm = kernel.virtual_machine();
    
    // Создаем аргументы
    std::vector<Variant> args = {Variant(5), Variant(3)};
    
    // Выполняем
    Variant result = vm.execute_bytecode(loaded_module.get(), SID("add"));
    
    lg::info("Result: 5 + 3 = {}", result.to_string());
    
    // 7. Очистка
    kernel.shutdown();
    
    return 0;
}