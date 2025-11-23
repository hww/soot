#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "binary_file.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

namespace vm {

    // Forward declarations
    class Process;
    struct ByteCode;

    /**
     * @brief FSM State definition using ByteCode (not native functions)
     */
    struct StateDefinition {
        std::string name;
        ByteCode* enter_bytecode = nullptr;    // ByteCode for state entry
        ByteCode* exit_bytecode = nullptr;     // ByteCode for state exit  
        ByteCode* update_bytecode = nullptr;   // Main state behavior (like 'code' in GOAL)
        ByteCode* trans_bytecode = nullptr;    // Transition logic

        // State-specific data
        Variant data;

        // Statistics
        u32 time_in_state = 0;
        u32 enter_count = 0;

        StateDefinition(const std::string& state_name, ByteCode* update_code = nullptr)
            : name(state_name), update_bytecode(update_code) {
        }

        bool has_enter() const { return enter_bytecode != nullptr; }
        bool has_exit() const { return exit_bytecode != nullptr; }
        bool has_update() const { return update_bytecode != nullptr; }
        bool has_trans() const { return trans_bytecode != nullptr; }

        std::string to_string() const {
            return fmt::format("State('{}')", name);
        }
    };

    /**
     * @brief Transition between states with ByteCode conditions
     */
    struct TransitionDefinition {
        std::string from_state;
        std::string to_state;
        ByteCode* condition_bytecode = nullptr;  // Returns bool for transition
        ByteCode* action_bytecode = nullptr;     // Action during transition

        TransitionDefinition(const std::string& from, const std::string& to)
            : from_state(from), to_state(to) {
        }

        bool has_condition() const { return condition_bytecode != nullptr; }
        bool has_action() const { return action_bytecode != nullptr; }
    };

    /**
     * @brief Finite State Machine working purely with ByteCode
     *
     * Follows our basis: No VM instructions, uses existing CALL mechanism
     */
    class StateMachine {
    public:
        explicit StateMachine(Process* owner);

        // State management
        void add_state(std::unique_ptr<StateDefinition> state);
        void add_transition(std::unique_ptr<TransitionDefinition> transition);

        // FSM control
        void set_initial_state(const std::string& state_name);
        bool transition_to(const std::string& state_name);
        void update();  // Called each process quantum

        // State access
        StateDefinition* get_current_state() {
            return current_state_ ? states_[current_state_].get() : nullptr;
        }

        StateDefinition* get_state(const std::string& name) {
            auto it = states_.find(name);
            return it != states_.end() ? it->second.get() : nullptr;
        }

        // Getters
        const std::string& get_current_state_name() const { return current_state_; }
        const std::string& get_previous_state_name() const { return previous_state_; }
        u32 get_time_in_current_state() const;

        std::string to_string() const;

    private:
        Process* owner_process_;
        std::unordered_map<std::string, std::unique_ptr<StateDefinition>> states_;
        std::vector<std::unique_ptr<TransitionDefinition>> transitions_;

        std::string current_state_;
        std::string previous_state_;

        void execute_state_enter(StateDefinition* state);
        void execute_state_exit(StateDefinition* state);
        void execute_state_update(StateDefinition* state);
        bool check_transition_conditions();
        void internal_state_change(const std::string& new_state);

        // Helper to execute ByteCode through process
        void execute_bytecode(ByteCode* bytecode);
    };

} // namespace vm