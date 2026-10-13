#pragma once

#include "carbon/ForwardDeclarations.hpp"
#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/Instructions.hpp"
#include "carbon/vm/StackFrame.hpp"
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
    // Errors
    // ============================================================================

    /* Erros */
    class VmError : public std::exception {
    public:
        explicit VmError(const std::string& msg, std::shared_ptr<StackFrame> frame = nullptr) 
            : message_(msg), frame_(frame) {
            if (frame_) {
                auto instruction = frame_->get_this_instruction();
                message_ = fmt::format("{} [Frame: name={}, pc={}, argc={}, inst={}]",
                    message_,
                    frame_->name,
                    frame_->pc,
                    frame_->argc,
                    instruction.to_string());
            }
        }
        
        const char* what() const noexcept override { return message_.c_str(); }
        
        std::string  get_message() const {return message_;} 
        std::shared_ptr<StackFrame> get_frame() const {return frame_;} 
        bool has_frame() const { return frame_!=nullptr;}

    protected:
        std::string message_;
        std::shared_ptr<StackFrame> frame_;
    };

    class VmTypeError : public VmError {
    public:
        VmTypeError(const std::string& msg, std::shared_ptr<StackFrame> frame, StringId expected, StringId actual) 
            : VmError(fmt::format("{} expected type {} actual {}", 
                msg, 
                expected, 
                actual), frame) {}
        
        VmTypeError(const std::string& msg, std::shared_ptr<StackFrame> frame, StringId actual) 
            : VmError(fmt::format("{} unexpected type {}", 
                msg, 
                actual), frame) {}
    };

    class VmResolvingError : public VmError {
    public:
        VmResolvingError(const std::string& msg, std::shared_ptr<StackFrame> frame, StringId name) 
            : VmError(fmt::format("{} can't resolve for name {}", 
                msg, 
                name), frame) {}
    };

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
        // Internal Helpers
        // ------------------------------------------------------------------------



    };

} // namespace vm