#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/EventMessage.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include "file/DCScript.hpp"
#include "lib/StringId.hpp"
#include <iostream>
#include <ostream>
#include <functional>
#include <memory>  

using namespace carbon;

namespace carbon {

    // ============================================================================
    // Stack Frame Structure
    // ============================================================================

    struct StackFrame : public std::enable_shared_from_this<StackFrame> {
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
        StringId name;                          // Имя фрейма (для отладки и throw)
        FrameType frame_type;                   // Тип фрейма
        ScriptLambda* byte_code = nullptr;      // Pointer to FunctionDesc
        Instruction* code_ptr = nullptr;        // Pointer to FunctionDesc instructions
        u64* data_ptr = nullptr;                // Pointer to static data
        std::shared_ptr<StackFrame> parent;     // Parent frame (for call stack)
        u32 pc = 0;                             // Program counter
        u32 argc = 0;                           // Number of arguments
        u32 ret_num = 0;                        // Return register number

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

       
        StackFrame(ScriptLambda* functionDesc, std::shared_ptr<StackFrame> parent = nullptr,
            FrameType frame_type = FrameType::GENERIC, StringId name = StringIds::none);

        virtual ~StackFrame(){}

        /// Вызывается при выходе из области видимости фрейма
        virtual void exit() {}

        /// Вызывается при исключении через этот фрейм
        virtual void on_throw() {}

        std::shared_ptr<StackFrame> get_parent() const {
            return parent;
        }
        // ------------------------------------------------------------------------
        // Register Access
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

        // ============================================================================
        // Register Access Methods
        // ============================================================================

        // Установка значения в регистр по индексу
        void set_register(u32 index, const Variant& value) {
            ASSERT_FORMAT(index < MAX_REGISTERS, "Register index out of bounds: {} (max: {})", index, MAX_REGISTERS - 1);
            registers[index] = value;
        }

        void set_register(u32 index, Variant&& value) {
            ASSERT_FORMAT(index < MAX_REGISTERS, "Register index out of bounds: {} (max: {})", index, MAX_REGISTERS - 1);
            registers[index] = std::move(value);
        }

        // Установка значения в аргумент по индексу
        void set_argument(u32 index, const Variant& value) {
            ASSERT_FORMAT(index < MAX_ARGS, "Argument index out of bounds: {} (max: {})", index, MAX_ARGS - 1);
            u32 reg_index = ARG_REGISTERS_OFFSET + index;
            registers[reg_index] = value;
        }

        void set_argument(u32 index, Variant&& value) {
            ASSERT_FORMAT(index < MAX_ARGS, "Argument index out of bounds: {} (max: {})", index, MAX_ARGS - 1);
            u32 reg_index = ARG_REGISTERS_OFFSET + index;
            registers[reg_index] = std::move(value);
        }

        // Установка значения в локальную переменную по индексу
        void set_local(u32 index, const Variant& value) {
            ASSERT_FORMAT(index < MAX_LOCALS, "Local index out of bounds: {} (max: {})", index, MAX_LOCALS - 1);
            u32 reg_index = LOCAL_REGISTERS_OFFSET + index;
            registers[reg_index] = value;
        }

        void set_local(u32 index, Variant&& value) {
            ASSERT_FORMAT(index < MAX_LOCALS, "Local index out of bounds: {} (max: {})", index, MAX_LOCALS - 1);
            u32 reg_index = LOCAL_REGISTERS_OFFSET + index;
            registers[reg_index] = std::move(value);
        }

        // ============================================================================
        // Convenience Methods for Common Types
        // ============================================================================

        // Установка аргумента с конкретными типами
        void set_argument_int(u32 index, i32 value) {
            set_argument(index, Variant(value));
        }

        void set_argument_float(u32 index, f32 value) {
            set_argument(index, Variant(value));
        }

        void set_argument_bool(u32 index, bool value) {
            set_argument(index, Variant(value));
        }

        void set_argument_string(u32 index, const std::string& value) {
            set_argument(index, Variant(value));
        }

        void set_argument_sid(u32 index, StringId value) {
            set_argument(index, Variant(value));
        }

        void set_argument_ptr(u32 index, void* value) {
            set_argument(index, Variant(value));
        }

        void set_argument_process(u32 index, Process* value) {
            set_argument(index, Variant(value));
        }

        void set_argument_event_message(u32 index, EventMessage* value) {
            set_argument(index, Variant(value));
        }

        // Установка локальной переменной с конкретными типами
        void set_local_int(u32 index, i32 value) {
            set_local(index, Variant(value));
        }

        void set_local_float(u32 index, f32 value) {
            set_local(index, Variant(value));
        }

        void set_local_bool(u32 index, bool value) {
            set_local(index, Variant(value));
        }

        void set_local_string(u32 index, const std::string& value) {
            set_local(index, Variant(value));
        }

        void set_local_sid(u32 index, StringId value) {
            set_local(index, Variant(value));
        }

        void set_local_ptr(u32 index, void* value) {
            set_local(index, Variant(value));
        }

        // Установка регистра с конкретными типами
        void set_register_int(u32 index, i32 value) {
            set_register(index, Variant(value));
        }

        void set_register_float(u32 index, f32 value) {
            set_register(index, Variant(value));
        }

        void set_register_bool(u32 index, bool value) {
            set_register(index, Variant(value));
        }

        void set_register_string(u32 index, const std::string& value) {
            set_register(index, Variant(value));
        }

        void set_register_sid(u32 index, StringId value) {
            set_register(index, Variant(value));
        }

        void set_register_ptr(u32 index, void* value) {
            set_register(index, Variant(value));
        }        

        // ------------------------------------------------------------------------
        // Instruction Execution
        // ------------------------------------------------------------------------

        Instruction get_this_instruction() {
            ASSERT_MSG(code_ptr != nullptr, "No code pointer set");
            ASSERT_FORMAT(pc < get_code_size(), "Program counter out of bounds: {}", pc);

            return code_ptr[pc];
        }

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
            // This would need to be stored in the frame or FunctionDesc header
            // For now, we'll assume a large enough buffer
            return 1024; // Temporary - should come from FunctionDesc
        }

        // ------------------------------------------------------------------------
        // Data Access
        // ------------------------------------------------------------------------

        i64 get_static_int(u32 offset) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            // Data is stored as Variants, so we need to extract i32
            return *((i32*)(data_ptr + offset));
        }

        f64 get_static_float(u32 offset) const {
            ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
            return *((float*)(data_ptr + offset));
        }

        void* get_static_pointer(u32 offset) const {
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
                pc, argc, ret_num, (void*)parent.get());
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

    // Исправленные функции
    inline std::shared_ptr<StackFrame> create_stack_frame(
        ScriptLambda* FunctionDesc, 
        std::shared_ptr<StackFrame> parent = nullptr) 
    {
        return std::make_shared<StackFrame>(FunctionDesc, parent);
    }

    // destroy_stack_frame больше не нужна - удаляем или оставляем пустой
    inline void destroy_stack_frame(std::shared_ptr<StackFrame> frame) {
        // Ничего не делаем - shared_ptr сам удалит
        // Или просто удаляем эту функцию
        (void)frame; // Подавляем warning о неиспользуемом параметре
    }

    inline std::shared_ptr<StackFrame> push_stack_frame(
        ScriptLambda* FunctionDesc, 
        std::shared_ptr<StackFrame> parent = nullptr) {
        
        return create_stack_frame(FunctionDesc, parent);
    }

    inline std::shared_ptr<StackFrame> pop_stack_frame(
        std::shared_ptr<StackFrame> frame) {
        
        if (!frame) return nullptr;
        
        // Получаем shared_ptr родителя
        std::shared_ptr<StackFrame> parent = frame->get_parent();
        
        // frame автоматически удалится, когда выйдет из области видимости
        // Не нужно вызывать destroy
        
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

        CatchFrame(ScriptLambda* FunctionDesc,std::shared_ptr<StackFrame> parent = nullptr, StringId tag_name = SID("null"))
            : StackFrame(FunctionDesc, parent, FrameType::CATCH, tag_name) {
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

        ProtectFrame(ScriptLambda* function_desc, std::shared_ptr<StackFrame> parent = nullptr, std::function<void()> cleanup = nullptr)
            : StackFrame(function_desc, parent, FrameType::PROTECT, SID("protected"))
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