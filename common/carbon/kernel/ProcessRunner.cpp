#include "carbon/kernel/ProcessRunner.hpp"
#include "carbon/Carbon.hpp"
#include "carbon/file/DCScript.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/StackFrame.hpp"
#include "carbon/kernel/Process.hpp"

#include "carbon/lib/StringId.hpp"

#include "util/Log.hpp"

#include <memory>

namespace carbon {

    // ============================================================================
    // Internal helpers
    // ============================================================================

    namespace {

        // Return the active Carbon context, or nullptr if none is set.
        // Every public entry point below starts with this check.
        Carbon *current_carbon() {
            Carbon *c = Carbon::current();
            if (!c || !c->is_initialized()) {
                lg::error("ProcessRunner: no active Carbon context");
                return nullptr;
            }
            return c;
        }

    } // namespace

    // ============================================================================
    // One-shot Process Creation
    // ============================================================================

    Process *ProcessRunner::spawn(StringId name, Process *parent, ScriptLambda *entry_point,
                                  std::shared_ptr<StackFrame> stack_top) {
        Carbon *c = current_carbon();
        if (!c) return nullptr;

        // Stage 1: create the process.
        Process *process = nullptr; // c->create_process(name);  // TODO: not in Carbon yet
        if (!process) {
            lg::error("Failed to create process '{}'", name);
            return nullptr;
        }

        // Stage 2: activate the process.
        // if (!c->activate_process(process, parent, name, stack_top)) { ... }

        // Stage 3: run the entry function.
        // if (entry_point && !c->run_process_function(process, entry_point)) { ... }

        lg::debug("Spawned process '{}' with entry point", name);
        return process;
    }

    Process *ProcessRunner::spawn_with_state(StringId name, Process *parent, SsState *initial_state,
                                             std::shared_ptr<StackFrame> stack_top) {
        if (!initial_state) {
            lg::error("Cannot spawn process with state - invalid state definition");
            return nullptr;
        }

        Process *process = spawn(name, parent, nullptr, stack_top);
        if (!process) { return nullptr; }

        // process->go_state(initial_state);

        lg::debug("Spawned process '{}' with initial state '{}'", name, initial_state->name());
        return process;
    }

    // ============================================================================
    // Two-stage Process Creation
    // ============================================================================

    Process *ProcessRunner::create(StringId name) {
        Carbon *c = current_carbon();
        if (!c) return nullptr;

        Process *process = nullptr; // c->create_process(name);
        if (process) { lg::debug("Created process '{}' (stage 1)", name); }
        return process;
    }

    bool ProcessRunner::activate(Process *process, Process *parent, StringId name,
                                 std::shared_ptr<StackFrame> stack_top) {
        Carbon *c = current_carbon();
        if (!c || !process) {
            lg::error("Cannot activate process - invalid process or context");
            return false;
        }

        bool success = false; // c->activate_process(process, parent, name, stack_top);
        if (success) { lg::debug("Activated process '{}' (stage 2)", process->get_name_string()); }
        return success;
    }

    bool ProcessRunner::run(Process *process, ScriptLambda *entry_point) {
        Carbon *c = current_carbon();
        if (!c || !process || !entry_point) {
            lg::error("Cannot run process - invalid parameters");
            return false;
        }

        bool success = false; // c->run_process_function(process, entry_point);
        if (success) { lg::debug("Running function in process '{}'", process->get_name_string()); }
        return success;
    }

    // ============================================================================
    // Pool-based Creation
    // ============================================================================

    Process *ProcessRunner::create_from_pool(StringId name) {
        lg::debug("Creating process '{}' from pool", name);
        return create(name);
    }

    void ProcessRunner::return_to_pool(Process *process) {
        if (!process) return;
        lg::debug("Returning process '{}' to pool", process->get_name_string());
    }

    // ============================================================================
    // Utility Functions
    // ============================================================================

    Process *ProcessRunner::spawn_module_function(StringId module_name, StringId function_name,
                                                  Process                    *parent,
                                                  std::shared_ptr<StackFrame> stack_top) {
        Carbon *c = current_carbon();
        if (!c) return nullptr;

        // Load the module through Carbon (not Globals::inst()).
        const std::string module_path = module_name.to_string() + ".bin";
        if (!c->load_module(module_path)) {
            lg::error("Module '{}' not found", module_name);
            return nullptr;
        }

        // Locate the function in Carbon's Globals.
        Globals *g = c->globals();
        if (!g) {
            lg::error("Carbon has no Globals");
            return nullptr;
        }

        void *fn_ptr = g->find_symbol_ptr(function_name);
        if (!fn_ptr) {
            lg::error("Function '{}' not found in module '{}'", function_name, module_name);
            return nullptr;
        }

        auto *function = reinterpret_cast<ScriptLambda *>(fn_ptr);

        StringId process_name = function_name;

        lg::debug("Spawning process for {}.{}", module_name, function_name);

        return spawn(process_name, parent, function, stack_top);
    }

    u32 ProcessRunner::kill_by_name(StringId name) {
        Carbon *c = current_carbon();
        if (!c) return 0;

        // Real cleanup lands when Carbon::Scheduler is written.
        // For now, just report nothing was killed.
        lg::debug("kill_by_name('{}') — not implemented yet", name);
        return 0;
    }

    u32 ProcessRunner::kill_by_type(StringId type_name) {
        lg::debug("kill_by_type('{}') — not implemented yet", type_name);
        return 0;
    }

} // namespace carbon