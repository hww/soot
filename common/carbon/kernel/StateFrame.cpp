// StateFrame.cpp
#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/kernel/StateFrame.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/carbon/kernel/Process.hpp"

namespace carbon::kernel {

// ============================================================================
// Конструктор
// ============================================================================

StateFrame::StateFrame(StateDesc* state_desc, std::shared_ptr<StackFrame> parent)
    : ProtectFrame(
        nullptr,  // нет основного кода во фрейме
        parent,
        [this]() { this->exit(); }  // cleanup function для ProtectFrame
    )
{
    if (state_desc) {
        // Устанавливаем имя фрейма
        name = state_desc->name;
        frame_type = FrameType::STATE;
        
        // Копируем обработчики из StateDesc (как в GOAL)
        enter_function_ = state_desc->get_enter_function();
        trans_function_ = state_desc->get_trans_function();
        code_function_ = state_desc->get_code_function();
        post_function_ = state_desc->get_post_function();
        exit_function_ = state_desc->get_exit_function();
        event_handler_ = state_desc->get_event_handler();
    }
    
    lg::debug("StateFrame created for state '{}'", name.to_string());
}

// ============================================================================
// Выполнение exit-обработчика
// ============================================================================

void StateFrame::execute_exit() {
    if (!exit_function_) {
        return;
    }
    
    lg::debug("StateFrame::execute_exit for state '{}'", name);
    
    // Создаём временный фрейм для exit-обработчика
    StackFrame* exit_frame = new StackFrame(
        exit_function_,
        nullptr,
        StackFrame::FrameType::GENERIC,
        SID("state_exit")
    );
;
    // Выполняем через VM
    // TODO: Получить VM из процесса или глобального контекста
    // VirtualMachine::instance().execute(exit_frame, owner_process_);
    
    delete exit_frame;
}

// ============================================================================
// Переопределенные методы ProtectFrame
// ============================================================================

void StateFrame::exit() {
    lg::debug("StateFrame::exit called for state '{}'", name);
    execute_exit();
}

void StateFrame::on_throw() {
    lg::warn("Exception in state '{}', calling exit handler", name);
    execute_exit();
}

// ============================================================================
// Отладочная информация
// ============================================================================

std::string StateFrame::to_string() const {
    return fmt::format(
        "StateFrame(state:'{}', handlers:[{}])",
        name,
        get_handler_hames()
    );
}

std::string StateFrame::inspect() const {
    return fmt::format(
        "StateFrame(state:'{}', handlers:[enter:{}, trans:{}, update:{}, post:{}, exit:{}, events:{}], pc:{}, parent:{})",
        name,
        has_enter() ? "yes" : "no",
        has_trans() ? "yes" : "no",
        has_code() ? "yes" : "no",
        has_post() ? "yes" : "no",
        has_exit() ? "yes" : "no",
        has_event(),
        pc,
        parent ? "yes" : "no"
    );
}

    std::string StateFrame::get_handler_hames() const {
        std::string result;
        
        if (has_enter()) result += " enter";
        if (has_trans()) result += " trans";
        if (has_code()) result += " code";
        if (has_post()) result += " post";
        if (has_exit()) result += " exit";
        if (has_event()) result += " event";
        return result.empty() ? "none" : result.substr(1);
    }

// ============================================================================
// Вспомогательные функции
// ============================================================================

StateFrame* create_state_frame(StateDesc* state_desc, std::shared_ptr<StackFrame> parent) {
    return new StateFrame(state_desc, parent);
}

void destroy_state_frame(StateFrame* frame) {
    delete frame;
}

// Функция с shared_ptr
std::shared_ptr<StateFrame> find_current_state_frame(std::shared_ptr<StackFrame> top_frame) {
    auto current = top_frame;
    while (current) {
        if (current->frame_type == StackFrame::FrameType::STATE) {
            return std::dynamic_pointer_cast<StateFrame>(current);
        }
        current = current->parent;  
    }
    return nullptr;
}

// Если нужно вернуть сырой указатель (не рекомендуется)
StateFrame* find_current_state_frame_raw(std::shared_ptr<StackFrame> top_frame) {
    auto current = top_frame;
    while (current) {
        if (current->frame_type == StackFrame::FrameType::STATE) {
            return dynamic_cast<StateFrame*>(current.get());
        }
        current = current->parent;
    }
    return nullptr;
}

} // namespace carbon::kernel