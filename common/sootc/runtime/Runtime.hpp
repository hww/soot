// sootc/runtime/Runtime.hpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "carbon/file/BinaryFile.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/VmError.hpp"

namespace sootc {

    /// @brief Result of running a function in a runtime.
    struct RuntimeResult {
        bool            ok = false;
        std::string     error;
        carbon::Variant value;
        // Forwarded from Carbon::CallResult when the failure was a VM
        // error. Null otherwise.
        std::shared_ptr<const carbon::VmError> vm_error;
    };

    /// @brief Abstract execution backend.
    ///
    /// @details sootc compiles source into a `BinaryFile`. That binary is
    ///          handed to the current Runtime, which is responsible for
    ///          executing it. There are three concrete implementations:
    ///
    ///            LocalVmRuntime  — runs the bytecode in the in-process
    ///                              Carbon VM. Used when no external
    ///                              host is configured. The Carbon
    ///                              instance owns a Scheduler, so
    ///                              state-scripts can be spawned and
    ///                              ticked from the REPL via
    ///                              (spawn ...), (tick N), (run hz N).
    ///                              Time is discrete: one tick is one
    ///                              frame, independent of wall-clock.
    ///                              A tick that overruns its budget
    ///                              slows the world; it is never caught
    ///                              up.
    ///
    ///            RemoteRuntime   — sends bytecode to an external host
    ///                              (carbon.exe) over the existing
    ///                              cross-socket protocol. The host is
    ///                              a full runtime with processes,
    ///                              entities, and game state.
    ///
    ///            NullRuntime     — discards bytecode. Used by tests and
    ///                              by `--compile-only`.
    ///
    ///          sootc does not know which runtime is active. It calls
    ///          load_module() and call() and that's it.
    class Runtime {
    public:
        virtual ~Runtime() = default;

        /// @brief Load a compiled module into the runtime.
        /// @details For LocalVmRuntime this registers the module's
        ///          symbols in `carbon::Globals`. For RemoteRuntime
        ///          this sends the module over the wire. NullRuntime
        ///          silently accepts everything.
        /// @return true on success.
        virtual bool load_module(carbon::BinaryFile &&file) = 0;

        /// @brief Call a function by name with the given arguments.
        /// @details The function must have been loaded into the runtime
        ///          beforehand (via load_module or by the runtime being
        ///          the host itself).
        virtual RuntimeResult call(const std::string                  &name,
                                   const std::vector<carbon::Variant> &args) = 0;

        /// @brief Human-readable name of this runtime, for logs and the REPL.
        virtual std::string name() const = 0;

        /// @brief Does the runtime currently have a symbol with this name?
        /// @details Used by the REPL to decide whether to call `top-level`
        ///          after a successful module load. Returns false in
        ///          backends that cannot answer the question.
        virtual bool has_symbol(const std::string & /*name*/) const { return false; }
    };

} // namespace sootc