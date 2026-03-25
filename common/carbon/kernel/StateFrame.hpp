// StateFrame.hpp
#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/files/StateDesc.hpp"

namespace carbon::kernel {

/**
 * @brief Фрейм для состояний процесса
 * 
 * Наследует от ProtectFrame для гарантированного вызова exit-обработчика
 * при разрушении фрейма (например, при выходе из состояния через throw или return).
 * 
 * В GOAL StateFrame копирует все обработчики состояния, но использует только exit.
 * Остальные обработчики (enter, trans, update, post, event) вызываются Process/StateMachine
 * напрямую из StateDesc. Копирование сделано для быстрого доступа, но фактически не используется.
 */
class StateFrame : public ProtectFrame {
public:
    /**
     * @brief Конструктор StateFrame
     * @param state_desc Определение состояния
     * @param process Процесс-владелец
     * @param parent Родительский фрейм
     */
    StateFrame(StateDesc* state_desc, std::shared_ptr<StackFrame> parent = nullptr);
    
    /**
     * @brief Деструктор
     */
    ~StateFrame() override = default;
    
    // ============================================================================
    // Обработчики состояния (копируются из StateDesc, но не выполняются здесь)
    // ============================================================================
    
    /// Проверка наличия обработчиков
    bool has_enter() const { return enter_function_ != nullptr; }
    bool has_exit() const { return exit_function_ != nullptr; }
    bool has_trans() const { return trans_function_ != nullptr; }
    bool has_code() const { return code_function_ != nullptr; }
    bool has_post() const { return post_function_ != nullptr; }
    bool has_event() const { return event_handler_ != nullptr; }
    
    /// Доступ к обработчикам (для Process/StateMachine)
    FunctionDesc* get_enter() const { return enter_function_; }
    FunctionDesc* get_trans() const { return trans_function_; }
    FunctionDesc* get_code() const { return code_function_; }
    FunctionDesc* get_post() const { return post_function_; }
    FunctionDesc* get_exit() const { return exit_function_; }
    FunctionDesc* get_event() const { return event_handler_; }
    
    // ============================================================================
    // Отладочная информация
    // ============================================================================
    
    std::string to_string() const;
    std::string inspect() const;
    
    // ============================================================================
    // Переопределенные методы ProtectFrame
    // ============================================================================
    
    /**
     * @brief Вызывается ProtectFrame при разрушении фрейма
     * Выполняет exit-обработчик состояния
     */
    void exit() override;
    
    /**
     * @brief Вызывается при исключении, проходящем через этот фрейм
     * Выполняет exit-обработчик для очистки
     */
    void on_throw() override;

private:
    /**
     * @brief Выполнить exit-обработчик
     */
    void execute_exit();

    /**
     * @brief Получить отчет о хендрепах состояния
     */
    std::string get_handler_hames() const;

    // ============================================================================
    // Данные
    // ============================================================================
    
    // Копии обработчиков из StateDesc (как в GOAL)
    FunctionDesc* enter_function_ = nullptr;
    FunctionDesc* trans_function_ = nullptr;
    FunctionDesc* code_function_ = nullptr;
    FunctionDesc* post_function_ = nullptr;
    FunctionDesc* exit_function_ = nullptr;
    FunctionDesc* event_handler_= nullptr;
};

// ============================================================================
// Вспомогательные функции
// ============================================================================

/// Создать StateFrame (автоматически вызовет exit при разрушении)
StateFrame* create_state_frame(StateDesc* state_desc, Process* process, StackFrame* parent = nullptr);

/// Удалить StateFrame (вызовет exit через ProtectFrame)
void destroy_state_frame(StateFrame* frame);

/// Найти текущий StateFrame в стеке
StateFrame* find_current_state_frame(StackFrame* top_frame);

} // namespace carbon::kernel