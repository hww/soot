#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "stack_frame.hpp"
#include "binary_file.hpp"
#include "native_func.hpp"
#include "util/assert.h"
#include "util/log.h"
#include "module_hub.hpp"
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
    // Native Function Interface
    // ============================================================================

    using NativeFunction = Variant(*)(u32 argc, const Variant* argv);

    // ============================================================================
    // Virtual Machine Core
    // ============================================================================

    class VirtualMachine {
    public:
        VirtualMachine() {
            initialize_native_functions();
        }

        ~VirtualMachine() {
            cleanup();
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

        Variant execute_bytecode(ByteCode* bytecode) {
            if (!bytecode) {
                lg::error("Cannot execute null bytecode");
                return Variant();
            }

            // Создаем корневой фрейм и основной фрейм выполнения
            StackFrame* current_frame = create_stack_frame(
                bytecode->get_code_ptr(),
                bytecode->get_data_ptr(),
                nullptr
            );

            Variant final_result;

            // Главный цикл выполнения
            while (current_frame != nullptr) {
                Instruction instr = current_frame->get_next_instruction();

#ifdef DEBUG_PRINT_EXECUTED_INSTRUCTION
                lg::debug("PC={} : {}", current_frame->pc - 1, instr.to_string());
#endif

                // ОДИН БОЛЬШОЙ SWITCH - как в оригинале
                switch (instr.opcode) {
                    // ============================================================
                    // Control Flow Instructions (0x0*)
                    // ============================================================
                case Opcode::RETURN: {
                    Variant return_value = current_frame->get_register(instr.a);
                    StackFrame* parent_frame = current_frame->parent_ptr;

                    if (parent_frame == nullptr) {
                        final_result = return_value;
                    }
                    else {
                        parent_frame->get_register(current_frame->ret_num) = return_value;
                    }

                    StackFrame* old_frame = current_frame;
                    current_frame = parent_frame;
                    destroy_stack_frame(old_frame);
                    break;
                }

                case Opcode::MOVE: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = src;
                    break;
                }

                case Opcode::CALL: {
                    Variant& func_var = current_frame->get_register(instr.a);

                    if (!func_var.is_lambda()) {
                        lg::error("Call target is not a lambda: {}", func_var.to_string());
                        current_frame = nullptr;
                        break;
                    }

                    ByteCode* target_code = reinterpret_cast<ByteCode*>(func_var.get_ptr());
                    StackFrame* new_frame = create_stack_frame(
                        target_code->get_code_ptr(),
                        target_code->get_data_ptr(),
                        current_frame
                    );

                    new_frame->ret_num = instr.b;
                    new_frame->argc = instr.c;

                    for (u32 i = 0; i < new_frame->argc; i++) {
                        new_frame->get_register(ARG_REGISTERS_OFFSET + i) =
                            current_frame->get_register(ARG_REGISTERS_OFFSET + i);
                    }

                    current_frame = new_frame;
                    break;
                }

                case Opcode::CALL_NATIVE: {
                    Variant& func_var = current_frame->get_register(instr.a);
                    NativeFunction native_func = nullptr;

                    if (func_var.is_ptr() && func_var.get_type() == SID("native")) {
                        native_func = reinterpret_cast<NativeFunction>(func_var.get_ptr());
                    }
                    else if (func_var.is_sid()) {
                        native_func = find_native_function(func_var.get_sid());
                    }

                    if (!native_func) {
                        lg::error("Native function not found: {}", func_var.to_string());
                        break;
                    }

                    Variant& result_reg = current_frame->get_register(instr.b);
                    u32 argc = instr.c;

                    Variant* argv = &current_frame->get_register(ARG_REGISTERS_OFFSET);
                    result_reg = native_func(argc, argv);
                    break;
                }

                case Opcode::BRANCH: {
                    current_frame->pc = instr.imm16;
                    break;
                }

                case Opcode::BRANCH_IF: {
                    Variant& condition = current_frame->get_register(instr.a);
                    if (condition.to_bool()) {
                        current_frame->pc = instr.imm16;
                    }
                    break;
                }

                case Opcode::BRANCH_IF_NOT: {
                    Variant& condition = current_frame->get_register(instr.a);
                    if (!condition.to_bool()) {
                        current_frame->pc = instr.imm16;
                    }
                    break;
                }

                // ============================================================
                // Integer Arithmetic Instructions (0x1*)
                // ============================================================
                case Opcode::ADD_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() + src2.to_int());
                    break;
                }

                case Opcode::SUB_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() - src2.to_int());
                    break;
                }

                case Opcode::MUL_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() * src2.to_int());
                    break;
                }

                case Opcode::DIV_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    s32 divisor = src2.to_int();
                    if (divisor == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_int() / divisor);
                    break;
                }

                case Opcode::MOD_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    s32 divisor = src2.to_int();
                    if (divisor == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_int() % divisor);
                    break;
                }

                case Opcode::ABS_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(std::abs(src.to_int()));
                    break;
                }

                case Opcode::NEG_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(-src.to_int());
                    break;
                }

                case Opcode::ASH_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    s32 value = src1.to_int();
                    s32 shift = src2.to_int();
                    dest = Variant(shift >= 0 ? value << shift : value >> -shift);
                    break;
                }

                case Opcode::TO_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(src.to_int());
                    break;
                }

                // ============================================================
                // Integer Immediate Instructions (0x2*)
                // ============================================================
                case Opcode::LOAD_IMMEDIATE_INT: {
                    Variant& dest = current_frame->get_register(instr.a_imm);
                    dest = Variant(static_cast<s32>(instr.imm16));
                    break;
                }

                case Opcode::ADD_IMM: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    s32 imm = static_cast<s32>(instr.c);
                    dest = Variant(src.to_int() + imm);
                    break;
                }

                case Opcode::SUB_IMM: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    s32 imm = static_cast<s32>(instr.c);
                    dest = Variant(src.to_int() - imm);
                    break;
                }

                case Opcode::MUL_IMM: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    s32 imm = static_cast<s32>(instr.c);
                    dest = Variant(src.to_int() * imm);
                    break;
                }

                case Opcode::DIV_IMM: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    s32 imm = static_cast<s32>(instr.c);
                    if (imm == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src.to_int() / imm);
                    break;
                }

                // ============================================================
                // Floating Point Instructions (0x3*)
                // ============================================================
                case Opcode::ADD_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() + src2.to_float());
                    break;
                }

                case Opcode::SUB_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() - src2.to_float());
                    break;
                }

                case Opcode::MUL_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() * src2.to_float());
                    break;
                }

                case Opcode::DIV_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    f32 divisor = src2.to_float();
                    if (divisor == 0.0f) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_float() / divisor);
                    break;
                }

                case Opcode::MOD_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(std::fmod(src1.to_float(), src2.to_float()));
                    break;
                }

                case Opcode::ABS_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(std::fabs(src.to_float()));
                    break;
                }

                case Opcode::NEG_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(-src.to_float());
                    break;
                }

                case Opcode::TO_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(src.to_float());
                    break;
                }

                // ============================================================
                // Comparison Instructions (0x4*)
                // ============================================================
                case Opcode::CMP_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() == src2.to_int());
                    break;
                }

                case Opcode::CMP_GT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() > src2.to_int());
                    break;
                }

                case Opcode::CMP_GT_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() >= src2.to_int());
                    break;
                }

                case Opcode::CMP_LT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() < src2.to_int());
                    break;
                }

                case Opcode::CMP_LT_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() <= src2.to_int());
                    break;
                }

                case Opcode::CMP_FLOAT_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(std::fabs(src1.to_float() - src2.to_float()) < 0.0001f);
                    break;
                }

                case Opcode::CMP_FLOAT_GT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() > src2.to_float());
                    break;
                }

                case Opcode::CMP_FLOAT_GT_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() >= src2.to_float());
                    break;
                }

                case Opcode::CMP_FLOAT_LT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() < src2.to_float());
                    break;
                }

                case Opcode::CMP_FLOAT_LT_EQUAL: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() <= src2.to_float());
                    break;
                }

                // ============================================================
                // Logical Instructions (0x5*)
                // ============================================================
                case Opcode::LOG_AND: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_bool() && src2.to_bool());
                    break;
                }

                case Opcode::LOG_OR: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_bool() || src2.to_bool());
                    break;
                }

                case Opcode::LOG_NOT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(!src.to_bool());
                    break;
                }

                // ============================================================
                // Bitwise Instructions (0x6*)
                // ============================================================
                case Opcode::BIT_AND: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() & src2.to_int());
                    break;
                }

                case Opcode::BIT_OR: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() | src2.to_int());
                    break;
                }

                case Opcode::BIT_XOR: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() ^ src2.to_int());
                    break;
                }

                case Opcode::BIT_NOR: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src1 = current_frame->get_register(instr.b);
                    Variant& src2 = current_frame->get_register(instr.c);
                    dest = Variant(~(src1.to_int() | src2.to_int()));
                    break;
                }

                case Opcode::BIT_NOT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(~src.to_int());
                    break;
                }

                // ============================================================
                // Utility Instructions (0x7*)
                // ============================================================
                case Opcode::LOAD_ARGC: {
                    Variant& dest = current_frame->get_register(instr.a);
                    dest = Variant(static_cast<s32>(current_frame->argc));
                    break;
                }

                case Opcode::GET_SID_STRING: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = Variant(string_id::to_string(src.get_sid()));
                    break;
                }

                // ============================================================
                // Lookup Instructions (0x8*)
                // ============================================================
                case Opcode::LOOKUP_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 name_id = instr.imm16;
                    // TODO: Implement environment lookup
                    lg::warn("LOOKUP_INT not implemented - needs environment access");
                    break;
                }

                case Opcode::LOOKUP_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 name_id = instr.imm16;
                    // TODO: Implement environment lookup
                    lg::warn("LOOKUP_FLOAT not implemented - needs environment access");
                    break;
                }

                case Opcode::LOOKUP_POINTER: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 name_id = instr.imm16;
                    // TODO: Implement environment lookup
                    lg::warn("LOOKUP_POINTER not implemented - needs environment access");
                    break;
                }

                // ============================================================
                // Indirect Load Instructions (0x9*)
                // ============================================================
                case Opcode::LOAD_IND_INT: {
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regB.is_ptr()) {
                        Variant& regA = current_frame->get_register(instr.a);
                        s32 val = *reinterpret_cast<s32*>(regB.get_ptr());
                        regA = Variant(val);
                    }
                    else {
                        lg::error("LOAD_IND_INT: Expected pointer, got {}", regB.to_string());
                    }
                    break;
                }

                case Opcode::LOAD_IND_FLOAT: {
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regB.is_ptr()) {
                        Variant& regA = current_frame->get_register(instr.a);
                        f32 val = *reinterpret_cast<f32*>(regB.get_ptr());
                        regA = Variant(val);
                    }
                    else {
                        lg::error("LOAD_IND_FLOAT: Expected pointer, got {}", regB.to_string());
                    }
                    break;
                }

                case Opcode::LOAD_IND_POINTER: {
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regB.is_ptr()) {
                        Variant& regA = current_frame->get_register(instr.a);
                        void* val = *reinterpret_cast<void**>(regB.get_ptr());
                        regA = Variant(val);
                    }
                    else {
                        lg::error("LOAD_IND_POINTER: Expected pointer, got {}", regB.to_string());
                    }
                    break;
                }

                // ============================================================
                // Indirect Store Instructions (0xA*)
                // ============================================================
                case Opcode::STORE_IND_INT: {
                    Variant& regA = current_frame->get_register(instr.a);
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regA.is_ptr()) {
                        *reinterpret_cast<s32*>(regA.get_ptr()) = regB.to_int();
                    }
                    else {
                        lg::error("STORE_IND_INT: Expected pointer, got {}", regA.to_string());
                    }
                    break;
                }

                case Opcode::STORE_IND_FLOAT: {
                    Variant& regA = current_frame->get_register(instr.a);
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regA.is_ptr()) {
                        *reinterpret_cast<f32*>(regA.get_ptr()) = regB.to_float();
                    }
                    else {
                        lg::error("STORE_IND_FLOAT: Expected pointer, got {}", regA.to_string());
                    }
                    break;
                }

                case Opcode::STORE_IND_POINTER: {
                    Variant& regA = current_frame->get_register(instr.a);
                    Variant& regB = current_frame->get_register(instr.b);
                    if (regA.is_ptr()) {
                        *reinterpret_cast<void**>(regA.get_ptr()) = regB.get_ptr();
                    }
                    else {
                        lg::error("STORE_IND_POINTER: Expected pointer, got {}", regA.to_string());
                    }
                    break;
                }

                // ============================================================
                // Static Load Instructions (0xB*)
                // ============================================================
                case Opcode::LOAD_STATIC_INT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 data_index = instr.imm16;
                    Record* data_record = &current_frame->data_ptr[data_index];
                    dest = Variant(data_record->as_s32);
                    break;
                }

                case Opcode::LOAD_STATIC_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 data_index = instr.imm16;
                    Record* data_record = &current_frame->data_ptr[data_index];
                    dest = Variant(data_record->as_f32);
                    break;
                }

                case Opcode::LOAD_STATIC_POINTER: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 data_index = instr.imm16;
                    Record* data_record = &current_frame->data_ptr[data_index];
                    dest = Variant(data_record->as_ptr);
                    break;
                }

                // ============================================================
                // Default case for unknown opcodes
                // ============================================================
                default: {
                    lg::error("Unknown opcode: {} (0x{:02X})", static_cast<u32>(instr.opcode), static_cast<u32>(instr.opcode));
                    current_frame = nullptr;
                    break;
                }
                }
            }

            return final_result;
        }

        // ------------------------------------------------------------------------
        // Utility Methods
        // ------------------------------------------------------------------------

        void dump_state() const {
            lg::info("=== VM State ===");
            lg::info("Loaded Binaries: {}", binaries_.size());
            lg::info("Native Functions: {}", native_functions_.size());
        }

        std::string to_string() const {
            return std::format("VirtualMachine(binaries:{}, natives:{})",
                binaries_.size(), native_functions_.size());
        }

        // ------------------------------------------------------------------------
        // Модульная система
        // ------------------------------------------------------------------------

        bool use_module(const std::string& module_name);
        bool use_module(StringId module_name);
        void release_module(const std::string& module_name);
        void release_module(StringId module_name);

        Variant execute_exported(const std::string& module_name,
            const std::string& function_name);
        Variant execute_exported(StringId module_name, StringId function_name);

        // Проверка поколений модулей (для Process Manager)
        bool is_module_generation_current(StringId module_name) const;
        std::vector<StringId> get_stale_modules() const;


    private:
        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        ByteCode* find_function(StringId name) {
            for (auto& binary_ptr : binaries_) {
                auto header = binary_ptr->get_header();
                // ИСПРАВЛЕНИЕ: defs_count вместо defs_num
                for (u32 i = 0; i < header->defs_count; i++) {
                    auto def = header->get_definition(i);
                    if (def->name == name) {
                        // ИСПРАВЛЕНИЕ: разыменование Ptr<ByteCode>
                        return header->get_definition_ptr<ByteCode>(i).c();
                    }
                }
            }
            return nullptr;
        }

        StackFrame* create_root_frame() {
            StackFrame* frame = new StackFrame();
            frame->parent_ptr = nullptr;
            frame->pc = 0;
            frame->ret_num = 0;
            frame->argc = 0;
            return frame;
        }

        StackFrame* create_stack_frame(Instruction* code_ptr, Record* data_ptr, StackFrame* parent) {
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

        void initialize_native_functions() {
            REGISTER_NATIVE_FUNCTION(SID("print"), [](u32 argc, const Variant* argv) -> Variant {
                for (u32 i = 0; i < argc; i++) {
                    lg::print("{} ", argv[i].to_string());
                }
                return Variant(true);
                });

            REGISTER_NATIVE_FUNCTION(SID("println"), [](u32 argc, const Variant* argv) -> Variant {
                for (u32 i = 0; i < argc; i++) {
                    lg::print("{} ", argv[i].to_string());
                }
                lg::print("\n");
                return Variant(true);
                });
        }

        void cleanup() {
            binaries_.clear();
            native_functions_.clear();
        }

        std::vector<std::unique_ptr<BinaryFile>> binaries_;  
        std::unordered_map<StringId, NativeFunction> native_functions_;

        // Модульная система
        GlobalModuleHub& hub_ = GlobalModuleHub::instance();
        std::unordered_map<StringId, u32> used_modules_;  // module -> generation

    };

} // namespace vm