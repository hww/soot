#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "stack_frame.hpp"
#include <memory>
#include <vector>
#include <string>

namespace vm {

    // Forward declarations
    class StateMachine;
    struct ByteCode;

    /**
     * @brief Process states matching GOAL architecture
     */
    enum class ProcessState {
        CREATED,      // Process created but not activated
        READY,        // Ready to be scheduled  
        RUNNING,      // Currently executing
        SUSPENDED,    // Suspended (waiting for next frame)
        WAITING,      // Waiting for external event
        TERMINATED    // Process finished
    };

    /**
     * @brief GOAL-like process - main executable unit
     *
     * Follows our basis:
     * - Has main_self_ptr_ for primary object
     * - No FSM instructions in VM
     * - State machine as separate component
     * - Context for native function access
     */
    class Process {
    public:
        // ------------------------------------------------------------------------
        // Construction & Identification
        // ------------------------------------------------------------------------

        Process(u32 pid, const std::string& name);
        ~Process();

        u32 get_pid() const { return pid_; }
        const std::string& get_name() const { return name_; }

        // ------------------------------------------------------------------------
        // Self Pointer Management (CRITICAL - according to our basis)
        // ------------------------------------------------------------------------

        void* get_self() const { return main_self_ptr_; }
        void set_self(void* self_ptr) { main_self_ptr_ = self_ptr; }

        // ------------------------------------------------------------------------
        // Execution Control
        // ------------------------------------------------------------------------

        bool execute_quantum();  // Execute one time quantum
        void suspend();
        void resume();
        void terminate();

        ProcessState get_state() const { return state_; }

        // ------------------------------------------------------------------------
        // Stack Frame Management
        // ------------------------------------------------------------------------

        // For method calls - with this pointer as first argument
        void push_method_frame(ByteCode* method_code, void* this_ptr, u32 argc = 0);

        // For main process code
        void push_main_frame(ByteCode* main_code);

        StackFrame* get_current_frame() {
            return call_stack_.empty() ? nullptr : call_stack_.back().get();
        }

        // ------------------------------------------------------------------------
        // State Machine Integration (FSM as separate component)
        // ------------------------------------------------------------------------

        void set_state_machine(std::unique_ptr<StateMachine> fsm);
        StateMachine* get_state_machine() { return state_machine_.get(); }

        // ------------------------------------------------------------------------
        // Native Function Context
        // ------------------------------------------------------------------------

        // For native functions to access process context
        static Process* get_current_process();
        static void set_current_process(Process* process);

        // ------------------------------------------------------------------------
        // Lookup Support (for LOOKUP_* instructions)
        // ------------------------------------------------------------------------

        Variant* lookup_field(StringId name);

        // ------------------------------------------------------------------------
        // Debugging
        // ------------------------------------------------------------------------

        std::string to_string() const;
        void dump_stack() const;

    private:
        u32 pid_;
        std::string name_;
        ProcessState state_ = ProcessState::CREATED;

        // MAIN SELF POINTER - according to our basis
        void* main_self_ptr_ = nullptr;

        // Call stack - each frame has its context
        std::vector<std::unique_ptr<StackFrame>> call_stack_;

        // Optional state machine
        std::unique_ptr<StateMachine> state_machine_;

        // Execution statistics
        u32 total_instructions_executed_ = 0;
        u32 quantum_remaining_ = 0;
        static constexpr u32 DEFAULT_QUANTUM = 1000;

        // Internal execution
        bool execute_current_frame();
        void handle_system_call(const Instruction& instr);
    };

} // namespace vm