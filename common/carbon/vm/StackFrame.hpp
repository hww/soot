#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include <vector>
#include <format>
#include <iostream>
#include <ostream>

using namespace runtime::lib;
using namespace runtime::files;

namespace runtime::vm {

    // ============================================================================
    // Stack Frame Structure
    // ============================================================================

    struct StackFrame {
        /// Тип фрейма (для определения поведения при очистке)
        enum class FrameType {
            CATCH,      // Обработка исключений
            PROTECT,    // Защищенный блок с cleanup
            STATE,      // Состояние процесса
            GENERIC     // Произвольный защищенный блок
        };

        // ------------------------------------------------------------------------
        // Execution State
        // ------------------------------------------------------------------------
        StringId name;                      // Имя фрейма (для отладки и throw)
        FrameType frame_type;               // Тип фрейма
        ByteCode* byte_code = nullptr;
        Instruction* code_ptr = nullptr;    // Pointer to bytecode instructions
        u8* data_ptr = nullptr;             // Pointer to static data
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


        StackFrame(ByteCode* bytecode, StackFrame* parent = nullptr,
            FrameType frame_type = FrameType::GENERIC, StringId name = SID("null"));

        virtual ~StackFrame(){}

        /// Вызывается при выходе из области видимости фрейма
        virtual void exit() {}

        /// Вызывается при исключении через этот фрейм
        virtual void on_throw() {}

        // ------------------------------------------------------------------------
        // Register Access (ČŃĎĐŔÂËĹÍÍŰĹ ĂĐŔÍČÖŰ)
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

        vm_int get_static_int(u32 offset) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            // Data is stored as Variants, so we need to extract s32
            return *((s32*)(data_ptr + offset));
        }

        vm_float get_static_float(u32 offset) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            return *((float*)(data_ptr + offset));
        }

        const void* get_static_pointer(u32 offset) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            return (void*)(data_ptr + offset);
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
            return fmt::format("StackFrame(pc:{}, argc:{}, ret_reg:{}, parent:{:p})",
                pc, argc, ret_num, (void*)parent_ptr);
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

    inline StackFrame* create_stack_frame(ByteCode* bytecode, StackFrame* parent = nullptr) {
        return new StackFrame(bytecode, parent);
    }

    inline void destroy_stack_frame(StackFrame* frame) {
        if (frame) {
            delete frame;
        }
    }

    inline StackFrame* push_stack_frame(ByteCode* bytecode, StackFrame* parent = nullptr) {
        return create_stack_frame(bytecode, parent);
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


    /**
     * Фрейм для перехвата исключений (аналог try-catch)
     * Сохраняет контекст выполнения для возможности отката
     */
    class CatchFrame : public StackFrame {
    public:

        CatchFrame(ByteCode* bytecode, StackFrame* parent = nullptr, StringId tag_name = SID("null"))
            : StackFrame(bytecode, parent, FrameType::CATCH, tag_name) {
        }

        void exit() override {
            // Для catch фрейма очистка обычно не требуется
            // кроме случаев, когда мы выходим без исключения
        }

        void on_throw() override {
            // Восстанавливаем контекст выполнения
            restore_execution_context();
        }

    private:
        void restore_execution_context() {
            // Восстановление регистров и стека
            // Ассемблерная реализация зависит от платформы
        }
    };

    /**
     * Фрейм для гарантированного выполнения cleanup-кода
     * Аналог RAII в C++ или defer в Go
     */
    class ProtectFrame : public StackFrame {
    public:
        /// Функция очистки (вызывается при выходе)
        std::function<void()> cleanup_function;

        /// Данные для очистки (опционально)
        void* user_data = nullptr;

        ProtectFrame(ByteCode* bytecode, StackFrame* parent = nullptr, std::function<void()> cleanup = nullptr)
            : StackFrame(bytecode, parent, FrameType::PROTECT, SID("protected"))
            , cleanup_function(std::move(cleanup)) {
        }

        void exit() override {
            if (cleanup_function) {
                cleanup_function();
            }
        }

        /// Устанавливает пользовательские данные
        void set_user_data(void* data) {
            user_data = data;
        }
    };

} // namespace vm