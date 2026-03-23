#pragma once

#include <cstdint>
#include <string>
#include "common/carbon/files/Base.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"

using namespace runtime::lib;

namespace runtime::files {

enum class StateFlags : uint32_t {
    None        = 0x00,
    Initial     = 0x01,
    Final       = 0x02,
    History     = 0x04,
    DeepHistory = 0x08,
    Parallel    = 0x10,
    Deferred    = 0x20
};
ENUM_FLAG_OPERATORS(StateFlags);

/**
 * @brief State header for finite state machines
 * 
 * Contains metadata about a state including its handlers and parent state.
 */
struct StateDesc : public Descriptor {
    static constexpr int CODE_ID  = 0;
    static constexpr int ENTER_ID = 1;
    static constexpr int EXIT_ID  = 2;
    static constexpr int TRANS_ID = 3;
    static constexpr int POST_ID  = 4;
    static constexpr int EVENT_ID = 5;
    
    StringId name;
    StringId parent_state;  // Parent state
    uint32_t count;       // Number of handlers
    int64_t offset;       // Offset to methods
    StateFlags flags;
    
    /**
     * @brief Convert to simple string representation
     * @return Basic string representation
     */
    std::string to_string() const;
    
    /**
     * @brief Create detailed inspection string
     * @return Detailed formatted string for debugging
     */
    std::string inspect() const;
    
    /**
     * @brief Check if a specific flag is set
     * @param flag The flag to check
     * @return true if the flag is set, false otherwise
     */
    inline bool has_flag(StateFlags flag) const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }
    
    /**
     * @brief Set a specific flag
     * @param flag The flag to set
     */
    inline void set_flag(StateFlags flag) {
        flags = flags | flag;
    }
    
    /**
     * @brief Clear a specific flag
     * @param flag The flag to clear
     */
    inline void clear_flag(StateFlags flag) {
        flags = flags & (~flag);
    }
    
    /**
     * @brief Get handler ID constant
     * @param index Handler index (0-5)
     * @return Handler ID constant
     */
    static inline int get_handler_id(int index) {
        static const int ids[] = {CODE_ID, ENTER_ID, EXIT_ID, TRANS_ID, POST_ID, EVENT_ID};
        if (index >= 0 && index < 6) return ids[index];
        return -1;
    }

    void relocate_pointers(intptr_t delta);

};

} // end of namespace