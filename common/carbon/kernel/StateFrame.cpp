#include "common/carbon/kernel/StateFrame.hpp"
#include "common/carbon/kernel/StateDefinition.hpp"

namespace runtime::kernel {

    StateFrame::StateFrame(StateDefinition* definition, Process* process, StackFrame* parent)
        : ProtectFrame(
            definition ? definition->update_FunctionDesc : nullptr,
            parent,
            [this]() { this->execute_exit(); }
        ),
        state_def(definition),
        owner_process(process)
    {
        if (state_def) {
            name = state_def->name;

            enter_FunctionDesc = state_def->enter_FunctionDesc;
            trans_FunctionDesc = state_def->trans_FunctionDesc;
            update_FunctionDesc = state_def->update_FunctionDesc;
            post_FunctionDesc = state_def->post_FunctionDesc;
            event_FunctionDesc = state_def->event_FunctionDesc;
        }
    }

    void StateFrame::execute_enter() {
        if (enter_FunctionDesc && owner_process) {
            auto enter_frame = new StackFrame(enter_FunctionDesc, this, StackFrame::FrameType::GENERIC, SID("state_enter"));
            // execute_frame(enter_frame, owner_process);
            delete enter_frame;
        }
    }

    void StateFrame::execute_trans() {
        if (trans_FunctionDesc && owner_process) {
            auto trans_frame = new StackFrame(trans_FunctionDesc, this, StackFrame::FrameType::GENERIC, SID("state_trans"));
            // execute_frame(trans_frame, owner_process);
            delete trans_frame;
        }
    }

    void StateFrame::execute_update() {
        if (update_FunctionDesc && owner_process) {
            pc = 0; // Сбрасываем PC для выполнения с начала
            // execute_frame(this, owner_process);
        }
    }

    void StateFrame::execute_post() {
        if (post_FunctionDesc && owner_process) {
            auto post_frame = new StackFrame(post_FunctionDesc, this, StackFrame::FrameType::GENERIC, SID("state_post"));
            // execute_frame(post_frame, owner_process);
            delete post_frame;
        }
    }

    void StateFrame::execute_event(StringId event_type, const Variant& event_data) {
        if (event_FunctionDesc && owner_process) {
            auto event_frame = new StackFrame(event_FunctionDesc, this, StackFrame::FrameType::GENERIC, SID("state_event"));

            event_frame->get_argument(0) = Variant(event_type);
            event_frame->get_argument(1) = event_data;

            // execute_frame(event_frame, owner_process);
            delete event_frame;
        }
    }

    void StateFrame::execute_exit() {
        if (state_def && state_def->has_exit() && owner_process) {
            auto exit_frame = new StackFrame(state_def->exit_FunctionDesc, this, SID("state_exit"));
            // execute_frame(exit_frame, owner_process);
            delete exit_frame;
        }
    }

    std::string StateFrame::to_string() const {
        std::string state_name = state_def ? state_def->get_name_string() : "null";
        std::string process_name = owner_process ? owner_process->get_name_string() : "null";
        return std::format("StateFrame('{}', process:'{}')", state_name, process_name);
    }

    void StateFrame::dump_state_info() const {
        lg::debug("=== State Frame Info ===");
        lg::debug("  State: {}", state_def ? state_def->get_name_string() : "null");
        lg::debug("  Process: {}", owner_process ? owner_process->get_name_string() : "null");
        lg::debug("  Handlers: enter:{}, trans:{}, update:{}, post:{}, event:{}, exit:{}",
            has_enter(), has_trans(), has_update(), has_post(), has_event(), has_exit());
        lg::debug("  PC: {}, Parent: {}", pc, parent_ptr ? "yes" : "no");
    }

    void StateFrame::exit() {
        ProtectFrame::exit();
    }

    void StateFrame::on_throw() {
        lg::warn("Exception in state '{}', calling exit handler",
            state_def ? state_def->get_name_string() : "null");
        execute_exit();
    }

    StateFrame* create_state_frame(StateDefinition* state_def, Process* process, StackFrame* parent) {
        auto frame = new StateFrame(state_def, process, parent);
        frame->execute_enter();
        return frame;
    }

    void destroy_state_frame(StateFrame* frame) {
        if (frame) {
            delete frame;
        }
    }

    StateFrame* find_current_state_frame(StackFrame* top_frame) {
        StackFrame* current = top_frame;
        while (current) {
            if (current->frame_type == StackFrame::FrameType::STATE) {
                return static_cast<StateFrame*>(current);
            }
            current = current->parent_ptr;
        }
        return nullptr;
    }

} // namespace runtime::kernel