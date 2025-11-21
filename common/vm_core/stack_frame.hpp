#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <vector>
#include <format>

namespace vm {


    // ============================================================================
    // Forward Declarations
    // ============================================================================

    struct ByteCode;
    struct Context;

    // ============================================================================
    // Stack Frame Structure
    // ============================================================================

    struct StackFrame {
        // ------------------------------------------------------------------------
        // Execution State
        // ------------------------------------------------------------------------
        Instruction* code_ptr = nullptr;    // Pointer to bytecode instructions
        Variant* data_ptr = nullptr;        // Pointer to static data
        StackFrame* parent_ptr = nullptr;   // Parent frame (for call stack)
        u32 pc = 0;                         // Program counter
        u32 argc = 0;                       // Number of arguments
        u32 ret_num = 0;                    // Return register number

        // ------------------------------------------------------------------------
        // Registers
        // ------------------------------------------------------------------------
        Variant registers[MAX_REGISTERS];   // All registers (locals + args)

        // ------------------------------------------------------------------------
        // Constructors
        // ------------------------------------------------------------------------

        StackFrame() {
            initialize_registers();
        }

        StackFrame(Instruction* code, Variant* data, StackFrame* parent = nullptr)
            : code_ptr(code), data_ptr(data), parent_ptr(parent), pc(0), argc(0), ret_num(0) {
            initialize_registers();
        }

        // ------------------------------------------------------------------------
        // Register Access (»—œ–¿¬À≈ÕÕ€≈ √–¿Õ»÷€)
        // ------------------------------------------------------------------------

        Variant& get_register(u32 index) {
            ASSERT_FORMAT(index < MAX_REGISTERS, "Register index out of bounds: {} (max: {})", index, MAX_REGISTERS - 1);
            return registers[index];
        }

        const Variant& get_register(u32 index) const {
            ASSERT_FORMAT(index < MAX_REGISTERS, "Register index out of bounds: {} (max: {})", index, MAX_REGISTERS - 1);
            return registers[index];
        }

        Variant& get_argument(u32 index) {
            ASSERT_FORMAT(index < MAX_ARGS, "Argument index out of bounds: {} (max: {})", index, MAX_ARGS - 1);
            u32 reg_index = ARG_REGISTERS_OFFSET + index;
            return registers[reg_index];
        }

        const Variant& get_argument(u32 index) const {
            ASSERT_FORMAT(index < MAX_ARGS, "Argument index out of bounds: {} (max: {})", index, MAX_ARGS - 1);
            u32 reg_index = ARG_REGISTERS_OFFSET + index;
            return registers[reg_index];
        }

        Variant& get_local(u32 index) {
            ASSERT_FORMAT(index < MAX_LOCALS, "Local index out of bounds: {} (max: {})", index, MAX_LOCALS - 1);
            u32 reg_index = LOCAL_REGISTERS_OFFSET + index;
            return registers[reg_index];
        }

        const Variant& get_local(u32 index) const {
            ASSERT_FORMAT(index < MAX_LOCALS, "Local index out of bounds: {} (max: {})", index, MAX_LOCALS - 1);
            u32 reg_index = LOCAL_REGISTERS_OFFSET + index;
            return registers[reg_index];
        }

        // ------------------------------------------------------------------------
        // Instruction Execution
        // ------------------------------------------------------------------------

        Instruction get_next_instruction() {
            ASSERT_MSG(code_ptr != nullptr, "No code pointer set");
            ASSERT_FORMAT(pc < get_code_size(), "Program counter out of bounds: {}", pc);

            Instruction instr = code_ptr[pc];
            pc++;
            return instr;
        }

        void jump_to(u32 new_pc) {
            ASSERT_FORMAT(new_pc < get_code_size(), "Jump target out of bounds: {}", new_pc);
            pc = new_pc;
        }

        u32 get_code_size() const {
            if (!code_ptr) return 0;
            // This would need to be stored in the frame or bytecode header
            // For now, we'll assume a large enough buffer
            return 1024; // Temporary - should come from ByteCode
        }

        // ------------------------------------------------------------------------
        // Data Access
        // ------------------------------------------------------------------------

        s32 get_static_s32(u32 index) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            // Data is stored as Variants, so we need to extract s32
            return data_ptr[index].get_int32();
        }

        f32 get_static_f32(u32 index) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            return data_ptr[index].get_float();
        }

        const void* get_static_pointer(u32 index) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            return data_ptr[index].get_ptr();
        }

        // ------------------------------------------------------------------------
        // Frame Management
        // ------------------------------------------------------------------------

        void setup_call(u32 num_args, u32 return_reg) {
            argc = num_args;
            ret_num = return_reg;
            pc = 0; // Start execution from beginning
        }

        void copy_arguments_from(const StackFrame& caller_frame) {
            for (u32 i = 0; i < argc && i < MAX_ARGS; i++) {
                get_argument(i) = caller_frame.get_argument(i);
            }
        }

        // ------------------------------------------------------------------------
        // Debugging
        // ------------------------------------------------------------------------

        std::string to_string() const {
            return std::format("StackFrame(pc:{}, argc:{}, ret_reg:{}, parent:{:x})",
                pc, argc, ret_num, (int)parent_ptr);
        }

        void dump_registers() const {
            lg::debug("=== Register Dump ===");

            // Local registers
            for (u32 i = 0; i < ARG_REGISTERS_OFFSET; i++) {
                if (!registers[i].is_null()) {
                    lg::debug("  r{:2d}: {}", i, registers[i].to_string());
                }
            }

            // Argument registers
            lg::debug("  --- Arguments ---");
            for (u32 i = ARG_REGISTERS_OFFSET; i < MAX_REGISTERS; i++) {
                if (!registers[i].is_null()) {
                    lg::debug("  r{:2d}: {}", i, registers[i].to_string());
                }
            }
        }

    private:
        void initialize_registers() {
            for (u32 i = 0; i < MAX_REGISTERS; i++) {
                registers[i] = Variant(); // Initialize as nil
            }
        }
    };

    // ============================================================================
    // Stack Frame Management Functions
    // ============================================================================

    inline StackFrame* create_stack_frame(Instruction* code, Variant* data, StackFrame* parent = nullptr) {
        return new StackFrame(code, data, parent);
    }

    inline void destroy_stack_frame(StackFrame* frame) {
        if (frame) {
            delete frame;
        }
    }

    inline StackFrame* push_stack_frame(Instruction* code, Variant* data, StackFrame* parent) {
        return create_stack_frame(code, data, parent);
    }

    inline StackFrame* pop_stack_frame(StackFrame* frame) {
        if (!frame) return nullptr;

        StackFrame* parent = frame->parent_ptr;
        destroy_stack_frame(frame);
        return parent;
    }

    // ============================================================================
    // Utility Functions
    // ============================================================================

    inline std::ostream& operator<<(std::ostream& os, const StackFrame& frame) {
        return os << frame.to_string();
    }

} // namespace vm