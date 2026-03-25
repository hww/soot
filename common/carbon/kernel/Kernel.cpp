#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/util/Log.hpp"
#include "vm/StackFrame.hpp"
#include <format>
#include <functional> // Добавлен для std::function

using namespace carbon::lib;

namespace carbon::kernel {

    bool Kernel::initialize() {
        if (initialized_) {
            lg::warn("Kernel already initialized");
            return true;
        }

        try {
            lg::info("Initializing Kernel...");

            // Создаем корневой процесс дерева через планировщик
            root_ = scheduler_.create_process(SID("root"));
            if (!root_) {
                lg::error("Failed to create root process");
                return false;
            }

            // Устанавливаем маску PROCESS_TREE для корневого узла
            root_->add_mask(ProcessMask::PROCESS_TREE);

            initialized_ = true;
            lg::info("Kernel initialized successfully");
            return true;
        }
        catch (const std::exception& e) {
            lg::error("Kernel initialization failed: {}", e.what());
            shutdown();
            return false;
        }
    }

    void Kernel::shutdown() {
        if (!initialized_) return;

        lg::info("Shutting down Kernel...");

        // Очищаем текущий контекст
        clear_current_context();

        // Останавливаем все процессы через планировщик
        scheduler_.cleanup_all_processes();

        initialized_ = false;
        lg::info("Kernel shutdown complete");
    }

    Process* Kernel::create_process(StringId name) {
        if (!initialized_) {
            lg::error("Cannot create process - Kernel not initialized");
            return nullptr;
        }

        return scheduler_.create_process(name);
    }

    bool Kernel::activate_process(Process* process, Process* parent, StringId name, std::shared_ptr<StackFrame> stack_top) {
        if (!initialized_ || !process) {
            lg::error("Cannot activate process - invalid parameters");
            return false;
        }

        if (process->status != ProcessStatus::DEAD) {
            lg::error("Cannot activate process '{}' - not in DEAD state",
                process->get_name_string());
            return false;
        }

        // Активируем процесс
        process->activate(parent, name, stack_top);

        // Добавляем в дерево процессов
        Process* actual_parent = parent ? parent : root_;
        scheduler_.add_to_tree(process, actual_parent);

        lg::debug("Activated process '{}'", process->get_name_string());
        return true;
    }

    bool Kernel::run_process_function(Process* process, FunctionDesc* entry_point) {
        if (!initialized_ || !process || !entry_point) {
            lg::error("Cannot run process function - invalid parameters");
            return false;
        }

        // Сохраняем предыдущий контекст
        auto* old_process = current_process_;
        auto old_thread = current_thread_;

        try {
            // Устанавливаем новый контекст
            set_current_context(process, ThreadType::MAIN);

            // Выполняем функцию через виртуальную машину
            virtual_machine_.execute_function(entry_point);

            lg::debug("Executed function in process '{}'", process->get_name_string());

            // Восстанавливаем предыдущий контекст
            set_current_context(old_process, old_thread);
            return true;
        }
        catch (const std::exception& e) {
            lg::error("Failed to run function in process '{}': {}",
                process->get_name_string(), e.what());

            // Восстанавливаем предыдущий контекст даже при ошибке
            set_current_context(old_process, old_thread);
            return false;
        }
    }

    void Kernel::execute_frame() {
        if (!initialized_) {
            lg::error("Cannot execute frame - Kernel not initialized");
            return;
        }

        // Обходим дерево процессов и выполняем готовые к выполнению
        scheduler_.iterate_tree(root_, [this](Process* process) {
            if (should_execute_process(process)) {
                // Сохраняем предыдущий контекст
                auto* old_process = current_process_;
                auto old_thread = current_thread_;

                try {
                    // Устанавливаем контекст текущего процесса
                    set_current_context(process, ThreadType::MAIN);

                    // Выполняем квант времени процесса
                    process->execute_quantum();

                    // Восстанавливаем контекст
                    set_current_context(old_process, old_thread);
                }
                catch (const std::exception& e) {
                    lg::error("Error executing process '{}': {}",
                        process->get_name_string(), e.what());

                    // Восстанавливаем контекст при ошибке
                    set_current_context(old_process, old_thread);
                }
            }
            return true; // Продолжаем обход
            });
    }

    bool Kernel::should_execute_process(Process* process) const {
        if (!process || !process->is_runnable()) {
            return false;
        }

        // Проверяем глобальную маску
        if ((global_mask_ & ProcessMask::EXECUTE) != ProcessMask::NONE) {
            return false;
        }

        // Проверяем маску процесса
        if (process->has_mask(ProcessMask::EXECUTE) ||
            process->has_mask(ProcessMask::SLEEP)) {
            return false;
        }

        return true;
    }

    std::string Kernel::to_string() const {
        return std::format("Kernel(initialized:{}, processes:{}, current_process:{}, global_mask:0x{:08X})",
            initialized_,
            scheduler_.get_process_count(),
            current_process_ ? current_process_->get_name_string() : "null",
            static_cast<u32>(global_mask_));
    }

    void Kernel::dump_debug_info() const {
        lg::info("=== Kernel Debug Info ===");
        lg::info("Initialized: {}", initialized_);
        lg::info("Current Process: {}",
            current_process_ ? current_process_->get_name_string() : "null");
        lg::info("Current Thread: {}", static_cast<int>(current_thread_));
        lg::info("Global Mask: 0x{:08X}", static_cast<u32>(global_mask_));
        lg::info("Total Processes: {}", scheduler_.get_process_count());

        // Выводим дерево процессов
        lg::info("Process Tree:");
        scheduler_.iterate_tree(root_, [](Process* proc) {
            lg::info("  - {} (PID: {}, Status: {}, Mask: 0x{:08X})",
                proc->get_name_string(), proc->pid,
                static_cast<int>(proc->status),
                static_cast<u32>(proc->mask));
            return true;
            });
    }

} // namespace carbon::kernel