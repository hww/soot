#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/modules/ModuleManager.hpp"
#include "common/util/Log.hpp"

namespace example {

    // ============================================================================
    // Пример 1: Простое создание и запуск процесса
    // ============================================================================

    void example_simple_process() {
        lg::info("=== Example 1: Simple Process ===");

        // Инициализируем ядро
        if (!kernel::kernel().initialize()) {
            lg::error("Failed to initialize kernel");
            return;
        }

        // Создаем стек для процесса (в реальной системе это будет выделенная память)
        char process_stack[8192];
        void* stack_top = process_stack + sizeof(process_stack);

        // Получаем байткод функции (в реальной системе из модуля)
        ByteCode* main_function = /* получаем из модуля */ nullptr;

        // Создаем и запускаем процесс одной функцией
        auto* player_process = kernel::ProcessRunner::spawn(
            SID("player"),
            kernel::kernel().scheduler().root,
            main_function,
            stack_top
        );

        if (player_process) {
            lg::info("Successfully spawned player process");
        }

        // Выполняем несколько кадров
        for (int i = 0; i < 10; i++) {
            kernel::kernel().execute_frame();
        }

        kernel::kernel().shutdown();
    }

    // ============================================================================
    // Пример 2: Двухэтапное создание (как в OpenGoal)
    // ============================================================================

    void example_two_stage_creation() {
        lg::info("=== Example 2: Two-stage Creation ===");

        kernel::kernel().initialize();

        char process_stack[4096];
        void* stack_top = process_stack + sizeof(process_stack);

        // Этап 1: Создаем процесс
        auto* enemy_process = kernel::ProcessRunner::create(SID("enemy_ai"));

        // Можем настроить процесс перед активацией
        if (enemy_process) {
            enemy_process->add_mask(kernel::ProcessMask::SLEEP); // Начинаем спящим
        }

        // Этап 2: Активируем процесс
        kernel::ProcessRunner::activate(enemy_process,
            kernel::kernel().scheduler().root,
            stack_top);

        // Этап 3: Запускаем функцию (опционально)
        ByteCode* ai_function = /* получаем из модуля */ nullptr;
        kernel::ProcessRunner::run(enemy_process, ai_function);

        kernel::kernel().shutdown();
    }

    // ============================================================================
    // Пример 3: Работа с модулями
    // ============================================================================

    void example_module_based() {
        lg::info("=== Example 3: Module-based Process ===");

        kernel::kernel().initialize();

        char process_stack[8192];
        void* stack_top = process_stack + sizeof(process_stack);

        // Создаем процесс из функции модуля
        auto* game_process = kernel::ProcessRunner::spawn_module_function(
            SID("game"),          // Имя модуля
            SID("main_loop"),     // Имя функции  
            kernel::kernel().scheduler().root,
            stack_top
        );

        if (game_process) {
            lg::info("Successfully spawned game main loop process");
        }

        kernel::kernel().shutdown();
    }

    // ============================================================================
    // Пример 4: Нативные функции с доступом к контексту
    // ============================================================================

    // Нативная функция для усыпления текущего процесса
    void register_sleep_function() {
        // Эта функция будет зарегистрирована в NativeFunctionRegistry
        // и сможет быть вызвана из байткода

        // Пример реализации:
        /*
        REGISTER_NATIVE_FUNCTION("sleep", [](u32 argc, const Variant* argv) -> Variant {
            if (auto* proc = kernel::current_process()) {
                lg::debug("Process '{}' going to sleep", proc->get_name_string());
                proc->suspend();
            }
            return Variant();
        });
        */

        // Другие полезные нативные функции:
        /*
        REGISTER_NATIVE_FUNCTION("current_process_name", [](u32, const Variant*) -> Variant {
            auto* proc = kernel::current_process();
            return Variant(proc ? proc->get_name_string() : "null");
        });

        REGISTER_NATIVE_FUNCTION("in_trans_thread?", [](u32, const Variant*) -> Variant {
            return Variant(kernel::current_thread() == kernel::ThreadType::TRANS);
        });

        REGISTER_NATIVE_FUNCTION("global_pause", [](u32, const Variant*) -> Variant {
            kernel::kernel().add_global_mask(kernel::ProcessMask::PAUSE);
            return Variant(true);
        });
        */
    }

    // ============================================================================
    // Пример 5: Работа с глобальными масками
    // ============================================================================

    void example_global_masks() {
        lg::info("=== Example 5: Global Masks ===");

        kernel::kernel().initialize();

        // Устанавливаем глобальную маску паузы
        kernel::kernel().add_global_mask(kernel::ProcessMask::PAUSE);
        lg::info("Global pause mask set");

        // Проверяем маску
        if (kernel::kernel().global_mask() & kernel::ProcessMask::PAUSE) {
            lg::info("Game is paused - processes with PAUSE mask won't execute");
        }

        // Снимаем маску паузы
        kernel::kernel().remove_global_mask(kernel::ProcessMask::PAUSE);
        lg::info("Global pause mask removed");

        kernel::kernel().shutdown();
    }

    // ============================================================================
    // Главная функция примеров
    // ============================================================================

    void run_all_examples() {
        lg::info("Starting Kernel API Examples...");

        example_simple_process();
        example_two_stage_creation();
        example_module_based();
        example_global_masks();

        lg::info("All examples completed");
    }

} // namespace example

// Точка входа для примеров
int main() {
    example::run_all_examples();
    return 0;
}