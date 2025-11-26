#include "common/runtime/kernel/Process.hpp"
#include "common/runtime/kernel/ProcessScheduler.hpp"
#include "common/runtime/vm/VirtualMachine.hpp"
#include "common/util/Log.hpp"
#include <algorithm>

namespace vm {

    Scheduler::Scheduler() {
        // Создаем корневой узел
        root = create_process(string_id::register_string("root"));
        root->add_mask(ProcessMask::PROCESS_TREE);
    }

    Process* Scheduler::create_process(StringId name) {
        u32 pid = generate_pid();
        auto process = std::make_unique<Process>(pid, name);
        Process* process_ptr = process.get();

        all_processes_.push_back(std::move(process));
        pid_to_process_[pid] = process_ptr;
        name_to_process_[name] = process_ptr;

        return process_ptr;
    }

    void Scheduler::destroy_process(Process* process) {
        if (!process) return;

        // Удаляем из карт
        pid_to_process_.erase(process->pid);
        name_to_process_.erase(process->name);

        // Удаляем из дерева
        remove_from_tree(process);

        // Удаляем из основного хранилища
        auto it = std::find_if(all_processes_.begin(), all_processes_.end(),
            [process](const auto& p) { return p.get() == process; });

        if (it != all_processes_.end()) {
            all_processes_.erase(it);
        }
    }

    void Scheduler::schedule() {
        VirtualMachine& vm = VirtualMachine::get_instance();

        // Очистка мертвых процессов
        cleanup_dead_processes();

        // Обход дерева и выполнение процессов
        iterate_tree(root, [&](Process* proc) {
            // Проверка глобальной маски
            if (has_mask(global_mask_, ProcessMask::EXECUTE) &&
                has_mask(proc->mask, ProcessMask::EXECUTE)) {
                return true; // Пропускаем процессы с EXECUTE маской
            }

            // Проверка готовности к выполнению
            if (proc->is_runnable() && !proc->has_mask(ProcessMask::SLEEP)) {
                vm.execute_process(proc);
            }

            return true;
            });
    }

    void Scheduler::add_to_tree(Process* process, Process* parent) {
        if (!process || !parent) return;

        // Удаляем из текущего родителя
        remove_from_tree(process);

        // Добавляем к новому родителю
        process->parent = parent;
        process->brother = parent->child;
        parent->child = process;
    }

    void Scheduler::remove_from_tree(Process* process) {
        if (!process || !process->parent) return;

        Process* parent = process->parent;
        Process* prev = nullptr;
        Process* current = parent->child;

        // Ищем процесс в списке детей
        while (current) {
            if (current == process) {
                if (prev) {
                    prev->brother = current->brother;
                }
                else {
                    parent->child = current->brother;
                }
                process->parent = nullptr;
                process->brother = nullptr;
                break;
            }
            prev = current;
            current = current->brother;
        }
    }

    u32 Scheduler::get_active_count() const {
        u32 count = 0;
        for (const auto& process : all_processes_) {
            if (process->is_active()) {
                count++;
            }
        }
        return count;
    }

    u32 Scheduler::get_suspended_count() const {
        u32 count = 0;
        for (const auto& process : all_processes_) {
            if (process->status == ProcessStatus::SUSPENDED) {
                count++;
            }
        }
        return count;
    }

    Process* Scheduler::find_process_by_pid(u32 pid) const {
        auto it = pid_to_process_.find(pid);
        return it != pid_to_process_.end() ? it->second : nullptr;
    }

    Process* Scheduler::find_process_by_name(StringId name) const {
        auto it = name_to_process_.find(name);
        return it != name_to_process_.end() ? it->second : nullptr;
    }

    void Scheduler::cleanup_dead_processes() {
        auto it = all_processes_.begin();
        while (it != all_processes_.end()) {
            if ((*it)->is_dead()) {
                // Уже удалено в destroy_process
                it = all_processes_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    std::string Scheduler::to_string() const {
        return fmt::format("Scheduler(Processes:{}, Active:{}, Suspended:{}, GlobalMask:0x{:08X})",
            get_process_count(), get_active_count(), get_suspended_count(),
            static_cast<u32>(global_mask_));
    }

} // namespace vm