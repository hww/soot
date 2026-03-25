#include "common/carbon/modules/ModuleManager.hpp"
#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/util/Log.hpp"
#include "fmt/color.h"
#include <cstddef>
#include <memory>

using namespace carbon::modules;
using namespace carbon::kernel;
using namespace carbon::vm;

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
            lg::info("  - {}", name);
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
    if (loaded_module->binary_file==nullptr)
        fmt::print("module does not have binary file\n");
    else
        fmt::print("Loaded file\n{}", loaded_module->binary_file->inspect());

    // 5. Находим функцию add
    FunctionDesc* add_code = loaded_module->resolve_function(SID("add"));
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
        StringId("test_process"),
        kernel.root(),
        nullptr, // No entry point, we'll execute manually
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
    auto frame = std::make_shared<StackFrame>(add_code, nullptr);
    
    // Устанавливаем аргументы
    frame->set_argument(0, Variant(5));
    frame->set_argument( 1, Variant(3));
    frame->argc = 2;
    
    lg::info("Calling add(5, 3)...");
    Variant result = vm.execute(frame);
    lg::info("Result: 5 + 3 = {}", result.to_string());
    
    // 9. Очистка
    kernel.shutdown();
    
    return 0;
}
