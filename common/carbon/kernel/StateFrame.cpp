#include "carbon/kernel/StateFrame.hpp"

#include "carbon/Carbon.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "util/Log.hpp"

namespace carbon {

    StateFrame::StateFrame(SsState *state_desc, std::shared_ptr<StackFrame> parent)
        : ProtectFrame(nullptr, parent, nullptr) {
        frame_type = FrameType::STATE;

        if (state_desc) {
            name = state_desc->state_id();
            enter_function_ = state_desc->get_enter_function();
            trans_function_ = state_desc->get_trans_function();
            code_function_ = state_desc->get_code_function();
            post_function_ = state_desc->get_post_function();
            exit_function_ = state_desc->get_exit_function();
        }
    }

    void StateFrame::exit() { execute_exit(); }

    void StateFrame::on_throw() { execute_exit(); }

    void StateFrame::execute_exit() {
        if (!exit_function_) return;

        // Run the exit handler on the VM. The frame has already been
        // unlinked from the process stack by pop_frame(), so we use the
        // Carbon singleton to reach the VM.
        Carbon *carbon = Carbon::current();
        if (!carbon) return;

        VirtualMachine &vm = *carbon->vm();

        auto exit_frame = std::make_shared<StackFrame>(exit_function_, nullptr, FrameType::GENERIC,
                                                       StringIds::exit);

        // Do NOT suspend during exit: it is a cleanup path.
        exit_frame->argc = 0;
        vm.execute(exit_frame);

        lg::debug("StateFrame: ran exit handler '{}'", name.to_string());
    }

    std::string StateFrame::to_string() const {
        return fmt::format("StateFrame(state='{}', pc={})", name.to_string(), pc);
    }

    std::string StateFrame::inspect() const {
        return fmt::format("StateFrame(name='{}', type=STATE, pc={}, has_enter={}, has_exit={})",
                           name.to_string(), pc, enter_function_ != nullptr,
                           exit_function_ != nullptr);
    }

} // namespace carbon