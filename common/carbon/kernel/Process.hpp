#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/lib/Types.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/kernel/StateDefinition.hpp"
#include "common/carbon/kernel/Connectable.hpp"
#include "common/carbon/kernel/Types.hpp"
#include <memory>
#include <functional>
#include <string>


namespace runtime::kernel {

    // ============================================================================
    // Process Class
    // ============================================================================

    class Process {
    public:
        // ============================================================================
        // Constants
        // ============================================================================

        static constexpr u32 INVALID_PROCESS_ID = 0; ///< Невалидный идентификатор процесса

        // ============================================================================
        // Constructors & Destructor
        // ============================================================================

        Process(StringId name);
        ~Process();

        // ============================================================================
        // Process Tree Structure
        // ============================================================================

        /// Родительский процесс в дереве процессов
        Process* parent = nullptr;

        /// Правый брат в дереве (linked list детей родителя)
        Process* brother = nullptr;

        /// Левый ребенок в дереве (первый дочерний процесс)
        Process* child = nullptr;

        /// Self-reference для стабильных указателей
        Process* self = this;

        // ============================================================================
        // Identification & Basic State
        // ============================================================================

        /// Уникальный идентификатор процесса
        u32 pid = 0;

        /// Имя процесса (для отладки и поиска)
        StringId name = 0;

        /// Текущий статус выполнения
        ProcessStatus status = ProcessStatus::DEAD;

        /// Маска процесса (флаги поведения)
        ProcessMask mask = ProcessMask::NONE;

        // ============================================================================
        // Memory Management & Pool
        // ============================================================================

        /// Пул, из которого был выделен этот процесс
        /// Определяет стратегию управления памятью
        DeadPool* pool = nullptr;

        /// База кучи процесса (начало выделенной памяти)
        void* heap_base = nullptr;

        /// Верх кучи процесса (конец выделенной памяти)
        void* heap_top = nullptr;

        /// Текущий указатель кучи (для аллокаций)
        void* heap_cur = nullptr;

        /// Размер выделенной памяти процесса в байтах
        u32 allocated_length = 0;

        // ============================================================================
        // Execution Threads
        // ============================================================================

        /// Главный поток процесса - может приостанавливаться и возобновляться
        /// Содержит контекст выполнения (регистры, стек) когда процесс suspended
        StackFrame* main_thread = nullptr;

        /// Текущий активный поток выполнения
        /// Может быть main_thread или временным потоком для trans/post обработчиков
        StackFrame* top_thread = nullptr;

        // ============================================================================
        // Stack Frame Management
        // ============================================================================

        /// Верхний стековый фрейм процесса
        /// Цепочка фреймов для управления временем жизни (catch/protect/state)
        StackFrame* stack_frame_top = nullptr;

        // ============================================================================
        // State Management
        // ============================================================================

        /// Текущий фрейм состояния процесса
        StackFrame* current_state_frame = nullptr;

        /// Текущее состояние процесса (определяет поведение)
        StateDefinition* current_state = nullptr;

        /// Следующее состояние (для отложенных переходов)
        StateDefinition* next_state = nullptr;

        // ============================================================================
        // State Hooks (обработчики из текущего состояния)
        // ============================================================================

        /// Trans-обработчик - выполняется ПЕРЕД основным кодом каждый кадр
        /// Используется для подготовки данных, проверки условий
        ByteCode* trans_hook = nullptr;

        /// Post-обработчик - выполняется ПОСЛЕ основного кода каждый кадр  
        /// Используется для очистки, финализации, пост-обработки
        ByteCode* post_hook = nullptr;

        /// Event-обработчик - обрабатывает события, отправленные процессу
        ByteCode* event_hook = nullptr;

        // ============================================================================
        // Entity & Game World Integration
        // ============================================================================

        /// Связанная игровая entity (если процесс представляет игровой объект)
        EntityActor* entity = nullptr;

        // ============================================================================
        // Inter-Process Communication
        // ============================================================================

        /// Список соединений с другими процессами и системами
        Connectable ConnectionList;

        // ============================================================================
        // PID generator
        // ============================================================================

        static u32 next_pid_; ///< Счетчик для генерации PID

        // ============================================================================
        // Public Methods
        // ============================================================================

        // === State Management ===

        /// Перейти в указанное состояние
        /// @param state Идентификатор состояния для перехода
        /// @return true если переход успешно запланирован
        bool go_state(StringId state);

        /// Отправить событие процессу
        /// @param event Тип события
        /// @param argc Количество аргументов
        /// @param argv Массив аргументов события
        /// @return true если событие было обработано
        bool send_event(StringId event, u32 argc = 0, Variant* argv = nullptr);

        /// Проверить наличие отложенного перехода между состояниями
        bool has_pending_transition() const { return next_state != nullptr; }

        // === Mask Operations ===

        /// Проверить наличие маски у процесса
        bool has_mask(ProcessMask check) const {
            return (static_cast<u32>(mask) & static_cast<u32>(check)) != 0;
        }

        /// Добавить маску процессу
        void add_mask(ProcessMask add) {
            mask = static_cast<ProcessMask>(static_cast<u32>(mask) | static_cast<u32>(add));
        }

        /// Удалить маску у процесса
        void remove_mask(ProcessMask remove) {
            mask = static_cast<ProcessMask>(static_cast<u32>(mask) & ~static_cast<u32>(remove));
        }

        // === Status Checks ===

        bool is_dead() const { return status == ProcessStatus::DEAD; }

        /// Проверить, может ли процесс быть выполнен
        bool is_runnable() const {
            return (status == ProcessStatus::READY || status == ProcessStatus::SUSPENDED) &&
                !has_mask(ProcessMask::EXECUTE) &&
                !has_mask(ProcessMask::SLEEP);
        }

        bool is_currently_running() const { return status == ProcessStatus::RUNNING; }

        /// Проверить, выполняется ли основной код процесса
        bool is_code_runnable() const {
            return is_runnable() && !has_mask(ProcessMask::SLEEP_CODE);
        }

        // === Stack Operations ===

        /// Добавить фрейм в стек процесса
        void push_frame(StackFrame* frame);

        /// Удалить верхний фрейм из стека процесса
        StackFrame* pop_frame();

        /// Найти фрейм по имени в стеке процесса
        StackFrame* find_frame(StringId frame_name);

        // === Memory Management ===

        /// Выделить память из кучи процесса
        void* heap_alloc(u32 size);

        /// Освободить память в куче процесса (в большинстве случаев не используется)
        void heap_free(void* ptr);

        /// Получить объем используемой памяти в куче
        u32 heap_used() const;

        /// Получить общий размер кучи процесса
        u32 heap_size() const { return allocated_length; }

        // === Execution Control ===

        /// Выполнить один квант времени процесса
        /// @return true если процесс должен продолжать выполняться
        bool execute_quantum();

        /// Приостановить выполнение процесса до следующего кадра
        void suspend();

        /// Возобновить выполнение приостановленного процесса
        void resume();

        /// Активировать процесс (подготовить к выполнению)
        /// @param stack_top Указатель на верх стека для выполнения
        void activate(void* stack_top);

        // === Connection Management ===

        /// Соединить с другим процессом
        void connect_to(Process* other, StringId connection_type);

        /// Разорвать соединение с другим процессом
        void disconnect_from(Process* other);

        /// Разорвать все соединения процесса
        void disconnect_all();

        // === Tree Operations ===

        /// Добавить дочерний процесс
        void add_child(Process* child);

        /// Удалить дочерний процесс
        void remove_child(Process* child);

        /// Найти процесс по имени в поддереве
        Process* find_child(StringId process_name);

        /// Выполнить функцию для всех дочерних процессов
        void for_each_child(std::function<void(Process*)> func);

        // === Utility Methods ===

        std::string to_string() const;

        /// Получить имя процесса как строку
        std::string get_name_string() const;

        /// Дамп информации о процессе для отладки
        void dump_info() const;

    private:
        // ============================================================================
        // Private Methods
        // ============================================================================

        /**
         * @brief Сгенерировать новый уникальный PID
         * @return Уникальный идентификатор процесса
         *
         * Ищет свободный PID, пропуская INVALID_PROCESS_ID и уже занятые.
         */
        u32 generate_pid();

        /// Выполнить немедленный переход между состояниями
        bool execute_immediate_transition(StateDefinition* new_state);

        /// Выполнить отложенный переход между состояниями
        bool execute_deferred_transition(StateDefinition* new_state);

        /// Обновить обработчики из текущего состояния
        void update_state_hooks();

        /// Выполнить trans-обработчик
        void execute_trans_handler();

        /// Выполнить post-обработчик  
        void execute_post_handler();

        /// Очистить процесс (при деактивации)
        void cleanup();
    };

    // ============================================================================
    // Inline Functions
    // ============================================================================

    inline std::string Process::get_name_string() const {
        return lib::to_string(name);
    }

    inline u32 Process::heap_used() const {
        if (!heap_base || !heap_cur) return 0;
        return static_cast<u32>(reinterpret_cast<uintptr_t>(heap_cur) -
            reinterpret_cast<uintptr_t>(heap_base));
    }

} // namespace vm