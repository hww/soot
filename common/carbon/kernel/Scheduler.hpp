#pragma once

#include "CommonTypes.hpp"
#include "carbon/ForwardDeclarations.hpp"
#include "carbon/kernel/Process.hpp"

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace carbon {
    
    class Carbon;

    /// @brief Round-robin scheduler for Process instances.
    ///
    /// Owns every Process object. A process is created by spawn(),
    /// advanced by tick(), and destroyed by kill() (or by returning
    /// from its entry function).
    ///
    /// The scheduler keeps three lists:
    ///   - m_ready      : processes ready to run this tick
    ///   - m_suspended  : processes waiting for an external wake-up
    ///                    (animation end, timer, event, ...)
    ///   - m_all        : every live process, for lookup and shutdown
    ///
    /// A suspended process is NOT in m_ready. When something calls
    /// Process::WakeUp() (or resume(pid)), the scheduler moves it
    /// back into m_ready at the next tick.
    class Scheduler {
    public:
        explicit Scheduler(Carbon *carbon);
        ~Scheduler();

        Scheduler(const Scheduler &) = delete;
        Scheduler &operator=(const Scheduler &) = delete;

        // -------------------------------------------------------------------
        // Lifecycle
        // -------------------------------------------------------------------

        /// @brief Create a process that starts executing `entry`.
        ///
        /// Looks up `entry` in Carbon's Globals, resolves the initial
        /// state, creates a Process, pushes a StateFrame for it, runs
        /// the on-enter handler, and queues the process for the next
        /// tick.
        ///
        /// @return pid on success, 0 on failure.
        u64 spawn(const std::string &entry, const std::vector<Variant> &args = {});

        /// @brief Kill a process and all its children.
        ///
        /// Runs the exit handler of the current state, tears down the
        /// process tree, and removes the process from every scheduler
        /// list.
        void kill(u64 pid);

        /// @brief Advance every ready process by one quantum.
        ///
        /// One call == one "frame". Processes that yield go back to
        /// ready; processes that suspend go to suspended; processes
        /// that finish are removed.
        void tick();

        // -------------------------------------------------------------------
        // Wake-up
        // -------------------------------------------------------------------

        /// @brief Move a suspended process back to the ready queue.
        /// @return true if the process was found and was suspended.
        bool resume(u64 pid);

        /// @brief Wake every suspended process whose sleep timer has
        ///        expired. Called automatically at the start of tick().
        void tick_sleepers();

        // -------------------------------------------------------------------
        // Lookup
        // -------------------------------------------------------------------

        [[nodiscard]] Process    *find(u64 pid) const noexcept;
        [[nodiscard]] bool        empty() const noexcept;
        [[nodiscard]] std::size_t live_count() const noexcept { return m_all.size(); }

        // -------------------------------------------------------------------
        // Diagnostics
        // -------------------------------------------------------------------

        void dump() const;

    private:
        // -------------------------------------------------------------------
        // Helpers
        // -------------------------------------------------------------------

        /// @brief Resolve a StateScript* from a logical entry name.
        /// @return nullptr if the name is not registered in Globals, or
        ///         if the entry is not a StateScript.
        StateScript *resolve_entry(const std::string &entry) const;

        /// @brief Remove a process from every scheduler list (does not
        ///        delete it; m_all owns it).
        void detach(Process *p);

        /// @brief Recursively kill a process and its children.
        void kill_recursive(Process *p);

        // -------------------------------------------------------------------
        // State
        // -------------------------------------------------------------------

        Carbon *m_carbon = nullptr;

        std::deque<Process *>                 m_ready;     ///< round-robin queue
        std::vector<Process *>                m_suspended; ///< waiting for wake-up
        std::vector<std::unique_ptr<Process>> m_all;       ///< owner of every process
        std::unordered_map<u64, Process *>    m_by_pid;    ///< pid -> Process*

        Process *m_current = nullptr; ///< process currently executing (if any)

        u64 m_next_pid = 1; ///< pid generator
    };

} // namespace carbon