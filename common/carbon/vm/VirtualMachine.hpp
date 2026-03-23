#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/kernel/NativeFunc.hpp"


using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

namespace runtime::vm {

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
        explicit VmError(const std::string& msg, StackFrame* frame = nullptr) 
            : message_(msg), frame_(frame) {
            if (frame_) {
                auto instruction = frame_->get_this_instruction();
                message_ = fmt::format("{} [Frame: name={}, pc={}, argc={}, inst={}]",
                    message_,
                    lib::to_string(frame_->name),
                    frame_->pc,
                    frame_->argc,
                    instruction.to_string());
            }
        }
        
        const char* what() const noexcept override { return message_.c_str(); }
        
        std::string  get_message() const {return message_;} 
        StackFrame* get_frame() const {return frame_;} 
        bool has_frame() const { return frame_!=nullptr;}

    protected:
        std::string message_;
        StackFrame* frame_;
    };

    class VmTypeError : public VmError {
    public:
        VmTypeError(const std::string& msg, StackFrame* frame, StringId expected, StringId actual) 
            : VmError(fmt::format("{} expected type {} actual {}", 
                msg, 
                string_id::to_cstring(expected), 
                string_id::to_cstring(actual)), frame) {}
        
        VmTypeError(const std::string& msg, StackFrame* frame, StringId actual) 
            : VmError(fmt::format("{} unexpected type {}", 
                msg, 
                string_id::to_cstring(actual)), frame) {}
    };

    class VmResolvingError : public VmError {
    public:
        VmResolvingError(const std::string& msg, StackFrame* frame, StringId name) 
            : VmError(fmt::format("{} can't resolve for name {}", 
                msg, 
                string_id::to_cstring(name)), frame) {}
    };

    // ============================================================================
    // Virtual Machine Core
    // ============================================================================

    class VirtualMachine {
    public:
        VirtualMachine() {
        }

        ~VirtualMachine() {
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

        Variant execute_FunctionDesc(Module* module, StringId function);
        Variant execute_FunctionDesc(FunctionDesc* FunctionDesc);
        Variant execute(StackFrame* stack_frame);



    private:
        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        vm_int resolve_integer(StackFrame* frame, StringId name);

        vm_float resolve_float(StackFrame* frame, StringId name);

        void* resolve_pointer(StackFrame* frame, StringId name);


        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        StackFrame* create_root_frame() {
            StackFrame* frame = new StackFrame();
            frame->parent_ptr = nullptr;
            frame->pc = 0;
            frame->ret_num = 0;
            frame->argc = 0;
            return frame;
        }

        StackFrame* create_stack_frame(Instruction* code_ptr, u8* data_ptr, StackFrame* parent) {
            StackFrame* frame = new StackFrame();
            frame->parent_ptr = parent;
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