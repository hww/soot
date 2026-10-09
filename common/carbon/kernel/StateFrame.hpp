// StateFrame.hpp
#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/file/Export.hpp"

namespace carbon {

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
    StateFrame(SsState* state_desc, std::shared_ptr<StackFrame> parent = nullptr);
    
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
    
    /// Доступ к обработчикам (для Process/StateMachine)
    ScriptLambda* get_enter() const { return enter_function_; }
    ScriptLambda* get_trans() const { return trans_function_; }
    ScriptLambda* get_code() const { return code_function_; }
    ScriptLambda* get_post() const { return post_function_; }
    ScriptLambda* get_exit() const { return exit_function_; }
    
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

    // ============================================================================
    // Accessors
    // ============================================================================

    i64 get_static_int(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        return static_cast<i64>(data_ptr[offset]);
    }

    f64 get_static_float(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        u64 raw = data_ptr[offset];
        f32 f;
        std::memcpy(&f, &raw, sizeof(f32));
        return static_cast<f64>(f);
    }

    void *get_static_pointer(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        const u64 raw = data_ptr[offset];
        if (raw == 0) return nullptr;
        // raw — смещение относительно НАЧАЛА PAYLOAD'а функции.
        // byte_code указывает на ScriptLambda внутри payload'а (сразу после
        // заголовочного sid64), поэтому база = byte_code - sizeof(sid64).
        // sizeof(sid64) == 8.
        return reinterpret_cast<u8 *>(byte_code) - sizeof(sid64) + raw;
    }

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
    ScriptLambda* enter_function_ = nullptr;
    ScriptLambda* trans_function_ = nullptr;
    ScriptLambda* code_function_ = nullptr;
    ScriptLambda* post_function_ = nullptr;
    ScriptLambda* exit_function_ = nullptr;
};

// ============================================================================
// Вспомогательные функции
// ============================================================================

/// Создать StateFrame (автоматически вызовет exit при разрушении)
StateFrame* create_state_frame(SsState* state_desc, Process* process, StackFrame* parent = nullptr);

/// Удалить StateFrame (вызовет exit через ProtectFrame)
void destroy_state_frame(StateFrame* frame);

/// Найти текущий StateFrame в стеке
StateFrame* find_current_state_frame(StackFrame* top_frame);

} // namespace carbon