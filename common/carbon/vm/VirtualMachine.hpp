#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/kernel/NativeFunc.hpp"
#include <memory>


using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;

namespace carbon {

    // Running modes
    enum class RunMode { Run, Step, StepIn, StepOut };

    // ============================================================================
    // Forward Declarations
    // ============================================================================
    
    class VirtualMachine;
    struct StackFrame;

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
        bool enable_debug_log;
        bool is_suspended;
        bool is_break;
        bool is_error;
        std::string break_reason;
        std::shared_ptr<StackFrame> current_frame;

        VirtualMachine() {
            enable_debug_log = true;
            is_suspended = false;
            is_break = false;
            is_error = false;
            break_reason = "";
            current_frame = nullptr; 
        }

        ~VirtualMachine() {
        }

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

        Variant execute_function(Module* module, StringId function, RunMode mode = RunMode::Run);
        Variant execute_function(ScriptLambda* script_lambda, RunMode mode = RunMode::Run);
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

        std::shared_ptr<StackFrame> create_root_frame() {
            auto frame = std::make_shared<StackFrame>();
            frame->pc = 0;
            frame->ret_num = 0;
            frame->argc = 0;
            return frame;
        }

        std::shared_ptr<StackFrame> create_stack_frame(Instruction* code_ptr, u64* data_ptr, std::shared_ptr<StackFrame> parent) {
            auto frame = std::make_shared<StackFrame>();
            frame->parent = parent;
            frame->code_ptr = code_ptr;
            frame->data_ptr = data_ptr;
            frame->pc = 0;
            frame->ret_num = 0;
            frame->argc = 0;
            return frame;
        }

        void destroy_stack_frame(StackFrame* frame) {
            delete frame;
        }
    };

} // namespace vm