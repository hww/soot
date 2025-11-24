#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "stack_frame.hpp"
#include "binary_file.hpp"
#include "native_func.hpp"
#include "util/assert.h"
#include "util/log.h"
#include "module_manager.hpp"
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

                    if (!func_var.is_function()) {
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
                    dest = Variant(resolve_integer(current_frame, name_id));
                    break;
                }

                case Opcode::LOOKUP_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 name_id = instr.imm16;
                    dest = Variant(resolve_float(current_frame, name_id));
                    break;
                }

                case Opcode::LOOKUP_POINTER: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 name_id = instr.imm16;
                    auto module = current_frame->byte_code->owner_module;
                    dest = Variant(resolve_pointer(current_frame, name_id));
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
                    u32 offset = instr.imm16;
                    vm_int data_record = current_frame->get_static_int(offset);
                    dest = Variant(data_record);
                    break;
                }

                case Opcode::LOAD_STATIC_FLOAT: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 offset = instr.imm16;
                    vm_float data_record = current_frame->get_static_float(offset);
                    dest = Variant(data_record);
                    break;
                }

                case Opcode::LOAD_STATIC_POINTER: {
                    Variant& dest = current_frame->get_register(instr.a);
                    u32 offset = instr.imm16;
                    const void* data_record = current_frame->get_static_pointer(offset);
                    dest = Variant(data_record);
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
            lg::info("Native Functions: {}", native_functions_.size());
        }

        std::string to_string() const {
            return std::format("VirtualMachine(natives:{})",
                native_functions_.size());
        }


    private:
        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        vm_int resolve_integer(StackFrame* frame, StringId name) {
            auto module = frame->byte_code->owner_module;
            if (module) {
                Definition* resolved = module->resolve_symbol(name);
                if (resolved) {
                    if (resolved->type == type::i64)
                        return *((s64*)resolved->data_ptr.c());
                    if (resolved->type == type::i32)
                        return *((s32*)resolved->data_ptr.c());
                    if (resolved->type == type::i16)
                        return *((s16*)resolved->data_ptr.c());
                    if (resolved->type == type::i8)
                        return *((s8*)resolved->data_ptr.c());

                    if (resolved->type == type::u64)
                        return *((u64*)resolved->data_ptr.c());
                    if (resolved->type == type::u32)
                        return *((u32*)resolved->data_ptr.c());
                    if (resolved->type == type::u16)
                        return *((u16*)resolved->data_ptr.c());
                    if (resolved->type == type::u8)
                        return *((u8*)resolved->data_ptr.c());

                    throw new VmTypeError("resolve_integer", resolved->type);
                }
                else {
                    throw new VmResolvingError("resolve_integer", name);
                }
            }
        }

        vm_float resolve_float(StackFrame* frame, StringId name) {
            auto module = frame->byte_code->owner_module;
            if (module) {
                Definition* resolved = module->resolve_symbol(name);
                if (resolved) {
                    if (resolved->type == type::i64)
                        return *((s64*)resolved->data_ptr.c());
                    if (resolved->type == type::i32)
                        return *((s32*)resolved->data_ptr.c());
                    if (resolved->type == type::i16)
                        return *((s16*)resolved->data_ptr.c());
                    if (resolved->type == type::i8)
                        return *((s8*)resolved->data_ptr.c());

                    if (resolved->type == type::u64)
                        return *((u64*)resolved->data_ptr.c());
                    if (resolved->type == type::u32)
                        return *((u32*)resolved->data_ptr.c());
                    if (resolved->type == type::u16)
                        return *((u16*)resolved->data_ptr.c());
                    if (resolved->type == type::u8)
                        return *((u8*)resolved->data_ptr.c());

                    throw new VmTypeError("resolve_float", resolved->type);
                }
                else {
                    throw new VmResolvingError("resolve_float", name);
                }
            }
        }

        void* resolve_pointer(StackFrame* frame, StringId name) {
            auto module = frame->byte_code->owner_module;
            if (module) {
                Definition* resolved = module->resolve_symbol(name);
                if (resolved) {
                    return resolved->data_ptr.c();
                }
                else {
                    throw new VmResolvingError("resolve_pointer", name);
                }
            }
        }


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
            native_functions_.clear();
        }

        std::unordered_map<StringId, NativeFunction> native_functions_;

        // Модульная система
        ModuleManager& modules_ = ModuleManager::instance();

    };

} // namespace vm