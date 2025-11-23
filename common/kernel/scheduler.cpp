#include "scheduler.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <algorithm>

namespace vm {

    Scheduler& Scheduler::get_instance() {
        static Scheduler instance;
        return instance;
    }

    // ------------------------------------------------------------------------
    // Process Management
    // ------------------------------------------------------------------------

    Process* Scheduler::create_process(const std::string& name, void* self_object) {
        u32 pid = generate_pid();
        auto process = std::make_unique<Process>(pid, name);

        // Set self pointer according to our basis
        if (self_object) {
            process->set_self(self_object);
        }

        Process* process_ptr = process.get();

        // Store process
        processes_.push_back(std::move(process));
        pid_to_process_[pid] = process_ptr;
        name_to_process_[name] = process_ptr;

        lg::info("Scheduler created process: {} (PID: {})", name, pid);
        return process_ptr;
    }

    void Scheduler::destroy_process(Process* process) {
        if (!process) return;

        // Remove from all queues and maps
        pid_to_process_.erase(process->get_pid());
        name_to_process_.erase(process->get_name());

        // Remove from ready queues
        for (auto& queue : ready_queues_) {
            std::queue<Process*> new_queue;
            while (!queue.empty()) {
                Process* p = queue.front();
                queue.pop();
                if (p != process) {
                    new_queue.push(p);
                }
            }
            queue = new_queue;
        }

        // Remove from suspended/waiting
        auto remove_from_vector = [process](auto& vec) {
            vec.erase(std::remove(vec.begin(), vec.end(), process), vec.end());
            };
        remove_from_vector(suspended_processes_);
        remove_from_vector(waiting_processes_);

        // Terminate and remove from main list
        process->terminate();
        processes_.erase(
            std::remove_if(processes_.begin(), processes_.end(),
                [process](const auto& p) { return p.get() == process; }),
            processes_.end()
        );

        lg::info("Scheduler destroyed process: {}", process->get_name());
    }

    Process* Scheduler::find_process(u32 pid) const {
        auto it = pid_to_process_.find(pid);
        return it != pid_to_process_.end() ? it->second : nullptr;
    }

    Process* Scheduler::find_process(const std::string& name) const {
        auto it = name_to_process_.find(name);
        return it != name_to_process_.end() ? it->second : nullptr;
    }

    // ------------------------------------------------------------------------
    // Execution Control
    // ------------------------------------------------------------------------

    void Scheduler::schedule() {
        frame_counter_++;
        lg::debug("Scheduler frame {}", frame_counter_);

        // Clean up terminated processes
        cleanup_terminated_processes();

        // Schedule processes until all are done or quantum exhausted
        u32 processes_executed = 0;
        const u32 max_processes_per_frame = 100; // Prevent starvation

        while (processes_executed < max_processes_per_frame) {
            Process* next_process = get_next_process();
            if (!next_process) {
                break; // No more processes to run
            }

            current_process_ = next_process;

            // Execute one quantum
            bool continue_running = next_process->execute_quantum();
            processes_executed++;

            // Handle process state after execution
            if (continue_running) {
                // Process wants to continue - put back in ready queue
                add_to_ready_queue(next_process);
            }
            else {
                // Process terminated or suspended
                if (next_process->get_state() == ProcessState::SUSPENDED) {
                    suspended_processes_.push_back(next_process);
                }
                // Terminated processes will be cleaned up next frame
            }

            current_process_ = nullptr;
        }

        if (processes_executed > 0) {
            lg::debug("Scheduler executed {} processes this frame", processes_executed);
        }
    }

    void Scheduler::suspend_process(Process* process) {
        if (!process) return;

        process->suspend();

        // Remove from ready queues if present
        for (auto& queue : ready_queues_) {
            std::queue<Process*> new_queue;
            while (!queue.empty()) {
                Process* p = queue.front();
                queue.pop();
                if (p != process) {
                    new_queue.push(p);
                }
            }
            queue = new_queue;
        }

        // Add to suspended list if not already there
        if (std::find(suspended_processes_.begin(), suspended_processes_.end(), process) == suspended_processes_.end()) {
            suspended_processes_.push_back(process);
        }

        lg::debug("Scheduler suspended process: {}", process->get_name());
    }

    void Scheduler::resume_process(Process* process) {
        if (!process) return;

        process->resume();

        // Remove from suspended list
        suspended_processes_.erase(
            std::remove(suspended_processes_.begin(), suspended_processes_.end(), process),
            suspended_processes_.end()
        );

        // Add back to ready queue
        add_to_ready_queue(process);

        lg::debug("Scheduler resumed process: {}", process->get_name());
    }

    void Scheduler::terminate_process(Process* process) {
        if (!process) return;

        process->terminate();
        lg::debug("Scheduler terminated process: {}", process->get_name());
    }

    // ------------------------------------------------------------------------
    // Process Configuration
    // ------------------------------------------------------------------------

    void Scheduler::set_process_priority(Process* process, ProcessPriority priority) {
        if (!process) return;

        // Implementation would track priority per process
        // For now, just log the request
        lg::debug("Scheduler set priority for {}: {}",
            process->get_name(), static_cast<int>(priority));
    }

    void Scheduler::set_process_quantum(Process* process, u32 quantum) {
        if (!process) return;

        // Quantum would be stored per process in real implementation
        lg::debug("Scheduler set quantum for {}: {}", process->get_name(), quantum);
    }

    // ------------------------------------------------------------------------
    // System Calls
    // ------------------------------------------------------------------------

    void Scheduler::yield() {
        Process* current = Process::get_current_process();
        if (current) {
            // Current process voluntarily yields - will be rescheduled
            lg::debug("Process {} yielded", current->get_name());
        }
    }

    void Scheduler::sleep(u32 frames) {
        Process* current = Process::get_current_process();
        if (current) {
            // Simplified sleep - just suspend for now
            // Real implementation would track sleep duration
            current->suspend();
            get_instance().suspended_processes_.push_back(current);
            lg::debug("Process {} sleeping for {} frames", current->get_name(), frames);
        }
    }

    // ------------------------------------------------------------------------
    // Statistics
    // ------------------------------------------------------------------------

    u32 Scheduler::get_running_count() const {
        u32 count = 0;
        for (const auto& queue : ready_queues_) {
            count += queue.size();
        }
        return count;
    }

    u32 Scheduler::get_total_count() const {
        return processes_.size();
    }

    u32 Scheduler::get_suspended_count() const {
        return suspended_processes_.size();
    }

    // ------------------------------------------------------------------------
    // Internal Methods
    // ------------------------------------------------------------------------

    u32 Scheduler::generate_pid() {
        return next_pid_++;
    }

    void Scheduler::add_to_ready_queue(Process* process) {
        if (!process) return;

        // Simplified - all processes go to NORMAL priority for now
        ready_queues_[static_cast<size_t>(ProcessPriority::NORMAL)].push(process);
    }

    Process* Scheduler::get_next_process() {
        // Round-robin through priority queues
        for (auto& queue : ready_queues_) {
            if (!queue.empty()) {
                Process* next = queue.front();
                queue.pop();
                return next;
            }
        }
        return nullptr;
    }

    void Scheduler::cleanup_terminated_processes() {
        auto it = processes_.begin();
        while (it != processes_.end()) {
            if ((*it)->get_state() == ProcessState::TERMINATED) {
                lg::debug("Scheduler cleaning up terminated process: {}", (*it)->get_name());
                pid_to_process_.erase((*it)->get_pid());
                name_to_process_.erase((*it)->get_name());
                it = processes_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Debugging
    // ------------------------------------------------------------------------

    void Scheduler::dump_process_list() const {
        lg::info("=== Scheduler Process List ===");
        lg::info("Total processes: {}", processes_.size());
        lg::info("Ready: {}, Suspended: {}, Terminated: {}",
            get_running_count(), get_suspended_count(),
            processes_.size() - get_running_count() - get_suspended_count());

        for (const auto& process : processes_) {
            lg::info("  {} - {}", process->to_string(),
                string_id::to_string(process->get_state()));
        }
    }

    std::string Scheduler::to_string() const {
        return fmt::format("Scheduler(Processes:{}, Ready:{}, Suspended:{})",
            processes_.size(), get_running_count(), get_suspended_count());
    }

} // namespace vm