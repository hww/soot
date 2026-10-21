#pragma once

#include "carbon/ForwardDeclarations.hpp"
#include "carbon/lib/Variant.hpp"

namespace carbon {

    // ========================================================================
    // Process-control native functions
    // ========================================================================
    //
    // Two groups of natives:
    //
    //   Lifecycle / scheduling
    //     spawn, start, current-pid, tick, run, process,
    //     sleep, yield
    //
    //     These do not take a process-name argument. `spawn` and `start`
    //     create a process from a state-script name; `tick` and `run`
    //     drive the scheduler from the host side; `sleep` and `yield`
    //     act on the current process by definition (there is no such
    //     thing as "sleeping on behalf of another process").
    //
    //   Process-facing
    //     kill, go-state, send,
    //     get-state-time, set-state-time,
    //     get-track-time, set-track-time,
    //     get-track-pc,   set-track-pc
    //
    //     Every one of these takes its process argument as a name in
    //     argument 0. The name is resolved in exactly one place,
    //     resolve_process_arg() in NativeProcess.cpp:
    //
    //         "self"   ->  Carbon::current_process()
    //         other    ->  Scheduler::find_by_name(other)
    //
    //     The argument may arrive as a string ("self", "my-gate") or as
    //     a StringId (SID("self"), SID("my-gate")) depending on what the
    //     compiler emitted; both forms are accepted. The natives do not
    //     care.
    //
    // There is no process-time. A process only has a state time: the
    // number of ticks since entering the current state. If a script
    // needs a "total time", it keeps its own variable.
    //

    /// Report a script-level error from a native function.
    /// Logs the message and returns a false Variant, matching the
    /// Naughty Dog ScriptError() convention.
    Variant ScriptError(const char *fmt, ...);

    // ---- Lifecycle / scheduling ----

    Variant native_spawn(u32 argc, const Variant *argv);
    Variant native_start(u32 argc, const Variant *argv);
    Variant native_current_pid(u32 argc, const Variant *argv);
    Variant native_tick(u32 argc, const Variant *argv);
    Variant native_run(u32 argc, const Variant *argv);
    Variant native_process(u32 argc, const Variant *argv);
    Variant native_sleep(u32 argc, const Variant *argv);
    Variant native_yield(u32 argc, const Variant *argv);

    // ---- Process-facing (arg 0 is "self" or a process name) ----

    Variant native_kill(u32 argc, const Variant *argv);
    Variant native_go_state(u32 argc, const Variant *argv);
    Variant native_send(u32 argc, const Variant *argv);

    Variant native_get_state_time(u32 argc, const Variant *argv);
    Variant native_set_state_time(u32 argc, const Variant *argv);

    Variant native_get_track_time(u32 argc, const Variant *argv);
    Variant native_set_track_time(u32 argc, const Variant *argv);
    Variant native_get_track_pc(u32 argc, const Variant *argv);
    Variant native_set_track_pc(u32 argc, const Variant *argv);

    /// Register the process-control natives in the global registry.
    void initialize_process_natives();

} // namespace carbon