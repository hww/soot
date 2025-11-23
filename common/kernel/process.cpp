#include "process.hpp"
#include "virtual_machine.hpp"
#include "state_machine.hpp"
#include "binary_file.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <algorithm>

namespace vm {

    // Static context for native functions
    static thread_local Process* current_process = nullptr;

    Process::Process(u32 pid, const std::string& name)
        : pid_(pid), name_(name), state_(ProcessState::CREATED) {
        lg::debug("Process created: {} (PID: {})", name_, pid_);
    }

    Process::~Process() {
        lg::debug("Process destroyed: {}", name_);
    }

    // ------------------------------------------------------------------------
    // Execution Control
    // ------------------------------------------------------------------------

    bool Process::execute_quantum() {
        if (state_ != ProcessState::READY && state_ != ProcessState::RUNNING) {
            return false;
        }

        // Set thread-local context for native functions
        Process* previous_process = current_process;
        current_process = this;
        state_ = ProcessState::RUNNING;

        bool continue_execution = true;
        quantum_remaining_ = DEFAULT_QUANTUM;

        // Execute until quantum exhausted or process blocks
        while (quantum_remaining_ > 0 && continue_execution) {
            if (call_stack_.empty()) {
                // No more frames to execute
                terminate();
                continue_execution = false;
                break;
            }

            // Execute current frame
            continue_execution = execute_current_frame();
            quantum_remaining_--;
            total_instructions_executed_++;

            // Check if process wants to suspend
            if (state_ == ProcessState::SUSPENDED) {
                break;
            }
        }

        // Restore previous context
        current_process = previous_process;

        if (state_ == ProcessState::RUNNING) {
            state_ = ProcessState::READY;
        }

        return continue_execution;
    }

    bool Process::execute_current_frame() {
        StackFrame* frame = get_current_frame();
        if (!frame) return false;

        try {
            Instruction instr = frame->get_next_instruction();

            // Handle system calls (native function calls)
            if (instr.opcode == Opcode::CALL_NATIVE) {
                handle_system_call(instr);
                return true;
            }

            // Delegate to VM for normal instruction execution
            VirtualMachine* vm = VirtualMachine::get_current();
            if (vm) {
                vm->execute_instruction(*frame, instr);
            }

            return true;

        }
        catch (const std::exception& e) {
            lg::error("Process {} execution error: {}", name_, e.what());
            terminate();
            return false;
        }
    }

    void Process::handle_system_call(const Instruction& instr) {
        Variant& func_var = get_current_frame()->get_register(instr.a);
        u32 argc = instr.c;

        // Get function name for lookup
        StringId func_name = SID_NONE;
        if (func_var.is_sid()) {
            func_name = func_var.get_sid();
        }
        else if (func_var.is_string()) {
            func_name = string_id::register_string(func_var.get_string());
        }

        // Execute system call
        Variant result;

        switch (func_name) {
        case SID("sleep"):
            if (argc >= 1) {
                u32 frames = get_current_frame()->get_argument(0).to_int();
                // Simplified sleep - just suspend for now
                suspend();
            }
            break;

        case SID("kill"):
            terminate();
            break;

        case SID("go"):
            if (argc >= 1) {
                StringId state_name = get_current_frame()->get_argument(0).get_sid();
                if (state_machine_) {
                    state_machine_->transition_to(string_id::to_string(state_name));
                }
            }
            break;

        case SID("send-event"):
            if (argc >= 1) {
                StringId event_name = get_current_frame()->get_argument(0).get_sid();
                // Event system would be implemented separately
                lg::debug("Process {} sending event: {}", name_, string_id::to_string(event_name));
            }
            break;

        default:
            lg::warn("Unknown system call: {}", string_id::to_string(func_name));
            break;
        }

        // Store result if needed
        if (instr.b < MAX_REGISTERS) {
            get_current_frame()->get_register(instr.b) = result;
        }
    }

    void Process::suspend() {
        state_ = ProcessState::SUSPENDED;
        lg::debug("Process {} suspended", name_);
    }

    void Process::resume() {
        if (state_ == ProcessState::SUSPENDED || state_ == ProcessState::WAITING) {
            state_ = ProcessState::READY;
            lg::debug("Process {} resumed", name_);
        }
    }

    void Process::terminate() {
        state_ = ProcessState::TERMINATED;
        call_stack_.clear();
        lg::debug("Process {} terminated", name_);
    }

    // ------------------------------------------------------------------------
    // Stack Frame Management
    // ------------------------------------------------------------------------

    void Process::push_method_frame(ByteCode* method_code, void* this_ptr, u32 argc) {
        ASSERT(method_code != nullptr);

        auto frame = std::make_unique<StackFrame>(
            method_code->get_code_ptr(),
            method_code->get_data_ptr(),
            get_current_frame()
        );

        // Set this pointer as first argument (our basis agreement)
        if (this_ptr) {
            frame->set_argument(0, Variant(this_ptr, SID("object")));
        }

        // Copy other arguments from current frame
        for (u32 i = 0; i < argc && i < MAX_ARGS; i++) {
            frame->set_argument(i + 1, get_current_frame()->get_argument(i));
        }

        call_stack_.push_back(std::move(frame));
        lg::debug("Process {} pushed method frame", name_);
    }

    void Process::push_main_frame(ByteCode* main_code) {
        ASSERT(main_code != nullptr);

        auto frame = std::make_unique<StackFrame>(
            main_code->get_code_ptr(),
            main_code->get_data_ptr(),
            nullptr
        );

        call_stack_.push_back(std::move(frame));
        state_ = ProcessState::READY;
        lg::debug("Process {} pushed main frame", name_);
    }

    // ------------------------------------------------------------------------
    // Native Function Context
    // ------------------------------------------------------------------------

    Process* Process::get_current_process() {
        return current_process;
    }

    void Process::set_current_process(Process* process) {
        current_process = process;
    }

    // ------------------------------------------------------------------------
    // Lookup Support
    // ------------------------------------------------------------------------

    Variant* Process::lookup_field(StringId name) {
        // Special case: "self" lookup
        if (name == SID("self")) {
            // This would return a Variant wrapping main_self_ptr_
            // But we need to consider how to handle this properly
            lg::debug("Process {} self lookup", name_);
            return nullptr; // Temporary
        }

        // Normal field lookup would go through the object system
        if (main_self_ptr_) {
            // object_system::lookup_field(main_self_ptr_, name);
            lg::debug("Process {} field lookup: {}", name_, string_id::to_string(name));
        }

        return nullptr;
    }

    // ------------------------------------------------------------------------
    // State Machine Integration
    // ------------------------------------------------------------------------

    void Process::set_state_machine(std::unique_ptr<StateMachine> fsm) {
        state_machine_ = std::move(fsm);
        if (state_machine_) {
            state_machine_->set_owner(this);
        }
    }

    // ------------------------------------------------------------------------
    // Debugging
    // ------------------------------------------------------------------------

    std::string Process::to_string() const {
        return fmt::format("Process({}, PID:{}, State:{}, Frames:{})",
            name_, pid_, static_cast<int>(state_), call_stack_.size());
    }

    void Process::dump_stack() const {
        lg::debug("=== Process {} Stack Dump ===", name_);
        for (size_t i = 0; i < call_stack_.size(); ++i) {
            lg::debug("  [{}] {}", i, call_stack_[i]->to_string());
        }
    }

} // namespace vm