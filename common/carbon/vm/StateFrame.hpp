#pragma once

#include "carbon/file/DCScript.hpp"
#include "carbon/vm/StackFrame.hpp"

namespace carbon {

    class VirtualMachine;
    class TrackFrame; // defined in TrackFrame.hpp; forward here to avoid the include

    // ============================================================================
    // StateFrame — one state of a state-script FSM
    // ============================================================================
    //
    // A StateFrame is the root of a state's subtree. It owns every
    // TrackFrame of the state through m_firstChild / m_nextSibling.
    //
    //   * tick(vm)   — advance every TrackFrame child by one tick.
    //   * exit()     — run the state's on-exit handler, then free the
    //                  whole subtree (TrackFrames and their main frames).
    //
    // go_state replaces the StateFrame: the old one is popped (exit runs,
    // subtree is freed), a new one is pushed. No sharing of tracks
    // between states.
    //
    class StateFrame : public StackFrame {
    public:
        SsState *state = nullptr;

        StateFrame(SsState *state_desc, std::shared_ptr<StackFrame> parent);

        void tick(VirtualMachine &vm);
        void exit() override;

        /// Find a TrackFrame child by its track id.
        /// Returns nullptr if no child matches.
        TrackFrame *find_track(StringId id);

    private:
        void execute_exit();

        // Cached handlers from SsState. They are stable pointers into
        // the loaded .bin; caching avoids a lookup on every tick.
        ScriptLambda *enter_function_ = nullptr;
        ScriptLambda *trans_function_ = nullptr;
        ScriptLambda *code_function_ = nullptr;
        ScriptLambda *post_function_ = nullptr;
        ScriptLambda *exit_function_ = nullptr;
    };

} // namespace carbon