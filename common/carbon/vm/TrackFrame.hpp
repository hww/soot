#pragma once

#include "carbon/file/DCScript.hpp"
#include "carbon/vm/StackFrame.hpp"

namespace carbon {

    class VirtualMachine;

    // ============================================================================
    // TrackFrame — one running track (animation / logic lane)
    // ============================================================================
    //
    // A track is a long-lived child of StateFrame. It owns:
    //
    //   * main_frame : the main coroutine, a child of this TrackFrame.
    //                  It runs once at track start, may yield, and resumes
    //                  on the next tick. If it returns, the frame is
    //                  destroyed and never runs again.
    //
    //   * frame lambdas : temporary. Each one is pushed, executed to
    //                     Return, and popped inside tick(). No yield
    //                     is allowed; a yield is logged as an error and
    //                     ignored.
    //
    // The track itself does not use pc. It advances `time` once per tick
    // and fires every frame lambda whose frameNumber equals `time`. A
    // script that wants to loop calls set-track-time(0); the next tick
    // starts from time == 0 and the main lambda is NOT re-run (it is
    // already running or done).
    //
    class TrackFrame : public StackFrame {
    public:
        SsTrack *track = nullptr; // data from the .bin (lambdas, ids)
        u64      time = 0;        // ticks since the track started
        bool     main_started = false;

        // main_frame is a child of this frame (in m_firstChild).
        // Stored here for fast access; ownership is the child list.
        StackFrame *main_frame_ptr = nullptr;

        TrackFrame(SsTrack *track_desc, std::shared_ptr<StackFrame> parent);

        [[nodiscard]] u64 get_time() const noexcept { return time; }
        void              set_time(u64 n) noexcept { time = n; }

        /// Advance the track by one tick: run/resume main, then run
        /// every frame lambda whose frameNumber == time, then ++time.
        void tick(VirtualMachine &vm);

        /// Called by the parent before the child list is freed.
        /// Drops the cached main pointer so it cannot dangle.
        void exit() override;
    };

} // namespace carbon