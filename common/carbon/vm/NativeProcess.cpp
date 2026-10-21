#include "carbon/vm/NativeProcess.hpp"
#include "carbon/vm/NativeFunc.hpp"

#include "carbon/kernel/EventMessage.hpp"
#include "carbon/kernel/Process.hpp"
#include "carbon/kernel/Scheduler.hpp"

#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"

#include "carbon/Carbon.hpp"

#include "util/Log.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

    // ========================================================================
    // Argument helpers
    // ========================================================================

    // A track-id, state-id, or event-id arrives from the compiler as one of:
    //
    //   Variant::Int     -- StringId (SID("bus"))
    //   Variant::String  -- GC-allocated string ("bus")
    //   Variant::Pointer -- const char* into the string table ("bus")
    //
    // All three mean the same thing: a name. Convert to StringId once, here.
    StringId sid_arg(const Variant &v) {
        if (v.is_int()) return v.get_sid();
        if (v.is_string()) return StringId(v.to_string());
        if (v.is_ptr()) {
            const char *s = static_cast<const char *>(v.get_ptr());
            return s ? StringId(s) : StringId();
        }
        return StringId();
    }

    // Every process-facing native takes its process argument as a name.
    // The name is resolved here, in one place:
    //
    //   "self"  ->  Carbon::current_process()
    //   other   ->  Scheduler::find_by_name(name)
    //
    // The argument may arrive as a string, a const char*, or a StringId;
    // all three are accepted.
    Process *resolve_process_arg(const Variant &v) {
        Carbon *c = Carbon::current();
        if (!c || !c->is_initialized()) return nullptr;

        if (v.is_string()) {
            const char *s = v.to_c_string();
            if (!s || *s == '\0') return nullptr;
            if (std::strcmp(s, "self") == 0) return Carbon::current_process();
            return c->scheduler()->find_by_name(s);
        }

        if (v.is_ptr()) {
            const char *s = static_cast<const char *>(v.get_ptr());
            if (!s || *s == '\0') return nullptr;
            if (std::strcmp(s, "self") == 0) return Carbon::current_process();
            return c->scheduler()->find_by_name(s);
        }

        if (v.is_int()) {
            const StringId sid = v.get_sid();
            if (sid == SID("self")) return Carbon::current_process();
            return c->scheduler()->find_by_name(sid.to_string());
        }

        return nullptr;
    }

    // Name of an argument for error messages. Never returns nullptr.
    const char *process_arg_name(const Variant &v) {
        if (v.is_string()) {
            const char *s = v.to_c_string();
            return s ? s : "(null)";
        }
        if (v.is_ptr()) {
            const char *s = static_cast<const char *>(v.get_ptr());
            return s ? s : "(null)";
        }
        if (v.is_int()) {
            thread_local std::string buf;
            buf = v.get_sid().to_string();
            return buf.c_str();
        }
        return "(not a process name)";
    }

    // State-script name argument for spawn/start.
    const char *script_name_arg(const Variant &v) {
        if (v.is_string()) {
            const char *s = v.to_c_string();
            if (!s || *s == '\0') return nullptr;
            return s;
        }
        if (v.is_ptr()) {
            const char *s = static_cast<const char *>(v.get_ptr());
            if (!s || *s == '\0') return nullptr;
            return s;
        }
        if (v.is_int()) {
            thread_local std::string buf;
            buf = v.get_sid().to_string();
            return buf.c_str();
        }
        return nullptr;
    }

} // namespace

namespace carbon {

    // ========================================================================
    // ScriptError
    // ========================================================================
    //
    // Naughty Dog convention: log, return false. The VM keeps running,
    // the process keeps going (unless it explicitly yields).
    //
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
    // spawn / start
    // ========================================================================
    //
    // (spawn "state-script-name" [args...]) -> pid : int
    // (start "state-script-name" [args...]) -> pid : int
    //
    // start is idempotent: if a process with this name is alive, its pid
    // is returned instead of creating a second one.
    //
    Variant native_spawn(u32 argc, const Variant *argv) {
        if (argc < 1) { return ScriptError("spawn: expected state-script name"); }

        const char *name = script_name_arg(argv[0]);
        if (!name) { return ScriptError("spawn: argument 0 must be a string"); }

        std::vector<Variant> args;
        args.reserve(argc - 1);
        for (u32 i = 1; i < argc; ++i) args.push_back(argv[i]);

        Scheduler *sched = Carbon::current()->scheduler();
        const u64  pid = sched->spawn(name, args);
        if (pid == 0) { return ScriptError("spawn: '%s' not found or not a state script", name); }
        return Variant(static_cast<i64>(pid));
    }

    Variant native_start(u32 argc, const Variant *argv) {
        if (argc < 1) { return ScriptError("start: expected state-script name"); }

        const char *name = script_name_arg(argv[0]);
        if (!name) { return ScriptError("start: argument 0 must be a string"); }

        Scheduler *sched = Carbon::current()->scheduler();

        if (Process *existing = sched->find_by_name(name)) {
            return Variant(static_cast<i64>(existing->pid));
        }

        std::vector<Variant> args;
        args.reserve(argc - 1);
        for (u32 i = 1; i < argc; ++i) args.push_back(argv[i]);

        const u64 pid = sched->spawn(name, args);
        if (pid == 0) { return ScriptError("start: '%s' not found or not a state script", name); }
        return Variant(static_cast<i64>(pid));
    }

    // ========================================================================
    // current-pid
    // ========================================================================
    //
    // () -> pid : int
    //
    Variant native_current_pid(u32 /*argc*/, const Variant * /*argv*/) {
        Process *self = Carbon::current_process();
        if (!self) return Variant(static_cast<i64>(0));
        return Variant(static_cast<i64>(self->pid));
    }

    // ========================================================================
    // sleep / yield
    // ========================================================================
    //
    // Act on the current process by definition. There is no meaningful
    // "sleep on behalf of another process".
    //
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

    Variant native_yield(u32 /*argc*/, const Variant * /*argv*/) {
        Process *self = Carbon::current_process();
        if (self) self->RequestYield();
        return Variant(true);
    }

    // ========================================================================
    // kill
    // ========================================================================
    //
    // (kill "self")      -> bool
    // (kill "my-gate")   -> bool
    //
    Variant native_kill(u32 argc, const Variant *argv) {
        if (argc < 1) { return ScriptError("kill: expected (process)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) { return ScriptError("kill: process '%s' not found", process_arg_name(argv[0])); }

        Carbon::current()->scheduler()->kill(p->pid);
        return Variant(true);
    }

    // ========================================================================
    // go-state
    // ========================================================================
    //
    // (go-state "self"    "opening") -> bool
    // (go-state "my-gate" "opening") -> bool
    //
    Variant native_go_state(u32 argc, const Variant *argv) {
        if (argc < 2) { return ScriptError("go-state: expected (process, state-id)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("go-state: process '%s' not found", process_arg_name(argv[0]));
        }

        return Variant(p->go_state(sid_arg(argv[1])));
    }

    // ========================================================================
    // send
    // ========================================================================
    //
    // (send "self"   "target" "interact" [arg0 ... arg6]) -> bool
    // (send "source" "target" "interact" [arg0 ... arg6]) -> bool
    //
    Variant native_send(u32 argc, const Variant *argv) {
        if (argc < 3) { return ScriptError("send: expected (source, target, event, ...)"); }

        Process *from = resolve_process_arg(argv[0]);
        Process *to = resolve_process_arg(argv[1]);
        if (!from) { return ScriptError("send: source '%s' not found", process_arg_name(argv[0])); }
        if (!to) { return ScriptError("send: target '%s' not found", process_arg_name(argv[1])); }

        const StringId event = sid_arg(argv[2]);

        const u32 n = (argc > 3) ? (argc - 3) : 0;
        if (n > EventMessage::MAX_PARAMS) {
            return ScriptError("send: too many arguments (%u, max %d)", n,
                               EventMessage::MAX_PARAMS);
        }

        Variant params[EventMessage::MAX_PARAMS];
        for (u32 i = 0; i < n; ++i) params[i] = argv[3 + i];

        return Variant(from->send_event(to, n, event, params));
    }

    // ========================================================================
    // process
    // ========================================================================
    //
    // (process "self")    -> pointer : Process*
    // (process "my-gate") -> pointer : Process*
    //
    Variant native_process(u32 argc, const Variant *argv) {
        if (argc < 1) { return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer); }
        Process *p = resolve_process_arg(argv[0]);
        return Variant(static_cast<void *>(p), RuntimeType::Pointer);
    }

    // ========================================================================
    // State time
    // ========================================================================
    //
    // A process has exactly one clock: state_time, the number of ticks
    // since entering the current state. There is no process-wide time.
    //
    // (get-state-time "self")    -> int
    // (set-state-time "self"    N) -> bool
    //
    Variant native_get_state_time(u32 argc, const Variant *argv) {
        if (argc < 1) { return ScriptError("get-state-time: expected (process)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("get-state-time: process '%s' not found", process_arg_name(argv[0]));
        }
        return Variant(static_cast<i64>(p->get_state_time()));
    }

    Variant native_set_state_time(u32 argc, const Variant *argv) {
        if (argc < 2) { return ScriptError("set-state-time: expected (process, ticks)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("set-state-time: process '%s' not found", process_arg_name(argv[0]));
        }
        p->set_state_time(static_cast<u64>(argv[1].to_int()));
        return Variant(true);
    }

    // ========================================================================
    // Track accessors
    // ========================================================================
    //
    // (get-track-time "self" "bus")      -> int
    // (set-track-time "self" "bus" N)    -> bool
    // (get-track-pc   "self" "bus")      -> int
    // (set-track-pc   "self" "bus" N)    -> bool
    //
    Variant native_get_track_time(u32 argc, const Variant *argv) {
        if (argc < 2) { return ScriptError("get-track-time: expected (process, track-id)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("get-track-time: process '%s' not found", process_arg_name(argv[0]));
        }
        TrackInstance *t = p->find_track(sid_arg(argv[1]));
        return Variant(static_cast<i64>(t ? t->get_time() : 0));
    }

    Variant native_set_track_time(u32 argc, const Variant *argv) {
        if (argc < 3) { return ScriptError("set-track-time: expected (process, track-id, ticks)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("set-track-time: process '%s' not found", process_arg_name(argv[0]));
        }
        TrackInstance *t = p->find_track(sid_arg(argv[1]));
        if (!t) { return ScriptError("set-track-time: track not found"); }

        t->set_time(static_cast<u64>(argv[2].to_int()));
        return Variant(true);
    }

    Variant native_get_track_pc(u32 argc, const Variant *argv) {
        if (argc < 2) { return ScriptError("get-track-pc: expected (process, track-id)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("get-track-pc: process '%s' not found", process_arg_name(argv[0]));
        }
        TrackInstance *t = p->find_track(sid_arg(argv[1]));
        return Variant(static_cast<i64>(t ? t->get_pc() : 0));
    }

    Variant native_set_track_pc(u32 argc, const Variant *argv) {
        if (argc < 3) { return ScriptError("set-track-pc: expected (process, track-id, pc)"); }

        Process *p = resolve_process_arg(argv[0]);
        if (!p) {
            return ScriptError("set-track-pc: process '%s' not found", process_arg_name(argv[0]));
        }
        TrackInstance *t = p->find_track(sid_arg(argv[1]));
        if (!t) { return ScriptError("set-track-pc: track not found"); }

        t->set_pc(static_cast<u32>(argv[2].to_int()));
        return Variant(true);
    }

    // ========================================================================
    // Host-side scheduler driving
    // ========================================================================
    //
    // (tick [n])              -> int
    // (run  [hz] [max-ticks]) -> int
    //
    // Refuse to run from inside a process: re-entering the scheduler
    // would corrupt it.
    //
    Variant native_tick(u32 argc, const Variant *argv) {
        if (Carbon::current_process() != nullptr) {
            return ScriptError("tick: cannot tick from inside a process");
        }
        u32 n = 1;
        if (argc >= 1 && argv[0].is_int()) { n = static_cast<u32>(argv[0].get_i64()); }
        for (u32 i = 0; i < n; ++i) { Carbon::current()->tick(); }
        return Variant(static_cast<i64>(n));
    }

    Variant native_run(u32 argc, const Variant *argv) {
        if (Carbon::current_process() != nullptr) {
            return ScriptError("run: cannot run from inside a process");
        }
        TickConfig cfg;
        if (argc >= 1 && argv[0].is_int()) { cfg.hz = static_cast<u32>(argv[0].get_i64()); }
        if (argc >= 2 && argv[1].is_int()) { cfg.max_ticks = argv[1].get_i64(); }
        cfg.realtime = true;
        cfg.until_empty = true;
        const u64 n = Carbon::current()->run_loop(cfg);
        return Variant(static_cast<i64>(n));
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void initialize_process_natives() {
        auto &reg = NativeFunctionRegistry::get_instance();

        // Lifecycle / scheduling
        reg.register_function("spawn", &native_spawn);
        reg.register_function("start", &native_start);
        reg.register_function("current-pid", &native_current_pid);
        reg.register_function("tick", &native_tick);
        reg.register_function("run", &native_run);
        reg.register_function("process", &native_process);
        reg.register_function("sleep", &native_sleep);
        reg.register_function("yield", &native_yield);

        // Process-facing (arg 0 is "self" or a process name)
        reg.register_function("kill", &native_kill);
        reg.register_function("go-state", &native_go_state);
        reg.register_function("send", &native_send);
        reg.register_function("get-state-time", &native_get_state_time);
        reg.register_function("set-state-time", &native_set_state_time);
        reg.register_function("get-track-time", &native_get_track_time);
        reg.register_function("set-track-time", &native_set_track_time);
        reg.register_function("get-track-pc", &native_get_track_pc);
        reg.register_function("set-track-pc", &native_set_track_pc);

        lg::info("NativeProcess: registered 8 lifecycle + 9 process-facing functions");
    }

} // namespace carbon