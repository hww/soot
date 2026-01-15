#include "common/carbon/kernel/Scheduler.hpp"
#include "common/util/Log.hpp"
#include <algorithm>
#include <format>

namespace runtime::kernel {

    Scheduler::Scheduler() {
        // Создаем корневой процесс
        root = create_process(SID("root"));
        if (root) {
            root->add_mask(ProcessMask::PROCESS_TREE);
            lg::debug("Scheduler initialized with root process");
        }
    }

    Process* Scheduler::create_process(StringId name) {
        auto process = std::make_unique<Process>(name);
        Process* process_ptr = process.get();

        all_processes_.push_back(std::move(process));
        pid_to_process_[process_ptr->pid] = process_ptr;
        name_to_process_[name] = process_ptr;

        lg::debug("Created process '{}' with PID {}", lib::to_string(name), process->pid);
        return process_ptr;
    }

    void Scheduler::add_to_tree(Process* process, Process* parent) {
        if (!process || !parent) {
            lg::error("Cannot add to tree - invalid process or parent");
            return;
        }

        remove_from_tree_internal(process);

        process->parent = parent;
        process->brother = parent->child;
        parent->child = process;

        lg::debug("Added process '{}' to tree under parent '{}'",
            process->get_name_string(), parent->get_name_string());
    }

    void Scheduler::remove_from_tree(Process* process) {
        if (!process) return;

        remove_from_tree_internal(process);

        if (process != root) {
            auto it = std::find_if(all_processes_.begin(), all_processes_.end(),
                [process](const std::unique_ptr<Process>& p) {
                    return p.get() == process;
                });
            if (it != all_processes_.end()) {
                pid_to_process_.erase(process->pid);
                name_to_process_.erase(process->name);
                all_processes_.erase(it);
            }
        }

        lg::debug("Removed process '{}' from tree", process->get_name_string());
    }

    void Scheduler::cleanup_all_processes() {
        // Удаляем все процессы кроме корневого
        auto it = all_processes_.begin();
        while (it != all_processes_.end()) {
            if ((*it).get() != root) {
                Process* process = (*it).get();
                pid_to_process_.erase(process->pid);
                name_to_process_.erase(process->name);
                it = all_processes_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void Scheduler::reparent_process(Process* process, Process* new_parent) {
        if (!process || !new_parent) {
            lg::error("Cannot reparent process - invalid parameters");
            return;
        }

        if (process == root) {
            lg::error("Cannot reparent root process");
            return;
        }

        remove_from_tree_internal(process);
        add_to_tree(process, new_parent);

        lg::debug("Reparented process '{}' to new parent '{}'",
            process->get_name_string(), new_parent->get_name_string());
    }

    bool Scheduler::iterate_tree(Process* node, TreeVisitor visitor) const{
        if (!node) return true;

        // Вызываем функцию для текущего узла
        if (!visitor(node)) {
            return false; // Обход прерван
        }

        // Рекурсивно обходим детей
        Process* child = node->child;
        while (child) {
            // Сохраняем следующего брата перед рекурсией (на случай изменения дерева)
            Process* next_brother = child->brother;
            if (!iterate_tree(child, visitor)) {
                return false;
            }
            child = next_brother;
        }

        return true;
    }

    Process* Scheduler::find_process_if(ProcessPredicate predicate) const {
        for (const auto& process : all_processes_) {
            if (predicate(process.get())) {
                return process.get();
            }
        }
        return nullptr;
    }

    void Scheduler::remove_from_tree_internal(Process* process) {
        if (!process || !process->parent) return;

        Process* parent = process->parent;
        Process* prev = nullptr;
        Process* current = parent->child;

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

    Process* Scheduler::find_process_by_pid(u32 pid) const {
        auto it = pid_to_process_.find(pid);
        return it != pid_to_process_.end() ? it->second : nullptr;
    }

    Process* Scheduler::find_process_by_name(StringId name) const {
        auto it = name_to_process_.find(name);
        return it != name_to_process_.end() ? it->second : nullptr;
    }

    u32 Scheduler::get_active_count() const {
        u32 count = 0;
        for (const auto& process : all_processes_) {
            if (process->status != ProcessStatus::DEAD) {
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

    u32 Scheduler::get_dead_count() const {
        u32 count = 0;
        for (const auto& process : all_processes_) {
            if (process->status == ProcessStatus::DEAD) {
                count++;
            }
        }
        return count;
    }


    void Scheduler::cleanup_dead_processes() {
        auto it = all_processes_.begin();
        while (it != all_processes_.end()) {
            if ((*it)->status == ProcessStatus::DEAD && (*it).get() != root) {
                Process* process = (*it).get();
                pid_to_process_.erase(process->pid);
                name_to_process_.erase(process->name);
                remove_from_tree_internal(process);
                it = all_processes_.erase(it);
                lg::debug("Cleaned up dead process '{}'", process->get_name_string());
            }
            else {
                ++it;
            }
        }
    }

    std::string Scheduler::to_string() const {
        return std::format("Scheduler(processes:{}, active:{}, suspended:{}, dead:{})",
            get_process_count(), get_active_count(),
            get_suspended_count(), get_dead_count());
    }

    void Scheduler::dump_tree_info() const {
        lg::info("=== Scheduler Tree Info ===");
        lg::info("Total Processes: {}", get_process_count());
        lg::info("Active: {}, Suspended: {}, Dead: {}",
            get_active_count(), get_suspended_count(), get_dead_count());

        lg::info("Process Tree Structure:");
        iterate_tree(root, [](Process* proc) {
            int level = 0;
            Process* p = proc;
            while (p->parent && p->parent->parent) {
                level++;
                p = p->parent;
            }

            std::string indent(level * 2, ' ');
            lg::info("{}{} (PID: {}, Status: {})",
                indent, proc->get_name_string(), proc->pid,
                static_cast<int>(proc->status));
            return true;
            });
    }

} // namespace runtime::kernel