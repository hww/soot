#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "stack_frame.hpp"
#include "bytecode.hpp"
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
        // Binary File Management
        // ------------------------------------------------------------------------

        bool load_binary(const BinaryFile& binary) {
            binaries_.push_back(binary);
            lg::info("Loaded binary: {}", binary.to_string());
            return true;
        }

        bool load_binary_file(const std::string& filename) {
            BinaryFile binary;
            if (binary.load_from_file(filename)) {
                return load_binary(binary);
            }
            return false;
        }

        // ------------------------------------------------------------------------
        // Function Execution
        // ------------------------------------------------------------------------

        Variant execute_function(StringId function_name) {
            ByteCode* bytecode = find_function(function_name);
            if (!bytecode) {
                lg::error("Function not found: {}", string_id_to_string(function_name));
                return Variant();
            }

            return execute_bytecode(bytecode);
        }

        Variant execute_function(const std::string& function_name) {
            return execute_function(string_id::from_string(function_name));
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
            StackFrame* root_frame = create_root_frame();
            StackFrame* current_frame = create_stack_frame(
                bytecode->get_code_ptr(),
                bytecode->get_data_ptr(),
                root_frame
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
                    // Control Flow Instructions
                    // ============================================================
                case Opcode::RETURN: {
                    Variant return_value = current_frame->get_register(instr.a);
                    StackFrame* parent_frame = current_frame->parent_ptr;

                    if (parent_frame == root_frame) {
                        // Возврат в корневой фрейм - сохраняем финальный результат
                        final_result = return_value;
                        parent_frame->get_register(current_frame->ret_num) = return_value;
                    }
                    else if (parent_frame != nullptr) {
                        // Обычный возврат в родительскую функцию
                        parent_frame->get_register(current_frame->ret_num) = return_value;
                    }

                    // Переходим к родительскому фрейму (может стать nullptr)
                    StackFrame* old_frame = current_frame;
                    current_frame = parent_frame;
                    destroy_stack_frame(old_frame);
                    break;
                }

                case Opcode::CALL: {
                    Variant& func_var = current_frame->get_register(instr.a);

                    if (!func_var.is_lambda()) {
                        lg::error("Call target is not a lambda: {}", func_var.to_string());
                        current_frame = nullptr; // Завершаем выполнение
                        break;
                    }

                    ByteCode* target_code = reinterpret_cast<ByteCode*>(func_var.get_ptr());
                    StackFrame* new_frame = create_stack_frame(
                        target_code->get_code_ptr(),
                        target_code->get_data_ptr(),
                        current_frame
                    );

                    // Настраиваем новый фрейм
                    new_frame->ret_num = instr.b;
                    new_frame->argc = instr.c;

                    // Копируем аргументы из текущего фрейма
                    for (u32 i = 0; i < new_frame->argc; i++) {
                        new_frame->get_register(ARG_REGISTERS_OFFSET + i) =
                            current_frame->get_register(ARG_REGISTERS_OFFSET + i);
                    }

                    current_frame = new_frame;
                    break;
                }

                case Opcode::CALL_NATIVE: {
                    Variant& func_var = current_frame->get_register(instr.a);

                    // Поддержка как прямых указателей, так и поиска по имени
                    NativeFunction native_func = nullptr;

                    if (func_var.is_ptr() && func_var.get_type() == "native"_sid) {
                        // Прямой указатель на функцию
                        native_func = reinterpret_cast<NativeFunction>(func_var.get_ptr());
                    }
                    else if (func_var.is_sid()) {
                        // Поиск по имени
                        native_func = find_native_function(func_var.get_sid());
                    }

                    if (!native_func) {
                        lg::error("Native function not found: {}", func_var.to_string());
                        break;
                    }

                    Variant& result_reg = current_frame->get_register(instr.b);
                    u32 argc = instr.c;

                    // Вызываем нативную функцию
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

                case Opcode::MOVE: {
                    Variant& dest = current_frame->get_register(instr.a);
                    Variant& src = current_frame->get_register(instr.b);
                    dest = src;
                    break;
                }

                                 // ============================================================
                                 // Integer Arithmetic
                                 // ============================================================
                case Opcode::LOAD_IMMEDIATE_INT: {
                    Variant& dest = current_frame->get_register(instr.a_imm);
                    dest = Variant(static_cast<s32>(instr.imm16));
                    break;
                }

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

                                    // ============================================================
                                    // Floating Point Arithmetic
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

                                      // ============================================================
                                      // Comparisons
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

                                         // ... добавь остальные инструкции по аналогии

                default: {
                    lg::error("Unknown opcode: {}", static_cast<u32>(instr.opcode));
                    current_frame = nullptr; // Завершаем выполнение при неизвестной инструкции
                    break;
                }
                }
            }

            // Очищаем корневой фрейм
            destroy_stack_frame(root_frame);

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

    private:
        // ------------------------------------------------------------------------
        // Internal Helpers
        // ------------------------------------------------------------------------

        ByteCode* find_function(StringId name) {
            for (auto& binary : binaries_) {
                ByteCode* func = binary.get_function(name);
                if (func) return func;
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

        StackFrame* create_stack_frame(Instruction* code_ptr, Variant* data_ptr, StackFrame* parent) {
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
            REGISTER_NATIVE_FUNCTION("print"_sid, [](u32 argc, const Variant* argv) -> Variant {
                for (u32 i = 0; i < argc; i++) {
                    lg::print("{} ", argv[i].to_string());
                }
                return Variant(true);
                });

            REGISTER_NATIVE_FUNCTION("println"_sid, [](u32 argc, const Variant* argv) -> Variant {
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

        std::vector<BinaryFile> binaries_;
        std::unordered_map<StringId, NativeFunction> native_functions_;
    };

} // namespace vm