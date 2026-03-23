#include "common/carbon/kernel/StateDefinition.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"
#include <algorithm>
#include <vector>

namespace runtime::kernel
{

    StateDefinition::StateDefinition(StringId state_name, FunctionDesc* update_code)
        : name(state_name), update_FunctionDesc(update_code) {
    }

    bool StateDefinition::has_event(StringId event_type) const {
        return event_handlers.find(event_type) != event_handlers.end();
    }

    bool StateDefinition::has_any_events() const {
        return !event_handlers.empty();
    }

    void StateDefinition::add_event_handler(StringId event_type, FunctionDesc* handler) {
        event_handlers[event_type] = handler;
    }

    void StateDefinition::remove_event_handler(StringId event_type) {
        event_handlers.erase(event_type);
    }

    FunctionDesc* StateDefinition::get_event_handler(StringId event_type) const {
        auto it = event_handlers.find(event_type);
        return it != event_handlers.end() ? it->second : nullptr;
    }

    bool StateDefinition::inherits_from(StringId other) const {
        return parent_state == other;
    }

    bool StateDefinition::can_activate_directly() const {
        return !metadata.is_virtual;
    }

    void StateDefinition::update_time_stats(u32 delta_time) {
        stats.time_in_state += delta_time;
    }

    void StateDefinition::record_enter() {
        stats.enter_count++;
    }

    void StateDefinition::record_update() {
        stats.update_count++;
    }

    void StateDefinition::record_event_handled() {
        stats.events_handled++;
    }

    void StateDefinition::record_cycles(u64 cycles) {
        stats.total_cycles += cycles;
    }

    void StateDefinition::update_consecutive_frames(u32 consecutive_frames) {
        if (consecutive_frames > stats.max_consecutive_frames) {
            stats.max_consecutive_frames = consecutive_frames;
        }
    }

    void StateDefinition::reset_statistics() {
        stats = Statistics{};
    }

    bool StateDefinition::is_valid() const {
        return has_enter() || has_exit() || has_update() ||
            has_trans() || has_post() || has_any_events();
    }

    std::string StateDefinition::get_name_string() const {
        return lib::to_string(name);
    }

    std::string StateDefinition::get_parent_name_string() const {
        return lib::to_string(parent_state);
    }

    std::string StateDefinition::to_string() const {
        std::string result = fmt::format("State('{}'", get_name_string());

        if (parent_state != string_id::NONE) {
            result += fmt::format(", parent:'{}'", get_parent_name_string());
        }

        if (metadata.is_virtual) {
            result += ", virtual";
            if (metadata.is_override) {
                result += "-override";
            }
        }

        // Собираем информацию об обработчиках
        std::vector<std::string> handlers;
        if (has_enter()) handlers.push_back("enter");
        if (has_trans()) handlers.push_back("trans");
        if (has_update()) handlers.push_back("update");
        if (has_post()) handlers.push_back("post");
        if (has_exit()) handlers.push_back("exit");
        if (has_any_events()) {
            handlers.push_back(fmt::format("events:{}", event_handlers.size()));
        }

        if (!handlers.empty()) {
            result += fmt::format(", handlers:[{}]", fmt::join(handlers, ", "));
        }

        result += ")";
        return result;
    }

    std::string StateDefinition::statistics_to_string() const {
        return fmt::format(
            "State '{}' stats: time={}ms, enters={}, updates={}, events={}, cycles={}",
            get_name_string(),
            stats.time_in_state,
            stats.enter_count,
            stats.update_count,
            stats.events_handled,
            stats.total_cycles
        );
    }

    bool StateDefinition::operator==(const StateDefinition& other) const {
        return name == other.name;
    }

    bool StateDefinition::operator!=(const StateDefinition& other) const {
        return !(*this == other);
    }

    bool StateDefinition::operator<(const StateDefinition& other) const {
        return name < other.name;
    }

} // namespace runtime