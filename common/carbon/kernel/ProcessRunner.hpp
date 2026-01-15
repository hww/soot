#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/kernel/Kernel.hpp"

namespace runtime::kernel {

    // ============================================================================
    // ProcessRunner Class - Упрощенный API для создания процессов
    // ============================================================================

    /// Вспомогательный класс для простого создания и запуска процессов
    /// Предоставляет высокоуровневый API аналогичный OpenGoal и C# системе
    class ProcessRunner {
    public:
        // ============================================================================
        // One-shot Process Creation
        // ============================================================================

        /// Создать и активировать процесс одной функцией
        /// @param name Имя процесса
        /// @param parent Родительский процесс
        /// @param entry_point Байткод точки входа
        /// @param stack_top Указатель на вершину стека
        /// @return Указатель на созданный процесс или nullptr при ошибке
        static Process* spawn(StringId name, Process* parent,
            ByteCode* entry_point, void* stack_top);

        /// Создать процесс с состоянием одной функцией
        /// @param name Имя процесса
        /// @param parent Родительский процесс
        /// @param initial_state Начальное состояние процесса
        /// @param stack_top Указатель на вершину стека
        /// @return Указатель на созданный процесс или nullptr при ошибке
        static Process* spawn_with_state(StringId name, Process* parent,
            StateDefinition* initial_state, void* stack_top);

        // ============================================================================
        // Two-stage Process Creation (как в OpenGoal)
        // ============================================================================

        /// Создать процесс (этап 1)
        /// @param name Имя процесса
        /// @return Указатель на созданный процесс
        static Process* create(StringId name);

        /// Активировать процесс (этап 2)
        /// @param process Процесс для активации
        /// @param parent Родительский процесс
        /// @param stack_top Указатель на вершину стека
        /// @return true если активация успешна
        static bool activate(Process* process, Process* parent, void* stack_top);

        /// Запустить функцию в процессе
        /// @param process Процесс для выполнения
        /// @param entry_point Байткод функции для запуска
        /// @return true если запуск успешен
        static bool run(Process* process, ByteCode* entry_point);

        // ============================================================================
        // Pool-based Creation (для оптимизации)
        // ============================================================================

        /// Создать процесс из пула (для частого переиспользования)
        /// @param name Имя процесса
        /// @return Указатель на процесс или nullptr если пул пуст
        static Process* create_from_pool(StringId name);

        /// Вернуть процесс в пул
        /// @param process Процесс для возврата
        static void return_to_pool(Process* process);

        // ============================================================================
        // Utility Functions
        // ============================================================================

        /// Создать и запустить процесс с главной функцией модуля
        /// @param module_name Имя модуля
        /// @param function_name Имя функции
        /// @param parent Родительский процесс
        /// @param stack_top Указатель на вершину стека
        /// @return Указатель на процесс или nullptr при ошибке
        static Process* spawn_module_function(StringId module_name, StringId function_name,
            Process* parent, void* stack_top);

        /// Убить процесс по имени
        /// @param name Имя процесса для уничтожения
        /// @return Количество уничтоженных процессов
        static u32 kill_by_name(StringId name);

        /// Убить все процессы определенного типа
        /// @param type_name Имя типа процессов
        /// @return Количество уничтоженных процессов
        static u32 kill_by_type(StringId type_name);

    private:
        // ============================================================================
        // Private Constructor
        // ============================================================================

        /// Приватный конструктор - класс статический
        ProcessRunner() = delete;
    };

} // namespace runtime::kernel