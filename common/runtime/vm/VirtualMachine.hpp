#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/runtime/lib/Variant.hpp"
#include "common/runtime/vm/Instructions.hpp"
#include "common/runtime/vm/StackFrame.hpp"
#include "common/runtime/files/BinaryFile.hpp"
#include "common/runtime/modules/ModuleManager.hpp"
#include "common/runtime/kernel/NativeFunc.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include <unordered_map>
#include <memory>
#include <format>

using namespace runtime::lib;
using namespace runtime::files;
using namespace runtime::modules;
using namespace runtime::kernel;

namespace runtime::vm {

    // ============================================================================
    // Forward Declarations
    // ============================================================================
    
    class VirtualMachine;
    class StackFrame;

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
        explicit VmError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    class VmTypeError : public std::exception {
    public:
        explicit VmTypeError(const std::string& msg, StringId expected, StringId actual) : 
            message(fmt::format("{} expected type {} actual {}", 
                msg, string_id::to_cstring(expected), string_id::to_cstring(actual))) { }
        explicit VmTypeError(const std::string& msg, StringId actual) :
            message(fmt::format("{} unexpected type {}",
                msg, string_id::to_cstring(actual))) {
        }
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    class VmResolvingError : public std::exception {
    public:
        explicit VmResolvingError(const std::string& msg, StringId name) :
            message(fmt::format("{} can't resolve for name {}",
                msg, string_id::to_cstring(name))) {
        }
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
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

        Variant execute_bytecode(Module* module, StringId function);
        Variant execute_bytecode(ByteCode* bytecode);
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