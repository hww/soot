#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/vm/StackFrame.hpp"

namespace runtime::kernel {
    /**
     * Фрейм для состояний процесса
     * Наследует от ProtectFrame для гарантированного вызова exit-обработчика
     */
    class StateFrame : public ProtectFrame {
    private:
        // Специфичные для состояния обработчики
        ByteCode* enter_bytecode = nullptr;    // При входе в состояние
        ByteCode* trans_bytecode = nullptr;    // Перед каждым обновлением  
        ByteCode* update_bytecode = nullptr;   // Основной код состояния
        ByteCode* post_bytecode = nullptr;     // После каждого обновления
        ByteCode* event_bytecode = nullptr;    // Обработчик событий
        ByteCode* exit_bytecode = nullptr;     // Обработчик событий

        // Мета-информация
        StateDefinition* state_def = nullptr;  // Определение состояния
        Process* owner_process = nullptr;      // Процесс-владелец

    public:
        /**
         * Конструктор StateFrame
         * @param state_def Определение состояния
         * @param owner_process Процесс-владелец
         * @param parent Родительский фрейм
         */
        StateFrame(StateDefinition* definition, Process* process, StackFrame* parent = nullptr);

        // ============================================================================
        // Обработчики состояния
        // ============================================================================

        /// Выполнить enter-обработчик (при входе в состояние)
        void execute_enter();

        /// Выполнить trans-обработчик (перед основным кодом)
        void execute_trans();

        /// Выполнить update-обработчик (основной код состояния)
        void execute_update();

        /// Выполнить post-обработчик (после основного кода)
        void execute_post();

        /// Выполнить event-обработчик
        void execute_event(StringId event_type, const Variant& event_data);

        /// Выполнить exit-обработчик (вызывается через ProtectFrame cleanup)
        void execute_exit();

        // ============================================================================
        // Методы доступа
        // ============================================================================

        StateDefinition* get_state_definition() const { return state_def; }
        Process* get_owner_process() const { return owner_process; }

        bool has_enter() const { return enter_bytecode != nullptr; }
        bool has_trans() const { return trans_bytecode != nullptr; }
        bool has_update() const { return update_bytecode != nullptr; }
        bool has_post() const { return post_bytecode != nullptr; }
        bool has_event() const { return event_bytecode != nullptr; }
        bool has_exit() const { return exit_bytecode != nullptr; }

        // ============================================================================
        // Отладочная информация
        // ============================================================================

        std::string to_string() const;

        /// Детальная информация о состоянии
        void dump_state_info() const;

        // ============================================================================
        // Переопределенные методы
        // ============================================================================

        void exit() override;
        void on_throw() override;
    };

    // ============================================================================
    // Функции управления StateFrame
    // ============================================================================

    /// Создать и активировать StateFrame
    StateFrame* create_state_frame(StateDefinition* state_def, Process* process, StackFrame* parent = nullptr);

    /// Удалить StateFrame (автоматически вызовет exit через ProtectFrame)
    void destroy_state_frame(StateFrame* frame);

    /// Получить текущий StateFrame из цепочки
    StateFrame* find_current_state_frame(StackFrame* top_frame);
} // namespace runtime::kernel