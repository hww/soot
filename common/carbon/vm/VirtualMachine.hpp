#pragma once

#include "carbon/ForwardDeclarations.hpp"
#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/Instructions.hpp"
#include "carbon/vm/VmError.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeAllocators.hpp"

#include "carbon/gc/GC.hpp"

#include <memory>

using namespace carbon::gc;

namespace carbon {

    // Running modes
    enum class RunMode { Run, Step, StepIn, StepOut };

    // ============================================================================
    // Forward Declarations
    // ============================================================================
    
    class VirtualMachine;
    struct StackFrame;
    class Carbon;

    // ============================================================================
    // Native Function Interface
    // ============================================================================

    using NativeFunction = Variant(*)(u32 argc, const Variant* argv);

    // ============================================================================
    // Virtual Machine Core
    // ============================================================================

    class VirtualMachine {
    public:
        Carbon* carbon;
        bool enable_debug_log;
        bool is_suspended;
        bool is_break;
        bool is_error;
        std::string break_reason;
        std::shared_ptr<StackFrame> current_frame;
        NativeAllocators            m_allocators;


        VirtualMachine(Carbon *carbon) : carbon(carbon)
        {
            enable_debug_log = true;
            is_suspended = false;
            is_break = false;
            is_error = false;
            break_reason = "";
            current_frame = nullptr; 
            
            NativeFunctionRegistry::get_instance().initialize_builtins();
            // Register allocators before anything else can call them.
            m_allocators.initialize_builtins();
        }

        ~VirtualMachine() { 
            purge();
        }

        /// @brief Clear every frame-lifetime allocator. Call once per frame,
        ///        or after each REPL expression, depending on the desired
        ///        lifetime model.
        void VirtualMachine::purge() { m_allocators.clear_all(); }

        // ------------------------------------------------------------------------
        // Properties
        // ------------------------------------------------------------------------

         std::shared_ptr<StackFrame> CurrentFrame() {return current_frame; }

        /// <summary>There is no stack frame to execute</summary>
        bool IsCompleted() { return current_frame == nullptr; }

        void ClearFlags()
        {
            is_suspended = false;
            is_break = false;
            is_error = false;
            break_reason.clear();
            current_frame = nullptr;
        }        

        // ------------------------------------------------------------------------
        // Native Function Management
        // ------------------------------------------------------------------------

        NativeFunction find_native_function(StringId name) const {
            return NativeFunctionRegistry::get_instance().find_function(name);
        }

        // ------------------------------------------------------------------------
        // Main Execution Engine
        // ------------------------------------------------------------------------

        /// @brief Execute a script lambda with the given positional arguments.
        ///
        /// @details The first argument goes into r24, the second into r25, etc.
        ///          See ARG_REGISTERS_OFFSET in your VM for the exact offset.
        Variant execute_function(ScriptLambda *script_lambda, RunMode mode,
                                 const std::vector<Variant> &args);
        Variant execute_function(ScriptLambda *script_lambda, RunMode mode = RunMode::Run);
        Variant execute(std::shared_ptr<StackFrame> stack_frame, RunMode mode = RunMode::Run);
        Variant execute(RunMode mode = RunMode::Run);

        
   
    private:
        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        i64 resolve_integer(std::shared_ptr<StackFrame> frame, StringId name);

        f64 resolve_float(std::shared_ptr<StackFrame> frame, StringId name);

        void* resolve_pointer(std::shared_ptr<StackFrame> frame, StringId name);


        // ------------------------------------------------------------------------
        // Error state
        // ------------------------------------------------------------------------
public:
        // The VM does not throw C++ exceptions for runtime errors. Instead,
        // an instruction that detects a problem calls set_error(), which
        // records the error and sets m_has_error. The main execute loop
        // checks m_has_error at the top of every iteration and, when set,
        // stops executing and returns. This is the "soft unwind" model: the
        // stack is not torn down in the middle of dispatch, and the error
        // object holds the full stacktrace until the host clears it.

        [[nodiscard]] bool           has_error() const noexcept { return m_has_error; }
        [[nodiscard]] const VmError &pending_error() const noexcept { return m_pending_error; }

        /// Reset the error state. Called by the host before each new
        /// top-level entry, and by the REPL after printing a report.
        void clear_error() {
            m_has_error = false;
            m_pending_error = VmError{};
        }

        /// Record an error. Does NOT throw. Sets m_has_error.
        void set_error(VmError::Kind kind, const std::string &message,
                       const Instruction *instr = nullptr);

        /// Record an already-built VmError.
        void set_error(VmError err);

    private:
        // Read the pointer stored in register `regnum` of the current frame
        // and return it through `out_ptr`.
        //
        // On success:
        //   * `out_ptr` is set to a non-null pointer.
        //   * returns false.
        //
        // On failure:
        //   * an error is recorded via set_error();
        //   * returns true.
        //
        // The caller is expected to do:
        //     void *ptr;
        //     if (checked_get_ptr(instr, instr.b, base, "LoadFloat", ptr)) break;
        //     ... use ptr ...
        //
        // `op_name` is used in the error message so that reports point at
        // the actual opcode, not a hardcoded one.
        [[nodiscard]] void *checked_get_ptr(Instruction &instr, u8 regnum, Variant &reg);
        
        bool    m_has_error = false;
        VmError m_pending_error;
    };

} // namespace vm