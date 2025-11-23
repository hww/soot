#include "state_machine.hpp"
#include "process.hpp"
#include "virtual_machine.hpp"
#include "util/assert.h"
#include "util/log.h"

namespace vm {

    StateMachine::StateMachine(Process* owner)
        : owner_process_(owner) {
        ASSERT(owner_process_ != nullptr);
        lg::debug("StateMachine created for process: {}", owner_process_->get_name());
    }

    // ------------------------------------------------------------------------
    // State Management
    // ------------------------------------------------------------------------

    void StateMachine::add_state(std::unique_ptr<StateDefinition> state) {
        ASSERT(state != nullptr);
        std::string name = state->name;
        states_[name] = std::move(state);
        lg::debug("StateMachine added state: {}", name);
    }

    void StateMachine::add_transition(std::unique_ptr<TransitionDefinition> transition) {
        ASSERT(transition != nullptr);
        transitions_.push_back(std::move(transition));
        lg::debug("StateMachine added transition: {} -> {}",
            transitions_.back()->from_state, transitions_.back()->to_state);
    }

    // ------------------------------------------------------------------------
    // FSM Control
    // ------------------------------------------------------------------------

    void StateMachine::set_initial_state(const std::string& state_name) {
        auto it = states_.find(state_name);
        if (it == states_.end()) {
            lg::error("StateMachine: initial state '{}' not found", state_name);
            return;
        }

        current_state_ = state_name;
        lg::debug("StateMachine set initial state: {}", state_name);

        // Execute enter bytecode for initial state
        execute_state_enter(it->second.get());
    }

    bool StateMachine::transition_to(const std::string& state_name) {
        if (state_name == current_state_) {
            return true; // Already in this state
        }

        auto it = states_.find(state_name);
        if (it == states_.end()) {
            lg::error("StateMachine: target state '{}' not found", state_name);
            return false;
        }

        // Check if transition is allowed
        bool transition_allowed = false;
        for (const auto& transition : transitions_) {
            if (transition->from_state == current_state_ && transition->to_state == state_name) {
                if (transition->has_condition()) {
                    // Execute condition bytecode
                    execute_bytecode(transition->condition_bytecode);
                    // Would need to check return value - simplified for now
                    transition_allowed = true;
                }
                else {
                    transition_allowed = true;
                }
                break;
            }
        }

        if (!transition_allowed) {
            lg::warn("StateMachine: transition from '{}' to '{}' not allowed",
                current_state_, state_name);
            return false;
        }

        internal_state_change(state_name);
        return true;
    }

    void StateMachine::update() {
        if (current_state_.empty()) {
            return;
        }

        StateDefinition* current = get_current_state();
        if (!current) {
            return;
        }

        // Update time in state
        current->time_in_state++;

        // Execute update bytecode (main state behavior)
        if (current->has_update()) {
            execute_state_update(current);
        }

        // Check for automatic transitions
        check_transition_conditions();
    }

    // ------------------------------------------------------------------------
    // State Execution
    // ------------------------------------------------------------------------

    void StateMachine::execute_state_enter(StateDefinition* state) {
        if (!state || !state->has_enter()) {
            return;
        }

        state->enter_count++;
        lg::debug("StateMachine executing enter for: {}", state->name);
        execute_bytecode(state->enter_bytecode);
    }

    void StateMachine::execute_state_exit(StateDefinition* state) {
        if (!state || !state->has_exit()) {
            return;
        }

        lg::debug("StateMachine executing exit for: {}", state->name);
        execute_bytecode(state->exit_bytecode);
    }

    void StateMachine::execute_state_update(StateDefinition* state) {
        if (!state || !state->has_update()) {
            return;
        }

        // Execute the main state behavior bytecode
        execute_bytecode(state->update_bytecode);
    }

    bool StateMachine::check_transition_conditions() {
        for (const auto& transition : transitions_) {
            if (transition->from_state == current_state_ && transition->has_condition()) {
                // Execute condition bytecode and check result
                // This would need proper return value handling from bytecode execution
                // Simplified for now - always return false
                bool condition_met = false; // Would be actual result from bytecode

                if (condition_met) {
                    if (transition->has_action()) {
                        execute_bytecode(transition->action_bytecode);
                    }
                    internal_state_change(transition->to_state);
                    return true;
                }
            }
        }
        return false;
    }

    void StateMachine::internal_state_change(const std::string& new_state) {
        // Execute exit for current state
        if (!current_state_.empty()) {
            StateDefinition* old_state = get_current_state();
            if (old_state) {
                execute_state_exit(old_state);
            }
        }

        // Change state
        previous_state_ = current_state_;
        current_state_ = new_state;

        // Execute enter for new state
        StateDefinition* new_state_ptr = get_current_state();
        if (new_state_ptr) {
            execute_state_enter(new_state_ptr);
        }

        lg::debug("StateMachine state change: {} -> {}", previous_state_, current_state_);
    }

    void StateMachine::execute_bytecode(ByteCode* bytecode) {
        if (!bytecode || !owner_process_) {
            return;
        }

        // Use the process to execute the bytecode
        // This creates a temporary frame for state behavior
        owner_process_->push_method_frame(bytecode, owner_process_->get_self(), 0);

        // Execute until completion or suspension
        // In real implementation, this would need proper frame management
    }

    // ------------------------------------------------------------------------
    // Getters
    // ------------------------------------------------------------------------

    u32 StateMachine::get_time_in_current_state() const {
        if (current_state_.empty()) {
            return 0;
        }

        auto it = states_.find(current_state_);
        return it != states_.end() ? it->second->time_in_state : 0;
    }

    std::string StateMachine::to_string() const {
        return fmt::format("StateMachine(States:{}, Current:'{}', Previous:'{}')",
            states_.size(), current_state_, previous_state_);
    }

} // namespace vm