#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/kernel/Kernel.hpp"
#include "common/util/Log.hpp"
#include "file/DCScript.hpp"
#include "file/Globals.hpp"
#include "lib/StringId.hpp"
#include "vm/StackFrame.hpp"
#include <memory>

namespace carbon {

    // ============================================================================
    // One-shot Process Creation
    // ============================================================================

    Process* ProcessRunner::spawn(StringId name, Process* parent,
        ScriptLambda* entry_point, std::shared_ptr<StackFrame> stack_top) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot spawn process - Kernel not initialized");
            return nullptr;
        }

        // Этап 1: Создаем процесс
        Process* process = kernel().create_process(name);
        if (!process) {
            lg::error("Failed to create process '{}'", name);
            return nullptr;
        }

        // Этап 2: Активируем процесс
        if (!kernel().activate_process(process, parent, name, stack_top)) {
            lg::error("Failed to activate process '{}'", name);
            return nullptr;
        }

        // Этап 3: Запускаем функцию
        if (entry_point && !kernel().run_process_function(process, entry_point)) {
            lg::error("Failed to run function in process '{}'", name);
            return nullptr;
        }

        lg::debug("Spawned process '{}' with entry point", name);
        return process;
    }

    Process* ProcessRunner::spawn_with_state(StringId name, Process* parent,
        SsState* initial_state, std::shared_ptr<StackFrame> stack_top) {
        if (!initial_state) {
            lg::error("Cannot spawn process with state - invalid state definition");
            return nullptr;
        }

        Process* process = spawn(name, parent, nullptr, stack_top);
        if (!process) {
            return nullptr;
        }

        // Устанавливаем начальное состояние
        process->go_state(initial_state); // Если есть такой метод

        lg::debug("Spawned process '{}' with initial state '{}'",
            name, initial_state->name());
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
            lg::debug("Created process '{}' (stage 1)", name);
        }
        return process;
    }

    bool ProcessRunner::activate(Process* process, Process* parent, StringId name, std::shared_ptr<StackFrame> stack_top) {
        if (!process) {
            lg::error("Cannot activate process - invalid process");
            return false;
        }

        bool success = kernel().activate_process(process, parent, name, stack_top);
        if (success) {
            lg::debug("Activated process '{}' (stage 2)", process->get_name_string());
        }
        return success;
    }

    bool ProcessRunner::run(Process* process, ScriptLambda* entry_point) {
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
        lg::debug("Creating process '{}' from pool", name);
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
        Process* parent, std::shared_ptr<StackFrame> stack_top) {
        if (!kernel().is_initialized()) {
            lg::error("Cannot spawn module function - Kernel not initialized");
            return nullptr;
        }

        // Загружаем модуль
        if (!Globals::inst().load_module(module_name.to_string())) {
            lg::error("Module '{}' not found", module_name);
            return nullptr;
        }

        // Находим функцию
        auto function = reinterpret_cast<ScriptLambda*>(Globals::inst().lookup(function_name, StringIds::script_lambda));
        if (!function) {
            lg::error("Function '{}' not found in module '{}'", function_name, module_name);
            return nullptr;
        }

        // Создаем процесс с именем модуля.функция
        // StringId process_name = string_id::concat(module_name, SID("."), function_name);
        // Временно используем просто имя функции
        StringId process_name = function_name;

        lg::debug("Spawning process for {}.{}", module_name, function_name);

        return spawn(process_name, parent, function, stack_top);
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

        lg::debug("Killed {} processes with name '{}'", count, name);
        return count;
    }

    u32 ProcessRunner::kill_by_type(StringId type_name) {
        // В этой системе "тип" может быть определен по-разному
        // Например, по префиксу имени или через дополнительные метаданные
        // Пока просто возвращаем 0
        lg::debug("Kill by type not implemented yet for type '{}'", type_name);
        return 0;
    }

} // namespace carbon