#pragma once

#include "types.hpp"
#include "process.hpp"
#include <vector>
#include <queue>
#include <memory>
#include <unordered_map>

namespace vm {

    /**
     * @brief Process priorities matching GOAL-like system
     */
    enum class ProcessPriority {
        HIGH = 0,    // System-critical processes
        NORMAL = 1,  // Game logic processes  
        LOW = 2,     // Background processes
        COUNT = 3
    };

    /**
     * @brief Main scheduler managing process execution
     *
     * Follows our basis:
     * - Processes have self pointers
     * - No FSM instructions in VM
     * - Uses existing CALL mechanism for state changes
     */
    class Scheduler {
    public:
        static Scheduler& get_instance();

        // Process management
        Process* create_process(const std::string& name, void* self_object = nullptr);
        void destroy_process(Process* process);
        Process* find_process(u32 pid) const;
        Process* find_process(const std::string& name) const;

        // Execution control
        void schedule();  // Main scheduling loop - call each frame
        void suspend_process(Process* process);
        void resume_process(Process* process);
        void terminate_process(Process* process);

        // Process configuration
        void set_process_priority(Process* process, ProcessPriority priority);
        void set_process_quantum(Process* process, u32 quantum);

        // System calls for processes
        static void yield();  // Voluntary yield
        static void sleep(u32 frames);

        // Statistics
        u32 get_running_count() const;
        u32 get_total_count() const;
        u32 get_suspended_count() const;

        // Debugging
        void dump_process_list() const;
        std::string to_string() const;

    private:
        Scheduler() = default;

        // Process storage
        std::vector<std::unique_ptr<Process>> processes_;
        std::unordered_map<u32, Process*> pid_to_process_;
        std::unordered_map<std::string, Process*> name_to_process_;

        // Priority queues
        std::array<std::queue<Process*>, static_cast<size_t>(ProcessPriority::COUNT)> ready_queues_;
        std::vector<Process*> suspended_processes_;
        std::vector<Process*> waiting_processes_;

        // Scheduling state
        Process* current_process_ = nullptr;
        u32 next_pid_ = 1;
        u32 frame_counter_ = 0;

        // Internal methods
        u32 generate_pid();
        void add_to_ready_queue(Process* process);
        Process* get_next_process();
        void cleanup_terminated_processes();
    };

} // namespace vm