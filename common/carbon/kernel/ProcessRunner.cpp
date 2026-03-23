#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/modules/ModuleManager.hpp"
#include "common/util/Log.hpp"

namespace runtime::kernel {

    // ============================================================================
    // One-shot Process Creation
    // ============================================================================

    Process* ProcessRunner::spawn(StringId name, Process* parent,
        FunctionDesc* entry_point, void* stack_top) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot spawn process - Kernel not initialized");
            return nullptr;
        }

        // Этап 1: Создаем процесс
        Process* process = kernel().create_process(name);
        if (!process) {
            lg::error("Failed to create process '{}'", lib::to_string(name));
            return nullptr;
        }

        // Этап 2: Активируем процесс
        if (!kernel().activate_process(process, parent, stack_top)) {
            lg::error("Failed to activate process '{}'", lib::to_string(name));
            return nullptr;
        }

        // Этап 3: Запускаем функцию
        if (entry_point && !kernel().run_process_function(process, entry_point)) {
            lg::error("Failed to run function in process '{}'", lib::to_string(name));
            return nullptr;
        }

        lg::debug("Spawned process '{}' with entry point", lib::to_string(name));
        return process;
    }

    Process* ProcessRunner::spawn_with_state(StringId name, Process* parent,
        StateDefinition* initial_state, void* stack_top) {
        if (!initial_state) {
            lg::error("Cannot spawn process with state - invalid state definition");
            return nullptr;
        }

        Process* process = spawn(name, parent, nullptr, stack_top);
        if (!process) {
            return nullptr;
        }

        // Устанавливаем начальное состояние
        // process->go_state(initial_state->name); // Если есть такой метод

        lg::debug("Spawned process '{}' with initial state '{}'",
            lib::to_string(name), lib::to_string(initial_state->name));
        return process;
    }

    // ============================================================================
    // Two-stage Process Creation
    // ============================================================================

    Process* ProcessRunner::create(StringId name) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot create process - Kernel not initialized");
            return nullptr;
        }

        Process* process = kernel().create_process(name);
        if (process) {
            lg::debug("Created process '{}' (stage 1)", lib::to_string(name));
        }
        return process;
    }

    bool ProcessRunner::activate(Process* process, Process* parent, void* stack_top) {
        if (!process) {
            lg::error("Cannot activate process - invalid process");
            return false;
        }

        bool success = kernel().activate_process(process, parent, stack_top);
        if (success) {
            lg::debug("Activated process '{}' (stage 2)", process->get_name_string());
        }
        return success;
    }

    bool ProcessRunner::run(Process* process, FunctionDesc* entry_point) {
        if (!process || !entry_point) {
            lg::error("Cannot run process - invalid parameters");
            return false;
        }

        bool success = kernel().run_process_function(process, entry_point);
        if (success) {
            lg::debug("Running function in process '{}'", process->get_name_string());
        }
        return success;
    }

    // ============================================================================
    // Pool-based Creation
    // ============================================================================

    Process* ProcessRunner::create_from_pool(StringId name) {
        // В реальной системе здесь будет логика пула процессов
        // Пока просто создаем новый процесс
        lg::debug("Creating process '{}' from pool", lib::to_string(name));
        return create(name);
    }

    void ProcessRunner::return_to_pool(Process* process) {
        if (!process) return;

        // В реальной системе здесь будет возврат в пул
        // Пока просто уничтожаем процесс
        lg::debug("Returning process '{}' to pool", process->get_name_string());
        // kernel().destroy_process(process); // Если будет такой метод
    }

    // ============================================================================
    // Utility Functions
    // ============================================================================

    Process* ProcessRunner::spawn_module_function(StringId module_name, StringId function_name,
        Process* parent, void* stack_top) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot spawn module function - Kernel not initialized");
            return nullptr;
        }

        // Загружаем модуль
        auto module = ModuleManager::instance().load_module(module_name);
        if (!module) {
            lg::error("Module '{}' not found", lib::to_string(module_name));
            return nullptr;
        }

        // Находим функцию
        auto FunctionDesc = module->resolve_code(function_name);
        if (!FunctionDesc) {
            lg::error("Function '{}' not found in module '{}'",
                lib::to_string(function_name), lib::to_string(module_name));
            return nullptr;
        }

        // Создаем процесс с именем модуля.функция
        // StringId process_name = string_id::concat(module_name, SID("."), function_name);
        // Временно используем просто имя функции
        StringId process_name = function_name;

        lg::debug("Spawning process for {}.{}",
            lib::to_string(module_name), lib::to_string(function_name));

        return spawn(process_name, parent, FunctionDesc, stack_top);
    }

    u32 ProcessRunner::kill_by_name(StringId name) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot kill processes - Kernel not initialized");
            return 0;
        }

        u32 count = 0;
        auto& scheduler = kernel().scheduler();

        // Ищем и уничтожаем все процессы с указанным именем
        scheduler.iterate_tree(scheduler.root, [&](Process* process) {
            if (process->name == name && process != scheduler.root) {
                // Устанавливаем статус DEAD для последующей очистки
                process->status = ProcessStatus::DEAD;
                count++;
                lg::debug("Killed process '{}'", process->get_name_string());
            }
            return true;
            });

        // Очищаем мертвые процессы
        // scheduler.cleanup_dead_processes(); // Если будет такой метод

        lg::debug("Killed {} processes with name '{}'", count, lib::to_string(name));
        return count;
    }

    u32 ProcessRunner::kill_by_type(StringId type_name) {
        // В этой системе "тип" может быть определен по-разному
        // Например, по префиксу имени или через дополнительные метаданные
        // Пока просто возвращаем 0
        lg::debug("Kill by type not implemented yet for type '{}'", lib::to_string(type_name));
        return 0;
    }

} // namespace runtime::kernel