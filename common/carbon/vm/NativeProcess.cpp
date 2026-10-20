#include "carbon/vm/NativeFunc.hpp"

#include "carbon/Carbon.hpp"
#include "carbon/kernel/Process.hpp"
#include "carbon/kernel/Scheduler.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/NativeProcess.hpp"

#include "util/Log.hpp"

#include <cstdarg>
#include <cstdio>

namespace carbon {

    // ========================================================================
    // ScriptError
    // ========================================================================
    //
    // The Naughty Dog convention: a native detects a bad argument or an
    // impossible state, logs a message, and returns Variant(false). The
    // script sees a false return, the VM keeps running, and the process
    // continues (unless it explicitly yields).
    //
    // Use this for recoverable script-level errors. For VM-level errors
    // (null deref, type mismatch), use the VM's set_error path instead.
    Variant ScriptError(const char *fmt, ...) {
        char    buf[512];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        lg::error("[ScriptError] {}", buf);
        return Variant(false);
    }

    // ========================================================================
    // spawn
    // ========================================================================
    //
    // (spawn "state-script-name" [args...]) -> pid : int
    //
    // Looks up the state script by name, spawns a process, returns its
    // pid. Fails with ScriptError if the name is missing or unknown.
    Variant native_spawn(u32 argc, const Variant *argv) {
        if (argc < 1) { return ScriptError("spawn: expected state-script name"); }
        if (!argv[0].is_string() && !argv[0].is_ptr()) {
            return ScriptError("spawn: argument 0 must be a string");
        }

        const char *name = nullptr;
        if (argv[0].is_string()) {
            name = argv[0].to_c_string();
        } else {
            name = static_cast<const char *>(argv[0].get_ptr());
        }
        if (!name || *name == '\0') { return ScriptError("spawn: empty state-script name"); }

        std::vector<Variant> args;
        args.reserve(argc - 1);
        for (u32 i = 1; i < argc; ++i) args.push_back(argv[i]);

        Scheduler *sched = Carbon::current()->scheduler();
        const u64  pid = sched->spawn(name, args);
        if (pid == 0) { return ScriptError("spawn: '%s' not found or not a state script", name); }
        return Variant(static_cast<i64>(pid));
    }

    // ========================================================================
    // kill
    // ========================================================================
    //
    // (kill pid) -> bool
    Variant native_kill(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_int()) { return ScriptError("kill: expected process id"); }
        const u64 pid = static_cast<u64>(argv[0].get_i64());
        if (pid == 0) return Variant(false);

        Carbon::current()->scheduler()->kill(pid);
        return Variant(true);
    }

    // ========================================================================
    // sleep
    // ========================================================================
    //
    // (sleep frames) -> true
    //
    // Parks the current process for N ticks. The scheduler moves it back
    // to the ready queue when the counter hits zero.
    //
    // This is the cooperative-suspend entry point: the native returns
    // normally, and the VM checks should_yield after CallFf.
    Variant native_sleep(u32 argc, const Variant *argv) {
        Process *self = Carbon::current_process();
        if (!self) { return ScriptError("sleep: no current process"); }

        i64 frames = 1;
        if (argc >= 1 && argv[0].is_int()) { frames = argv[0].get_i64(); }
        if (frames < 1) frames = 1;

        self->sleep_frames = frames;
        self->Suspend();
        return Variant(true);
    }

    // ========================================================================
    // yield
    // ========================================================================
    //
    // (yield) -> true
    //
    // Parks the process for one tick without changing its state. The
    // scheduler re-queues it on the next tick.
    Variant native_yield(u32 /*argc*/, const Variant * /*argv*/) {
        Process *self = Carbon::current_process();
        if (self) self->RequestYield();
        return Variant(true);
    }

    // ========================================================================
    // go-state
    // ========================================================================
    //
    // (go-state "state-name") -> bool
    //
    // Schedules a state transition. If the process is running, the
    // transition is deferred to the next quantum.
    Variant native_go_state(u32 argc, const Variant *argv) {
        if (argc < 1 || !argv[0].is_int()) { return ScriptError("go-state: expected state id"); }
        Process *self = Carbon::current_process();
        if (!self) { return ScriptError("go-state: no current process"); }
        const StringId state_id(argv[0].get_i64());
        return Variant(self->go_state(state_id));
    }

    // ========================================================================
    // send
    // ========================================================================
    //
    // (send target-pid event-id [arg0 ... arg6]) -> bool
    //
    // Builds an EventMessage and dispatches it to the target process's
    // current state. Up to MAX_PARAMS arguments.
    Variant native_send(u32 argc, const Variant *argv) {
        if (argc < 2) { return ScriptError("send: expected (target-pid, event-id, ...)"); }
        if (!argv[0].is_int() || !argv[1].is_int()) {
            return ScriptError("send: pid and event-id must be integers");
        }

        Process *self = Carbon::current_process();
        if (!self) { return ScriptError("send: no current process"); }

        const u64      target_pid = static_cast<u64>(argv[0].get_i64());
        const StringId event_id(argv[1].get_i64());

        Process *target = Carbon::current()->scheduler()->find(target_pid);
        if (!target) {
            return ScriptError("send: pid %llu not found",
                               static_cast<unsigned long long>(target_pid));
        }

        const u32 n = (argc > 2) ? (argc - 2) : 0;
        if (n > EventMessage::MAX_PARAMS) {
            return ScriptError("send: too many arguments (%u, max %d)", n,
                               EventMessage::MAX_PARAMS);
        }

        // Copy the trailing arguments into a stack buffer; send_event
        // copies them into an EventMessage before dispatching.
        Variant params[EventMessage::MAX_PARAMS];
        for (u32 i = 0; i < n; ++i) params[i] = argv[2 + i];

        return Variant(self->send_event(target, n, event_id, params));
    }

    // ========================================================================
    // current-pid
    // ========================================================================
    //
    // () -> pid : int
    Variant native_current_pid(u32 /*argc*/, const Variant * /*argv*/) {
        Process *self = Carbon::current_process();
        if (!self) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(self->pid));
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void initialize_process_natives() {
        auto &reg = NativeFunctionRegistry::get_instance();
        reg.register_function("spawn", &native_spawn);
        reg.register_function("kill", &native_kill);
        reg.register_function("sleep", &native_sleep);
        reg.register_function("yield", &native_yield);
        reg.register_function("go-state", &native_go_state);
        reg.register_function("send", &native_send);
        reg.register_function("current-pid", &native_current_pid);
        lg::info("NativeProcess: registered 7 process functions");
    }

} // namespace carbon