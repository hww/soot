#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "stack_frame.hpp"
#include "binary_file.hpp"
#include "process.hpp"  // Добавлено
#include "scheduler.hpp" // Добавлено
#include "native_func.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <unordered_map>
#include <memory>
#include <format>

namespace vm {

    // ============================================================================
    // Forward Declarations
    // ============================================================================

    class VirtualMachine;
    struct Context;

    // ============================================================================
    // Virtual Machine Core with Process Integration
    // ============================================================================

    class VirtualMachine {
    public:
        static VirtualMachine& get_instance();

        VirtualMachine();
        ~VirtualMachine();

        // ------------------------------------------------------------------------
        // Binary File Management 
        // ------------------------------------------------------------------------
        bool load_binary(std::unique_ptr<BinaryFile> binary);
        bool load_binary_file(const std::string& filename);

        // ------------------------------------------------------------------------
        // Process Management (NEW - according to our basis)
        // ------------------------------------------------------------------------
        Process* create_process(const std::string& name, void* self_object = nullptr);
        void destroy_process(Process* process);
        Process* get_process(u32 pid) const;
        Process* get_process(const std::string& name) const;

        // ------------------------------------------------------------------------
        // Execution Control
        // ------------------------------------------------------------------------
        void execute_frame();  // Execute one frame of all processes
        Variant execute_function(StringId function_name);
        Variant execute_function(const std::string& function_name);
        Variant execute_bytecode(ByteCode* bytecode);

        // ------------------------------------------------------------------------
        // Instruction Execution (UPDATED for process context)
        // ------------------------------------------------------------------------
        void execute_instruction(StackFrame& frame, const Instruction& instr);

        // ------------------------------------------------------------------------
        // Native Function Management
        // ------------------------------------------------------------------------
        NativeFunction find_native_function(StringId name) const;

        // ------------------------------------------------------------------------
        // Utility Methods
        // ------------------------------------------------------------------------
        void dump_state() const;
        std::string to_string() const;

    private:
        std::vector<std::unique_ptr<BinaryFile>> binaries_;
        std::unordered_map<StringId, NativeFunction> native_functions_;

        // Process scheduler integration
        Scheduler& scheduler_;

        // Internal helpers
        ByteCode* find_function(StringId name);
        void initialize_native_functions();
        void register_system_calls();
        void cleanup();

        // Instruction execution helpers
        void execute_control_flow(StackFrame& frame, const Instruction& instr);
        void execute_arithmetic(StackFrame& frame, const Instruction& instr);
        void execute_comparison(StackFrame& frame, const Instruction& instr);
        void execute_lookup(StackFrame& frame, const Instruction& instr);
        void execute_memory_access(StackFrame& frame, const Instruction& instr);
    };

} // namespace vm