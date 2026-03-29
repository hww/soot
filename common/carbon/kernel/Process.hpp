#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/kernel/Types.hpp"
#include "common/carbon/kernel/Connectable.hpp"
#include "kernel/EventMessage.hpp"
#include <functional>
#include <memory>
#include <string>


using namespace carbon::files;
using namespace carbon::vm;

namespace carbon::kernel {

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
        // Process Tree Structure
        // ============================================================================

        /// Родительский процесс в дереве процессов
        Process* parent = nullptr;

        /// Правый брат в дереве (linked list детей родителя)
        Process* brother = nullptr;

        /// Левый ребенок в дереве (первый дочерний процесс)
        Process* child = nullptr;

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

        /// Тип процесса, имеется в виду структура бинарного файла
        TypeDesc* type = nullptr;

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
        // Stack Frame Management
        // ============================================================================

        /// Главный поток процесса - может приостанавливаться и возобновляться
        /// Содержит контекст выполнения (регистры, стек) когда процесс suspended
        std::shared_ptr<StackFrame> main_thread = nullptr;

        /// Текущий активный поток выполнения
        /// Может быть main_thread или временным потоком для trans/post обработчиков
        std::shared_ptr<StackFrame> top_thread = nullptr;

        /// Верхний стековый фрейм процесса
        /// Цепочка фреймов для управления временем жизни (catch/protect/state)
        std::shared_ptr<StackFrame> stack_frame_top = nullptr;

        // ============================================================================
        // State Management
        // ============================================================================

        /// Текущий фрейм состояния процесса
        std::shared_ptr<StackFrame> current_state_frame = nullptr;

        /// Текущее состояние процесса (определяет поведение)
        StateDesc* current_state = nullptr;

        /// Следующее состояние (для отложенных переходов)
        StateDesc* next_state = nullptr;

        // ============================================================================
        // State Hooks (обработчики из текущего состояния)
        // ============================================================================

        /// Trans-обработчик - выполняется ПЕРЕД основным кодом каждый кадр
        /// Используется для подготовки данных, проверки условий
        FunctionDesc* trans_hook = nullptr;

        /// Post-обработчик - выполняется ПОСЛЕ основного кода каждый кадр  
        /// Используется для очистки, финализации, пост-обработки
        FunctionDesc* post_hook = nullptr;

        /// Event-обработчик - обрабатывает события, отправленные процессу
        FunctionDesc* event_handler = nullptr;

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
        // Constructors & Destructor
        // ============================================================================

        Process();
        ~Process();

        const char* name_cstr() { return name.to_cstring(); }
        std::string name_str() { return name.to_string(); }

        // ============================================================================
        // === State Management ===
        // ============================================================================

        /// Перейти в указанное состояние
        /// @param state Идентификатор состояния для перехода
        /// @return true если переход успешно запланирован
        bool go_state(StringId state);

        /// Перейти в указанное состояние
        /// @param state Идентификатор состояния для перехода
        /// @return true если переход успешно запланирован
        bool go_state(StateDesc* state);

        /// Отправить событие от этого процесса процессу
        /// @param target Получатель события
        /// @param event Тип события
        /// @param argc Количество аргументов
        /// @param argv Массив аргументов события
        /// @return true если событие было обработано
        /// TODO! Сделать стандартный varargs!
        bool send_event(Process* target, u32 argc, StringId event,  Variant* argv = nullptr);

        /// Отправить событие от этого процесса процессу
        /// @param target Получатель события
        /// @param event Тип события
        /// @param argc Количество аргументов
        /// @param argv Массив аргументов события
        /// @return true если событие было обработано
        bool send_event(Process* target, u32 argc, StringId event, EventMessage* argv = nullptr);

        /// Проверить наличие отложенного перехода между состояниями
        bool has_pending_transition() const { return next_state != nullptr; }

        // ============================================================================
        // === Mask Operations ===
        // ============================================================================

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

        // ============================================================================
        // === Status Checks ===
        // ============================================================================

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

        // ============================================================================
        // === Stack Operations ===
        // ============================================================================

        /// Добавить фрейм в стек процесса
        void push_frame(std::shared_ptr<StackFrame> frame);

        /// Удалить верхний фрейм из стека процесса
        std::shared_ptr<StackFrame> pop_frame();

        /// Найти фрейм по имени в стеке процесса
        std::shared_ptr<StackFrame> find_frame(StringId frame_name);

        // ============================================================================
        // === Memory Management ===
        // ============================================================================

        /// Выделить память из кучи процесса
        void* heap_alloc(u32 size);

        /// Освободить память в куче процесса (в большинстве случаев не используется)
        void heap_free(void* ptr);

        /// Получить объем используемой памяти в куче
        u32 heap_used() const;

        /// Получить общий размер кучи процесса
        u32 heap_size() const { return allocated_length; }

        // ============================================================================
        // === Execution Control ===
        // ============================================================================

        /// Выполнить один квант времени процесса
        /// @return true если процесс должен продолжать выполняться
        bool execute_quantum();

        /// Приостановить выполнение процесса до следующего кадра
        void suspend();

        /// Возобновить выполнение приостановленного процесса
        void resume();

        /// Активировать процесс (подготовить к выполнению)
        /// @param stack_top Указатель на верх стека для выполнения
        void activate(Process* active_pool, StringId name, std::shared_ptr<StackFrame> stack_top = nullptr);

        // ============================================================================
        // === Connection Management ===
        // ============================================================================

        /// Соединить с другим процессом
        void connect_to(Process* other, StringId connection_type);

        /// Разорвать соединение с другим процессом
        void disconnect_from(Process* other);

        /// Разорвать все соединения процесса
        void disconnect_all();

        // ============================================================================
        // === Tree Operations ===
        // ============================================================================

        /// Добавить дочерний процесс
        void add_child(Process* child);

        /// Удалить дочерний процесс
        void remove_child(Process* child);

        /// Найти процесс по имени в поддереве
        Process* find_child(StringId process_name);

        /// Выполнить функцию для всех дочерних процессов
        void for_each_child(std::function<void(Process*)> func);

        // ============================================================================
        // === Utility Methods ===
        // ============================================================================

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
        bool execute_immediate_transition(StateDesc* new_state);

        /// Выполнить отложенный переход между состояниями
        bool execute_deferred_transition(StateDesc* new_state);

        /// Обновить обработчики из текущего состояния
        void update_state_hooks(VirtualMachine& vm);

        /// Выполнить post-обработчик  
        void execute_enter_handler(VirtualMachine& vm, FunctionDesc* enter_hook);

        /// Выполнить trans-обработчик
        void execute_trans_handler(VirtualMachine& vm);

        /// Выполнить post-обработчик  
        void execute_post_handler(VirtualMachine& vm);

        /// Получение сообщение
        bool execute_event(Process* sender, u32 argc, StringId event, EventMessage* message = nullptr);

        /// Очистить процесс (при деактивации)
        void cleanup();

        /// Наличие делегатов
        bool has_event_handler() { return event_handler != nullptr; }
    };

    // ============================================================================
    // Inline Functions
    // ============================================================================

    inline std::string Process::get_name_string() const {
        return name.to_string();
    }

    inline u32 Process::heap_used() const {
        if (!heap_base || !heap_cur) return 0;
        return static_cast<u32>(reinterpret_cast<uintptr_t>(heap_cur) -
            reinterpret_cast<uintptr_t>(heap_base));
    }

} // namespace vm