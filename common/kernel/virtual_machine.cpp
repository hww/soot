#include "virtual_machine.hpp"
#include "util/assert.h"
#include "util/log.h"

namespace vm {

    VirtualMachine& VirtualMachine::get_instance() {
        static VirtualMachine instance;
        return instance;
    }

    VirtualMachine::VirtualMachine()
        : scheduler_(Scheduler::get_instance()) {
        initialize_native_functions();
        lg::info("VirtualMachine initialized with process system");
    }

    VirtualMachine::~VirtualMachine() {
        cleanup();
        lg::info("VirtualMachine shutdown");
    }

    // ------------------------------------------------------------------------
    // Process Management (НОВОЕ - согласно нашему базису)
    // ------------------------------------------------------------------------

    Process* VirtualMachine::create_process(const std::string& name, void* self_object) {
        Process* process = scheduler_.create_process(name, self_object);

        // Автоматически загружаем основной байткод для процесса, если найден
        ByteCode* main_code = find_function(string_id::register_string(name + "-main"));
        if (!main_code) {
            main_code = find_function(string_id::register_string(name));
        }

        if (main_code) {
            process->push_main_frame(main_code);
            lg::debug("VM: Auto-loaded main code for process {}", name);
        }
        else {
            lg::warn("VM: No main code found for process {}", name);
        }

        return process;
    }

    void VirtualMachine::destroy_process(Process* process) {
        scheduler_.destroy_process(process);
    }

    Process* VirtualMachine::get_process(u32 pid) const {
        return scheduler_.find_process(pid);
    }

    Process* VirtualMachine::get_process(const std::string& name) const {
        return scheduler_.find_process(name);
    }

    // ------------------------------------------------------------------------
    // Execution Control (ОБНОВЛЕНО для процессной системы)
    // ------------------------------------------------------------------------

    void VirtualMachine::execute_frame() {
        // Выполняем один кадр планировщика
        scheduler_.schedule();

        // Можно добавить статистику здесь
        static u32 frame_count = 0;
        frame_count++;

        if (frame_count % 100 == 0) {
            lg::debug("VM frame {} - Processes: total={}, ready={}",
                frame_count, scheduler_.get_total_count(), scheduler_.get_running_count());
        }
    }

    Variant VirtualMachine::execute_function(StringId function_name) {
        ByteCode* bytecode = find_function(function_name);
        if (!bytecode) {
            lg::error("Function not found: {}", string_id::to_string(function_name));
            return Variant();
        }

        return execute_bytecode(bytecode);
    }

    Variant VirtualMachine::execute_function(const std::string& function_name) {
        return execute_function(string_id::register_string(function_name));
    }

    Variant VirtualMachine::execute_bytecode(ByteCode* bytecode) {
        if (!bytecode) {
            lg::error("Cannot execute null bytecode");
            return Variant();
        }

        // Создаем временный процесс для выполнения
        auto temp_process = std::make_unique<Process>(0, "temp-bytecode-execution");
        temp_process->push_main_frame(bytecode);

        // Выполняем до завершения
        while (temp_process->execute_quantum()) {
            // Продолжаем выполнение
        }

        // Возвращаем результат (упрощенно)
        return Variant();
    }

    // ------------------------------------------------------------------------
    // Instruction Execution (ОБНОВЛЕНО для работы в контексте процесса)
    // ------------------------------------------------------------------------

    void VirtualMachine::execute_instruction(StackFrame& frame, const Instruction& instr) {
        // Устанавливаем контекст процесса для нативных вызовов
        Process::set_current_process(frame.owner_process);

        try {
            switch (instr.opcode) {
                // Control Flow
            case Opcode::RETURN:
            case Opcode::MOVE:
            case Opcode::CALL:
            case Opcode::CALL_NATIVE:
            case Opcode::BRANCH:
            case Opcode::BRANCH_IF:
            case Opcode::BRANCH_IF_NOT:
                execute_control_flow(frame, instr);
                break;

                // Arithmetic
            case Opcode::ADD_INT:
            case Opcode::SUB_INT:
            case Opcode::MUL_INT:
            case Opcode::DIV_INT:
            case Opcode::MOD_INT:
            case Opcode::ABS_INT:
            case Opcode::NEG_INT:
            case Opcode::ASH_INT:
            case Opcode::TO_INT:
            case Opcode::ADD_IMM:
            case Opcode::SUB_IMM:
            case Opcode::MUL_IMM:
            case Opcode::DIV_IMM:
            case Opcode::ADD_FLOAT:
            case Opcode::SUB_FLOAT:
            case Opcode::MUL_FLOAT:
            case Opcode::DIV_FLOAT:
            case Opcode::MOD_FLOAT:
            case Opcode::ABS_FLOAT:
            case Opcode::NEG_FLOAT:
            case Opcode::TO_FLOAT:
                execute_arithmetic(frame, instr);
                break;

                // Comparison
            case Opcode::CMP_EQUAL:
            case Opcode::CMP_NOT_EQUAL:
            case Opcode::CMP_GT:
            case Opcode::CMP_GT_EQUAL:
            case Opcode::CMP_LT:
            case Opcode::CMP_LT_EQUAL:
            case Opcode::CMP_FLOAT_EQUAL:
            case Opcode::CMP_FLOAT_NOT_EQUAL:
            case Opcode::CMP_FLOAT_GT:
            case Opcode::CMP_FLOAT_GT_EQUAL:
            case Opcode::CMP_FLOAT_LT:
            case Opcode::CMP_FLOAT_LT_EQUAL:
                execute_comparison(frame, instr);
                break;

                // Lookup
            case Opcode::LOOKUP_INT:
            case Opcode::LOOKUP_FLOAT:
            case Opcode::LOOKUP_POINTER:
                execute_lookup(frame, instr);
                break;

                // Memory Access
            case Opcode::LOAD_IND_INT:
            case Opcode::LOAD_IND_FLOAT:
            case Opcode::LOAD_IND_POINTER:
            case Opcode::STORE_IND_INT:
            case Opcode::STORE_IND_FLOAT:
            case Opcode::STORE_IND_POINTER:
            case Opcode::LOAD_STATIC_INT:
            case Opcode::LOAD_STATIC_FLOAT:
            case Opcode::LOAD_STATIC_POINTER:
                execute_memory_access(frame, instr);
                break;

                // Logical
            case Opcode::LOG_AND:
            case Opcode::LOG_OR:
            case Opcode::LOG_NOT:
                // ... implementation
                break;

                // Bitwise
            case Opcode::BIT_AND:
            case Opcode::BIT_OR:
            case Opcode::BIT_XOR:
            case Opcode::BIT_NOR:
            case Opcode::BIT_NOT:
                // ... implementation  
                break;

            default:
                lg::error("Unknown opcode: {} (0x{:02X})",
                    static_cast<u32>(instr.opcode), static_cast<u32>(instr.opcode));
                break;
            }
        }
        catch (const std::exception& e) {
            lg::error("Instruction execution error: {} at PC: {}", e.what(), frame.pc);
            throw;
        }

        // Сбрасываем контекст процесса
        Process::set_current_process(nullptr);
    }

    // ------------------------------------------------------------------------
    // Instruction Execution Helpers (выборочная реализация)
    // ------------------------------------------------------------------------

    void VirtualMachine::execute_control_flow(StackFrame& frame, const Instruction& instr) {
        switch (instr.opcode) {
        case Opcode::RETURN: {
            Variant return_value = frame.get_register(instr.a);
            // Обработка возврата - будет обработана на уровне процесса
            break;
        }

        case Opcode::MOVE: {
            Variant& dest = frame.get_register(instr.a);
            Variant& src = frame.get_register(instr.b);
            dest = src;
            break;
        }

        case Opcode::CALL_NATIVE: {
            // Нативные вызовы обрабатываются на уровне процесса
            // VM только предоставляет доступ к функциям
            break;
        }

        case Opcode::BRANCH: {
            frame.jump_to(instr.imm16);
            break;
        }

        case Opcode::BRANCH_IF: {
            Variant& condition = frame.get_register(instr.a);
            if (condition.to_bool()) {
                frame.jump_to(instr.imm16);
            }
            break;
        }

        default:
            lg::warn("Unimplemented control flow instruction: {}", static_cast<u32>(instr.opcode));
            break;
        }
    }

    void VirtualMachine::execute_arithmetic(StackFrame& frame, const Instruction& instr) {
        switch (instr.opcode) {
        case Opcode::ADD_INT: {
            Variant& dest = frame.get_register(instr.a);
            Variant& src1 = frame.get_register(instr.b);
            Variant& src2 = frame.get_register(instr.c);
            dest = Variant(src1.to_int() + src2.to_int());
            break;
        }

        case Opcode::SUB_INT: {
            Variant& dest = frame.get_register(instr.a);
            Variant& src1 = frame.get_register(instr.b);
            Variant& src2 = frame.get_register(instr.c);
            dest = Variant(src1.to_int() - src2.to_int());
            break;
        }

        case Opcode::MUL_INT: {
            Variant& dest = frame.get_register(instr.a);
            Variant& src1 = frame.get_register(instr.b);
            Variant& src2 = frame.get_register(instr.c);
            dest = Variant(src1.to_int() * src2.to_int());
            break;
        }

        case Opcode::ADD_FLOAT: {
            Variant& dest = frame.get_register(instr.a);
            Variant& src1 = frame.get_register(instr.b);
            Variant& src2 = frame.get_register(instr.c);
            dest = Variant(src1.to_float() + src2.to_float());
            break;
        }

                              // ... другие арифметические операции

        default:
            lg::warn("Unimplemented arithmetic instruction: {}", static_cast<u32>(instr.opcode));
            break;
        }
    }

    void VirtualMachine::execute_lookup(StackFrame& frame, const Instruction& instr) {
        // LOOKUP инструкции теперь работают через контекст процесса
        Process* process = frame.owner_process;
        if (!process) {
            lg::error("LOOKUP instruction without process context");
            return;
        }

        Variant& dest = frame.get_register(instr.a);
        StringId name_id = instr.imm16;

        Variant* value = process->lookup_field(name_id);
        if (value) {
            dest = *value;
        }
        else {
            dest.set_null();
            lg::debug("LOOKUP failed for: {}", string_id::to_string(name_id));
        }
    }

    void VirtualMachine::execute_comparison(StackFrame& frame, const Instruction& instr) {
        // ... реализация сравнений
    }

    void VirtualMachine::execute_memory_access(StackFrame& frame, const Instruction& instr) {
        // ... реализация доступа к памяти
    }

    // ------------------------------------------------------------------------
    // Native Function Management
    // ------------------------------------------------------------------------

    NativeFunction VirtualMachine::find_native_function(StringId name) const {
        auto it = native_functions_.find(name);
        return it != native_functions_.end() ? it->second : nullptr;
    }

    void VirtualMachine::initialize_native_functions() {
        // Базовые нативные функции
        native_functions_[SID("print")] = [](u32 argc, const Variant* argv) -> Variant {
            for (u32 i = 0; i < argc; i++) {
                lg::print("{} ", argv[i].to_string());
            }
            return Variant(true);
            };

        native_functions_[SID("println")] = [](u32 argc, const Variant* argv) -> Variant {
            for (u32 i = 0; i < argc; i++) {
                lg::print("{} ", argv[i].to_string());
            }
            lg::print("\n");
            return Variant(true);
            };

        // Системные вызовы будут обрабатываться на уровне процесса
        register_system_calls();

        lg::info("VM initialized {} native functions", native_functions_.size());
    }

    void VirtualMachine::register_system_calls() {
        // Системные вызовы регистрируются, но обрабатываются Process::handle_system_call
        native_functions_[SID("sleep")] = nullptr;
        native_functions_[SID("kill")] = nullptr;
        native_functions_[SID("go")] = nullptr;
        native_functions_[SID("send-event")] = nullptr;
    }

    // ------------------------------------------------------------------------
    // Internal Helpers
    // ------------------------------------------------------------------------

    ByteCode* VirtualMachine::find_function(StringId name) {
        for (auto& binary_ptr : binaries_) {
            auto header = binary_ptr->get_header();
            for (u32 i = 0; i < header->defs_num; i++) {
                auto def = header->get_definition(i);
                if (def->name == name) {
                    return header->get_definition_ptr<ByteCode>(i);
                }
            }
        }
        return nullptr;
    }

    void VirtualMachine::cleanup() {
        binaries_.clear();
        native_functions_.clear();
        lg::info("VM cleanup completed");
    }

    // ------------------------------------------------------------------------
    // Utility Methods
    // ------------------------------------------------------------------------

    void VirtualMachine::dump_state() const {
        lg::info("=== VM State ===");
        lg::info("Loaded Binaries: {}", binaries_.size());
        lg::info("Native Functions: {}", native_functions_.size());
        scheduler_.dump_process_list();
    }

    std::string VirtualMachine::to_string() const {
        return fmt::format("VirtualMachine(binaries:{}, natives:{}, processes:{})",
            binaries_.size(), native_functions_.size(), scheduler_.get_total_count());
    }

} // namespace vm