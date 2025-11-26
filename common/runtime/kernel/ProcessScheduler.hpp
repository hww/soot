#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/kernel/Process.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace runtime::kernel {


    class Scheduler {
    public:
        static Scheduler& get_instance() {
            static Scheduler instance;
            return instance;
        }

        // === ПРОЦЕССЫ ===
        Process* root = nullptr;

        Process* create_process(StringId name);
        void destroy_process(Process* process);

        // === ВЫПОЛНЕНИЕ ===
        void schedule();

        // === ДЕРЕВО ===
        void add_to_tree(Process* process, Process* parent);
        void remove_from_tree(Process* process);

        // === МАСКИ ===
        ProcessMask get_global_mask() const { return global_mask_; }
        void set_global_mask(ProcessMask mask) { global_mask_ = mask; }
        void add_global_mask(ProcessMask mask) { global_mask_ = global_mask_ | mask; }
        void remove_global_mask(ProcessMask mask) {
            global_mask_ = static_cast<ProcessMask>(static_cast<u32>(global_mask_) & ~static_cast<u32>(mask));
        }

        // === СТАТИСТИКА ===
        u32 get_process_count() const { return all_processes_.size(); }
        u32 get_active_count() const;
        u32 get_suspended_count() const;

        // === ОБХОД ДЕРЕВА ===
        template<typename Func>
        void iterate_tree(Process* node, Func func) {
            if (!node) return;
            if (!func(node)) return;

            Process* child = node->child;
            while (child) {
                iterate_tree(child, func);
                child = child->brother;
            }
        }

        // Поиск процессов
        Process* find_process_by_pid(u32 pid) const;
        Process* find_process_by_name(StringId name) const;

        std::string to_string() const;

    private:
        Scheduler();

        // Хранилище процессов
        std::vector<std::unique_ptr<Process>> all_processes_;
        std::unordered_map<u32, Process*> pid_to_process_;
        std::unordered_map<StringId, Process*> name_to_process_;

        // Глобальная маска для фильтрации
        ProcessMask global_mask_ = ProcessMask::NONE;

        // Генерация PID
        u32 next_pid_ = 1;
        u32 generate_pid() { return next_pid_++; }

        // Вспомогательные методы
        void cleanup_dead_processes();
    };

} // namespace vm