#include "common/carbon/modules/ModuleManager.hpp"
#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/util/Log.hpp"

using namespace runtime::modules;
using namespace runtime::kernel;
using namespace runtime::vm;

int main() {
    lg::info("=== Carbon VM Test ===");
    
    // 1. Инициализируем модульную систему
    ModuleRegistry& registry = ModuleRegistry::instance();
    registry.add_search_path(".");
    
    // 2. Сканируем модули
    registry.scan_and_index(true);
    
    // 3. Находим модуль math/add
    auto module = registry.find_module(SID("math/add"));
    if (!module) {
        lg::error("Module 'math/add' not found");
        lg::info("Available modules:");
        for (auto name : registry.get_available_modules()) {
            lg::info("  - {}", runtime::lib::to_string(name));
        }
        return 1;
    }
    
    lg::info("Module found: {}", module->to_string());
    
    // 4. Загружаем бинарные данные
    ModuleManager& mm = ModuleManager::instance();
    auto loaded_module = mm.load_module(SID("math/add"));
    if (!loaded_module) {
        lg::error("Failed to load module");
        return 1;
    }
    
    // 5. Находим функцию add
    ByteCode* add_code = loaded_module->resolve_code(SID("add"));
    if (!add_code) {
        lg::error("Function 'add' not found");
        return 1;
    }
    
    lg::info("Found function 'add'");
    
    // 6. Инициализируем ядро
    Kernel& kernel = Kernel::instance();
    if (!kernel.initialize()) {
        lg::error("Failed to initialize kernel");
        return 1;
    }
    
    // 7. Создаём процесс
    Process* process = ProcessRunner::spawn(
        SID("test_process"),
        kernel.root(),
        add_code,
        nullptr
    );
    
    if (!process) {
        lg::error("Failed to create process");
        kernel.shutdown();
        return 1;
    }
    
    lg::info("Process created: {}", process->to_string());
    
    // 8. Создаём фрейм и выполняем
    VirtualMachine& vm = kernel.virtual_machine();
    StackFrame* frame = new StackFrame(add_code, nullptr);
    
    // Устанавливаем аргументы
    frame->get_register(ARG_REGISTERS_OFFSET + 0) = Variant(5);
    frame->get_register(ARG_REGISTERS_OFFSET + 1) = Variant(3);
    frame->argc = 2;
    
    lg::info("Calling add(5, 3)...");
    Variant result = vm.execute(frame);
    lg::info("Result: 5 + 3 = {}", result.to_string());
    
    // 9. Очистка
    delete frame;
    kernel.shutdown();
    
    return 0;
}
