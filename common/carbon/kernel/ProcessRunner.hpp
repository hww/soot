#pragma once

#include "carbon/ForwardDeclarations.hpp"
#include "carbon/file/DCScript.hpp"

namespace carbon {

    // ============================================================================
    // ProcessRunner Class - simplified API for creating processes
    // ============================================================================

    /// Helper class for simple creation and running of processes.
    /// Provides a high-level API similar to OpenGoal and C# systems.
    class ProcessRunner {
    public:
        // ============================================================================
        // One-shot Process Creation
        // ============================================================================

        /// Create and activate a process in one call.
        /// @param name        Process name
        /// @param parent      Parent process
        /// @param entry_point Entry point bytecode
        /// @param stack_top   Pointer to the top of the stack
        /// @return Pointer to the created process, or nullptr on error
        static Process *spawn(StringId name, Process *parent, ScriptLambda *entry_point,
                              std::shared_ptr<StackFrame> stack_top);

        /// Create a process with an initial state in one call.
        /// @param name          Process name
        /// @param parent        Parent process
        /// @param initial_state Initial state of the process
        /// @param stack_top     Pointer to the top of the stack
        /// @return Pointer to the created process, or nullptr on error
        static Process *spawn_with_state(StringId name, Process *parent, SsState *initial_state,
                                         std::shared_ptr<StackFrame> stack_top);

        // ============================================================================
        // Two-stage Process Creation (as in OpenGoal)
        // ============================================================================

        /// Create a process (stage 1).
        /// @param name Process name
        /// @return Pointer to the created process
        static Process *create(StringId name);

        /// Activate a process (stage 2).
        /// @param process   Process to activate
        /// @param parent    Parent process
        /// @param stack_top Pointer to the top of the stack
        /// @return true if activation succeeded
        static bool activate(Process *process, Process *parent, StringId name,
                             std::shared_ptr<StackFrame> stack_top);

        /// Run a function inside a process.
        /// @param process     Process to run in
        /// @param entry_point Bytecode of the function to run
        /// @return true if the run succeeded
        static bool run(Process *process, ScriptLambda *entry_point);

        // ============================================================================
        // Pool-based Creation (for optimization)
        // ============================================================================

        /// Create a process from the pool (for frequent reuse).
        /// @param name Process name
        /// @return Pointer to the process, or nullptr if the pool is empty
        static Process *create_from_pool(StringId name);

        /// Return a process to the pool.
        /// @param process Process to return
        static void return_to_pool(Process *process);

        // ============================================================================
        // Utility Functions
        // ============================================================================

        /// Create and run a process whose entry point is a module function.
        /// @param module_name   Module name
        /// @param function_name Function name
        /// @param parent        Parent process
        /// @param stack_top     Pointer to the top of the stack
        /// @return Pointer to the process, or nullptr on error
        static Process *spawn_module_function(StringId module_name, StringId function_name,
                                              Process                    *parent,
                                              std::shared_ptr<StackFrame> stack_top);

        /// Kill a process by name.
        /// @param name Name of the process to destroy
        /// @return Number of processes destroyed
        static u32 kill_by_name(StringId name);

        /// Kill all processes of a given type.
        /// @param type_name Type name of the processes
        /// @return Number of processes destroyed
        static u32 kill_by_type(StringId type_name);

    private:
        // ============================================================================
        // Private Constructor
        // ============================================================================

        /// Private constructor - this class is static-only.
        ProcessRunner() = delete;
    };

} // namespace carbon