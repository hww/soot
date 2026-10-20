#pragma once

#include "carbon/ForwardDeclarations.hpp"
#include "carbon/lib/Variant.hpp"
#include <map>


namespace carbon {

    // ========================================================================
    // Process-control native functions
    // ========================================================================

    /// Report a script-level error from a native function.
    /// Logs the message and returns a false Variant, matching the
    /// Naughty Dog ScriptError() convention.
    Variant ScriptError(const char *fmt, ...);

    /// Native entry points registered by NativeProcess.
    Variant native_spawn(u32 argc, const Variant *argv);
    Variant native_kill(u32 argc, const Variant *argv);
    Variant native_sleep(u32 argc, const Variant *argv);
    Variant native_yield(u32 argc, const Variant *argv);
    Variant native_go_state(u32 argc, const Variant *argv);
    Variant native_send(u32 argc, const Variant *argv);
    Variant native_current_pid(u32 argc, const Variant *argv);

    /// Register the process-control natives in the global registry.
    void initialize_process_natives();
} // namespace carbon