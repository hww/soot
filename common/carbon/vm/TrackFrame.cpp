#include "carbon/vm/TrackFrame.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "util/Log.hpp"

namespace carbon {

    TrackFrame::TrackFrame(SsTrack *track_desc, std::shared_ptr<StackFrame> parent)
        : StackFrame(nullptr, parent, FrameType::TRACK,
                     // m_trackId is a sid64; StringIds::none is a StringId.
                     // Wrap explicitly so the conditional is unambiguous.
                     track_desc ? StringId(track_desc->m_trackId) : StringIds::none),
          track(track_desc) {}

    void TrackFrame::tick(VirtualMachine &vm) {
        if (!track || track->m_totalLambdaCount <= 0) return;

        // ------------------------------------------------------------------
        // 1. Main lambdas — run once, at track start.
        //
        //    There may be several in a row (e.g. enter, exit). They run
        //    in order. If one yields, we stop and resume it on the next
        //    tick; the ones after it do not start until it returns.
        // ------------------------------------------------------------------
        if (!main_started) {
            for (i16 i = 0; i < track->m_totalLambdaCount; ++i) {
                const SsLambda &sl = track->m_pSsLambda[i];

                // Main lambdas are a prefix of the list. Stop at the
                // first non-main lambda.
                if (sl.m_frameNumber != FRAME_SENTINEL_MAIN) break;
                if (!sl.m_pScriptLambda) continue;

                auto main_frame = std::make_shared<StackFrame>(
                    sl.m_pScriptLambda, shared_from_this(), FrameType::GENERIC, StringIds::none);

                // The frame is a child of this track; free_children()
                // will free it if the track is destroyed mid-run.
                attach_child(main_frame);
                main_frame_ptr = main_frame.get();

                vm.execute(main_frame);

                if (vm.is_yielded) {
                    // Park. Next tick resumes from here.
                    main_started = true;
                    return;
                }

                // Returned normally — drop the frame, move to the next main.
                detach_child(main_frame);
                main_frame_ptr = nullptr;
            }
            main_started = true;
        } else if (main_frame_ptr) {
            // ------------------------------------------------------------------
            // 1b. Resume a parked main coroutine.
            // ------------------------------------------------------------------
            auto main_frame = main_frame_ptr->shared_from_this();
            vm.execute(main_frame);

            if (vm.is_yielded) return;

            detach_child(main_frame);
            main_frame_ptr = nullptr;
        }

        // ------------------------------------------------------------------
        // 2. Frame lambdas — temporary, one per matching tick.
        //
        //    Each one is pushed, executed to Return, and popped. Yield
        //    inside a frame lambda is not allowed: it would leave a
        //    temporary frame parked with nothing to resume it. We log
        //    the error and clear the flag, so the VM keeps going.
        // ------------------------------------------------------------------
        for (i16 i = 0; i < track->m_totalLambdaCount; ++i) {
            const SsLambda &sl = track->m_pSsLambda[i];

            if (sl.m_frameNumber == FRAME_SENTINEL_MAIN) continue; // handled above
            if (sl.m_frameNumber != time) continue;                // not this tick
            if (!sl.m_pScriptLambda) continue;

            auto frame = std::make_shared<StackFrame>(sl.m_pScriptLambda, shared_from_this(),
                                                      FrameType::GENERIC, StringIds::none);

            vm.execute(frame);

            if (vm.is_yielded) {
                lg::error("TrackFrame '{}': frame lambda yielded; not allowed", name.to_string());
                vm.is_yielded = false;
            }
        }

        // ------------------------------------------------------------------
        // 3. Advance time.
        // ------------------------------------------------------------------
        ++time;
    }

    void TrackFrame::exit() {
        // free_children() is called by the parent before exit().
        // The main frame (if any) has already been freed at that point;
        // drop the cached pointer so it cannot be dereferenced later.
        main_frame_ptr = nullptr;
    }

} // namespace carbon