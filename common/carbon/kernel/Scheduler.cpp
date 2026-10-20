#include "carbon/kernel/Scheduler.hpp"

#include "carbon/Carbon.hpp"
#include "carbon/file/DCScript.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/kernel/StateFrame.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "util/Log.hpp"

namespace carbon {

    // ========================================================================
    // Lifecycle
    // ========================================================================

    Scheduler::Scheduler(Carbon *carbon) : m_carbon(carbon) { lg::debug("Scheduler: constructed"); }

    Scheduler::~Scheduler() {
        // m_all owns the processes; unique_ptr handles the rest.
        m_ready.clear();
        m_suspended.clear();
        m_by_pid.clear();
        m_all.clear();
        lg::debug("Scheduler: destroyed");
    }

    // ========================================================================
    // Spawn
    // ========================================================================

    u64 Scheduler::spawn(const std::string &entry, const std::vector<Variant> &args) {
        // 1. Resolve the StateScript from Globals.
        StateScript *state_script = resolve_entry(entry);
        if (!state_script) {
            lg::error("Scheduler::spawn: '{}' is not a valid StateScript entry", entry);
            return 0;
        }

        // 2. Create the process.
        auto process = std::make_unique<Process>();
        process->pid = m_next_pid++;
        process->name = StringId(entry);
        process->type = state_script;

        // 3. Resolve the initial state.
        SsState *initial = state_script->resolve_state(StringId(state_script->m_initialStateId));
        if (!initial) {
            lg::error("Scheduler::spawn: initial state of '{}' not found", entry);
            return 0;
        }
        process->current_state = initial;

        // 4. Copy the state hooks.
        process->trans_hook = initial->get_trans_function();
        process->post_hook = initial->get_post_function();

        // 5. Push the StateFrame for the initial state.
        //
        //    StateFrame is a ProtectFrame subclass: destroying it calls
        //    the state's on-exit handler. The Process stack owns the frame.
        auto state_frame = std::make_shared<StateFrame>(initial, nullptr);
        process->push_frame(state_frame);

        // 6. Run the on-enter handler, if any.
        //
        //    This must run under the scheduler's current_process pointer,
        //    so that any native called from it sees the right process.
        Process *raw = process.get();
        m_carbon->set_current_process(raw);

        if (auto *enter = initial->get_enter_function()) {
            auto &vm = *m_carbon->vm();
            auto  enter_frame = std::make_shared<StackFrame>(
                enter, nullptr, StackFrame::FrameType::GENERIC, StringIds::enter);
            // Pass self as argument 0, so the script can access the process.
            enter_frame->set_argument(0, Variant(static_cast<void *>(raw)));
            enter_frame->argc = 1;
            vm.execute(enter_frame);
        }

        m_carbon->set_current_process(nullptr);

        // 7. Create the main thread from the state's code function.
        if (auto *code = initial->get_code_function()) {
            process->main_thread = std::make_shared<StackFrame>(
                code, nullptr, StackFrame::FrameType::GENERIC, StringIds::code);
            process->main_thread->argc = static_cast<u32>(args.size());
            for (std::size_t i = 0; i < args.size(); ++i) {
                process->main_thread->get_register(ARG_REGISTERS_OFFSET + i) = args[i];
            }
            process->top_thread = process->main_thread;
        }

        // 8. Mark the process ready.
        process->status = ProcessStatus::READY;

        // 9. Register and queue.
        const u64 pid = process->pid;
        m_by_pid[pid] = raw;
        m_ready.push_back(raw);
        m_all.push_back(std::move(process));

        lg::info("Scheduler: spawned '{}' (pid={}, state={})", entry, pid, initial->name());

        return pid;
    }

    // ========================================================================
    // Kill
    // ========================================================================

    void Scheduler::kill(u64 pid) {
        Process *p = find(pid);
        if (!p) {
            lg::warn("Scheduler::kill: pid {} not found", pid);
            return;
        }
        kill_recursive(p);
    }

    void Scheduler::kill_recursive(Process *p) {
        if (!p || p->is_dead()) return;

        // Detach from every list first, so the process cannot be ticked
        // again while we tear it down.
        detach(p);

        // Kill children depth-first.
        while (p->child) {
            Process *child = p->child;
            p->remove_child(child);
            kill_recursive(child);
        }

        // Run the exit handler of the current state (via StateFrame).
        // push_frame stored the frame; pop_frame calls exit().
        while (p->stack_frame_top) { p->pop_frame(); }

        // Mark dead and drop from lookup.
        p->status = ProcessStatus::DEAD;
        m_by_pid.erase(p->pid);

        // Remove from m_all (this is what actually deletes the Process).
        for (auto it = m_all.begin(); it != m_all.end(); ++it) {
            if (it->get() == p) {
                m_all.erase(it);
                break;
            }
        }

        lg::info("Scheduler: killed pid {}", p->pid);
    }

    void Scheduler::detach(Process *p) {
        if (!p) return;

        // Remove from ready queue.
        for (auto it = m_ready.begin(); it != m_ready.end(); ++it) {
            if (*it == p) {
                m_ready.erase(it);
                break;
            }
        }

        // Remove from suspended list.
        for (auto it = m_suspended.begin(); it != m_suspended.end(); ++it) {
            if (*it == p) {
                m_suspended.erase(it);
                break;
            }
        }
    }

    // ========================================================================
    // Tick
    // ========================================================================

    void Scheduler::tick() {
        // 1. Wake any process whose sleep timer expired.
        tick_sleepers();

        // 2. Snapshot the current ready queue. Processes may be spawned
        //    or killed during the tick; the snapshot guarantees that
        //    each process is visited exactly once per tick.
        const std::size_t count = m_ready.size();
        if (count == 0) return;

        std::vector<Process *> this_tick(m_ready.begin(), m_ready.begin() + count);
        m_ready.erase(m_ready.begin(), m_ready.begin() + count);

        for (Process *p : this_tick) {
            if (!p || p->is_dead()) continue;

            m_current = p;
            m_carbon->set_current_process(p);

            p->execute_quantum();

            m_carbon->set_current_process(nullptr);
            m_current = nullptr;

            if (p->is_dead()) {
                // Process is done. Remove from m_all and lookup.
                m_by_pid.erase(p->pid);
                for (auto it = m_all.begin(); it != m_all.end(); ++it) {
                    if (it->get() == p) {
                        m_all.erase(it);
                        break;
                    }
                }
            } else if (p->status == ProcessStatus::SUSPENDED) {
                // Waiting for an external wake-up.
                m_suspended.push_back(p);
            } else if (p->status == ProcessStatus::READY) {
                // Still running. Put it back for the next tick.
                m_ready.push_back(p);
            }
        }
    }

    void Scheduler::tick_sleepers() {
        for (auto it = m_suspended.begin(); it != m_suspended.end();) {
            Process *p = *it;
            if (p && !p->is_dead() && p->sleep_frames > 0) {
                if (--p->sleep_frames == 0) {
                    p->status = ProcessStatus::READY;
                    m_ready.push_back(p);
                    it = m_suspended.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    bool Scheduler::resume(u64 pid) {
        Process *p = find(pid);
        if (!p) return false;
        if (p->status != ProcessStatus::SUSPENDED) return false;

        // Remove from suspended list, push into ready.
        for (auto it = m_suspended.begin(); it != m_suspended.end(); ++it) {
            if (*it == p) {
                m_suspended.erase(it);
                break;
            }
        }
        p->status = ProcessStatus::READY;
        m_ready.push_back(p);
        return true;
    }

    // ========================================================================
    // Lookup
    // ========================================================================

    Process *Scheduler::find(u64 pid) const noexcept {
        auto it = m_by_pid.find(pid);
        return it != m_by_pid.end() ? it->second : nullptr;
    }

    bool Scheduler::empty() const noexcept {
        return m_ready.empty() && m_suspended.empty() && m_all.empty();
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    StateScript *Scheduler::resolve_entry(const std::string &entry) const {
        void *sym = m_carbon->globals()->find_symbol_ptr(StringId(entry));
        if (!sym) return nullptr;

        // The symbol must be a StateScript. We cannot verify the type
        // here without the TypeRegistry lookup, so we trust the loader:
        // only StateScript entries are registered under their name.
        return reinterpret_cast<StateScript *>(sym);
    }

    void Scheduler::dump() const {
        lg::info("=== Scheduler dump ===");
        lg::info("  ready:     {}", m_ready.size());
        lg::info("  suspended: {}", m_suspended.size());
        lg::info("  all:       {}", m_all.size());
        for (const auto &p : m_all) {
            lg::info("  pid={} name='{}' status={}", p->pid, p->name.to_string(),
                     static_cast<int>(p->status));
        }
    }

} // namespace carbon