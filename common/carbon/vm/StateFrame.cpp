#include "carbon/vm/StateFrame.hpp"
#include "carbon/Carbon.hpp"
#include "carbon/vm/TrackFrame.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "util/Log.hpp"

namespace carbon {

    StateFrame::StateFrame(SsState *state_desc, std::shared_ptr<StackFrame> parent)
        : StackFrame(nullptr, parent, FrameType::STATE,
                     state_desc ? state_desc->state_id() : StringIds::none),
          state(state_desc) {
        if (state_desc) {
            enter_function_ = state_desc->get_enter_function();
            trans_function_ = state_desc->get_trans_function();
            code_function_ = state_desc->get_code_function();
            post_function_ = state_desc->get_post_function();
            exit_function_ = state_desc->get_exit_function();
        }
    }

    void StateFrame::tick(VirtualMachine &vm) {
        // Walk the child list in order. A yield from any track stops
        // the whole state for this tick: remaining tracks see the same
        // time, they do not advance ahead of the one that parked.
        StackFrame *child = m_firstChild.get();
        while (child) {
            if (auto *track = dynamic_cast<TrackFrame *>(child)) {
                track->tick(vm);
                if (vm.is_yielded) return;
            }
            child = child->m_nextSibling.get();
        }
    }

    void StateFrame::exit() {
        // 1. Run the on-exit handler while the state's data is still alive.
        execute_exit();

        // 2. Free the subtree (TrackFrames + their main frames).
        //    free_children() walks m_firstChild -> m_nextSibling,
        //    calls exit() on each child, and releases everything.
        free_children();
    }

    void StateFrame::execute_exit() {
        if (!exit_function_) return;

        Carbon *carbon = Carbon::current();
        if (!carbon) return;

        VirtualMachine &vm = *carbon->vm();

        // A plain frame, not attached to the tree: it is a one-shot
        // call and must not be freed twice.
        auto exit_frame = std::make_shared<StackFrame>(exit_function_, nullptr, FrameType::GENERIC,
                                                       StringIds::exit);
        exit_frame->argc = 0;
        vm.execute(exit_frame);
    }

    TrackFrame *StateFrame::find_track(StringId id) {
        StackFrame *child = m_firstChild.get();
        while (child) {
            if (auto *track = dynamic_cast<TrackFrame *>(child)) {
                if (track->track && track->track->m_trackId == id) return track;
            }
            child = child->m_nextSibling.get();
        }
        return nullptr;
    }

} // namespace carbon