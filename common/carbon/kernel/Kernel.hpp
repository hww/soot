#pragma once

/**
 * @file Kernel.hpp
 * @brief Главный синглтон системы - центральный координатор выполнения процессов
 * @ingroup kernel
 */

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/kernel/Types.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/Scheduler.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"

namespace runtime::kernel {

    /**
     * @class Kernel
     * @brief Центральный координатор всей системы выполнения процессов
     *
     * Аналог *kernel-context* из OpenGoal и ядра из C# системы. Управляет:
     * - Созданием и уничтожением процессов
     * - Контекстным переключением между процессами
     * - Глобальными масками выполнения
     * - Координацией работы планировщика и виртуальной машины
     *
     * @note Реализован как синглтон (Meyer's singleton)
     * @see Process, Scheduler, VirtualMachine
     *
     * @code
     * // Пример использования:
     * if (kernel().initialize()) {
     *     auto* process = kernel().create_process(SID("my_process"));
     *     kernel().activate_process(process, nullptr, stack_ptr);
     *     kernel().execute_frame();
     * }
     * @endcode
     */
    class Kernel {
    public:
        // ============================================================================
        // Singleton Access
        // ============================================================================

        /**
         * @brief Получить глобальный экземпляр ядра
         * @return Ссылка на единственный экземпляр Kernel
         *
         * @warning Не создавайте экземпляры Kernel вручную!
         * Всегда используйте этот метод для доступа к ядру.
         */
        static Kernel& instance() {
            static Kernel instance;
            return instance;
        }

        // ============================================================================
        // Lifecycle Management
        // ============================================================================

        /**
         * @brief Инициализировать ядро системы
         * @return true если инициализация успешна, false при ошибке
         *
         * Создает корневой процесс и инициализирует все подсистемы.
         * Должна быть вызвана перед любыми другими операциями с ядром.
         *
         * @post Создан корневой процесс с маской PROCESS_TREE
         * @post Инициализирован планировщик процессов
         * @post Установлен флаг initialized_ = true
         *
         * @see shutdown()
         */
        bool initialize();

        /**
         * @brief Завершить работу ядра и очистить все ресурсы
         *
         * Останавливает все процессы, очищает контекст выполнения
         * и освобождает все системные ресурсы.
         *
         * @post Все процессы остановлены и удалены (кроме корневого)
         * @post Контекст выполнения очищен
         * @post initialized_ = false
         *
         * @see initialize()
         */
        void shutdown();

        /**
         * @brief Проверить инициализировано ли ядро
         * @return true если ядро готово к работе
         */
        bool is_initialized() const { return initialized_; }

        // ============================================================================
        // Current Execution Context
        // ============================================================================

        Process* root() const { return root_; }

        /**
         * @brief Получить текущий выполняющийся процесс
         * @return Указатель на процесс или nullptr если нет текущего
         *
         * @note Возвращает процесс, который выполняется в данный момент
         * в основном потоке выполнения (ThreadType::MAIN)
         */
        Process* current_process() const { return current_process_; }

        /**
         * @brief Получить текущий тип потока выполнения
         * @return Текущий тип потока (MAIN, EVENT, etc.)
         */
        ThreadType current_thread() const { return current_thread_; }

        /**
         * @brief Установить текущий контекст выполнения
         * @param process Выполняемый процесс
         * @param thread_type Тип потока выполнения
         *
         * Используется для переключения контекста между процессами.
         * Всегда должен сопровождаться restore предыдущего контекста.
         *
         * @warning Не вызывайте напрямую! Используйте run_process_function()
         * @see run_process_function(), clear_current_context()
         */
        void set_current_context(Process* process, ThreadType thread_type) {
            current_process_ = process;
            current_thread_ = thread_type;
        }

        /**
         * @brief Очистить текущий контекст выполнения
         *
         * Сбрасывает текущий процесс и поток в значения по умолчанию.
         * Вызывается при завершении работы ядра.
         *
         * @post current_process_ = nullptr
         * @post current_thread_ = ThreadType::MAIN
         */
        void clear_current_context() {
            current_process_ = nullptr;
            current_thread_ = ThreadType::MAIN;
        }

        // ============================================================================
        // Global Mask Management
        // ============================================================================

        /**
         * @brief Получить глобальную маску выполнения
         * @return Текущая глобальная маска процессов
         *
         * Глобальная маска влияет на ВСЕ процессы в системе.
         * Если бит установлен в глобальной маске, соответствующие
         * процессы не будут выполняться.
         */
        ProcessMask global_mask() const { return global_mask_; }

        /**
         * @brief Установить глобальную маску выполнения
         * @param mask Новая глобальная маска
         */
        void set_global_mask(ProcessMask mask) { global_mask_ = mask; }

        /**
         * @brief Добавить маску к глобальной маске
         * @param mask Маска для добавления (битовое ИЛИ)
         */
        void add_global_mask(ProcessMask mask) { global_mask_ = global_mask_ | mask; }

        /**
         * @brief Удалить маску из глобальной маски
         * @param mask Маска для удаления (битовое И НЕ)
         */
        void remove_global_mask(ProcessMask mask) {
            global_mask_ = global_mask_ & ~mask;
        }

        /**
         * @brief Проверить можно ли выполнять процесс с учетом глобальных масок
         * @param process Процесс для проверки
         * @return true если процесс можно выполнять
         *
         * Проверяет:
         * - Процесс не nullptr и находится в runnable состоянии
         * - Глобальная маска не блокирует выполнение
         * - Маска процесса не содержит EXECUTE или SLEEP
         */
        bool should_execute_process(Process* process) const;

        // ============================================================================
        // Subsystem Access
        // ============================================================================

        /**
         * @brief Получить доступ к планировщику процессов
         * @return Ссылка на объект планировщика
         */
        Scheduler& scheduler() { return scheduler_; }

        /**
         * @brief Получить доступ к виртуальной машине
         * @return Ссылка на объект виртуальной машины
         */
        VirtualMachine& virtual_machine() { return virtual_machine_; }

        // ============================================================================
        // Process Management (2-stage как в OpenGoal)
        // ============================================================================

        /**
         * @brief Создать новый процесс (этап 1: выделение)
         * @param name Имя процесса
         * @return Указатель на созданный процесс или nullptr при ошибке
         *
         * Создает процесс в состоянии DEAD. Для активации необходимо
         * вызвать activate_process().
         *
         * @note Двухэтапное создание предотвращает выполнение
         * неинициализированных процессов.
         *
         * @see activate_process()
         */
        Process* create_process(StringId name);

        /**
         * @brief Активировать процесс (этап 2: инициализация)
         * @param process Процесс для активации
         * @param parent Родительский процесс в дереве (nullptr для корня)
         * @param stack_top Указатель на вершину стека для выполнения
         * @return true если активация успешна
         *
         * Переводит процесс из состояния DEAD в ACTIVE и добавляет
         * его в дерево процессов.
         *
         * @pre Процесс должен быть в состоянии DEAD
         * @pre Ядро должно быть инициализировано
         *
         * @see create_process()
         */
        bool activate_process(Process* process, Process* parent, void* stack_top);

        /**
         * @brief Запустить функцию в процессе
         * @param process Процесс для выполнения
         * @param entry_point Байткод функции для запуска
         * @return true если запуск успешен
         *
         * Временно переключает контекст на указанный процесс,
         * выполняет байткод, затем восстанавливает предыдущий контекст.
         *
         * @note Автоматически управляет контекстом выполнения
         * @note Безопасен для вложенных вызовов
         */
        bool run_process_function(Process* process, ByteCode* entry_point);

        // ============================================================================
        // Frame Execution
        // ============================================================================

        /**
         * @brief Выполнить один кадр системы (вызывается каждый игровой кадр)
         *
         * Обходит дерево процессов и выполняет кванты времени для всех
         * процессов, готовых к выполнению (с учетом масок).
         *
         * @post Все runnable процессы получили квант времени выполнения
         * @see should_execute_process()
         */
        void execute_frame();

        // ============================================================================
        // Utility Methods
        // ============================================================================

        /**
         * @brief Получить строковое представление состояния ядра
         * @return Строка с отладочной информацией о ядре
         */
        std::string to_string() const;

        /**
         * @brief Вывести отладочную информацию о ядре в лог
         *
         * Выводит:
         * - Статус инициализации
         * - Текущий процесс и поток
         * - Глобальную маску
         * - Статистику процессов
         * - Дерево процессов
         */
        void dump_debug_info() const;

    private:
        // ============================================================================
        // Private Constructor
        // ============================================================================

        /// Приватный конструктор для singleton
        Kernel() = default;

        // ============================================================================
        // Member Variables
        // ============================================================================

        Process* current_process_ = nullptr;      ///< Текущий выполняющийся процесс
        ThreadType current_thread_ = ThreadType::MAIN; ///< Текущий тип потока выполнения
        ProcessMask global_mask_ = ProcessMask::NONE;  ///< Глобальная маска выполнения
        Scheduler scheduler_;                     ///< Планировщик процессов
        VirtualMachine virtual_machine_;          ///< Виртуальная машина для выполнения байткода
        bool initialized_ = false;                ///< Флаг инициализации ядра
        Process* root_ = nullptr;                 ///< Корневой процесс дерева
    };

    // ============================================================================
    // Global Helper Functions
    // ============================================================================

    /**
     * @brief Глобальный доступ к ядру
     * @return Ссылка на глобальный экземпляр Kernel
     */
    inline Kernel& kernel() { return Kernel::instance(); }

    /**
     * @brief Получить текущий выполняющийся процесс
     * @return Указатель на текущий процесс или nullptr
     */
    inline Process* current_process() { return kernel().current_process(); }

    /**
     * @brief Получить текущий тип потока выполнения
     * @return Текущий тип потока
     */
    inline ThreadType current_thread() { return kernel().current_thread(); }

    /**
     * @brief Получить глобальную маску выполнения
     * @return Текущая глобальная маска
     */
    inline ProcessMask global_mask() { return kernel().global_mask(); }

} // namespace runtime::kernel