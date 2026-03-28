#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"
#include "files/BinaryFile.hpp"
#include "files/FunctionDesc.hpp"
#include <cstdint>
#include <string>

using namespace carbon::lib;

namespace carbon::files {
/**
 * @brief State header contains additional flags
 */
enum class StateFlags : uint32_t {
    None        = 0x00,
    Virtual     = 0x01,
    Override    = 0x02,
};
ENUM_FLAG_OPERATORS(StateFlags);

/**
 * @brief State header for finite state machines
 * 
 * Contains metadata about a state including its handlers and parent state.
 */
struct StateDesc  {
    static constexpr int CODE_ID  = 0;
    static constexpr int ENTER_ID = 1;
    static constexpr int EXIT_ID  = 2;
    static constexpr int TRANS_ID = 3;
    static constexpr int POST_ID  = 4;
    static constexpr int EVENT_ID = 5;
    StringId name;
    StringId parent_state;          // Parent state
    uint32_t defs_count;                 // Number of handlers
    Ptr<Definition> definitions;    // Offset to Definition которыйе лежат в порядке ID
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
    
    void relocate_pointers(bool to_memory, intptr_t delta, Module* owmer);

    bool is_virtual() { return has_flag(StateFlags::Virtual);}
    bool is_override() { return has_flag(StateFlags::Override);}

    /**
    * @brief Get definition by index
    * @param idx Handler index (0-5)
    * @return Pointer to FunctionDesc or nullptr if not found
    */
    Definition* get_definition(uint idx) const {
        if (idx < 0 || idx >= defs_count) {
            return nullptr;
        }
        
        // Calculate pointer to the handler table
        Definition* handlers = reinterpret_cast<Definition*>(definitions.ptr);
        
        return &handlers[idx];
    }

    /**
    * @brief Get method by index
    * @param idx Handler index (0-5)
    * @return Pointer to FunctionDesc or nullptr if not found
    */
    FunctionDesc* get_method(uint idx) const {
        auto def = get_definition(idx);
        if (def) {
            // Calculate pointer to the handler table
            return reinterpret_cast<FunctionDesc*>(def->data.ptr);
        }
        return nullptr;
    }

    /**
    * @brief Get method by name
    * @param name Handler name (e.g., "enter", "update", etc.)
    * @return Pointer to FunctionDesc or nullptr if not found
    */
    FunctionDesc* resolve_method(StringId name) const {
        // Map common handler names to IDs
        if (name == SID("enter")) return get_method(ENTER_ID);
        if (name == SID("exit")) return get_method(EXIT_ID);
        if (name == SID("trans")) return get_method(TRANS_ID);
        if (name == SID("post")) return get_method(POST_ID);
        if (name == SID("code")) return get_method(CODE_ID);
        if (name == SID("event")) return get_method(EVENT_ID);       
        return nullptr;
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

    FunctionDesc* get_enter_function() { return get_method(ENTER_ID); }
    FunctionDesc* get_exit_function() { return get_method(EXIT_ID); }
    FunctionDesc* get_trans_function() { return get_method(TRANS_ID); }
    FunctionDesc* get_post_function() { return get_method(POST_ID); }
    FunctionDesc* get_code_function() { return get_method(CODE_ID); }
    FunctionDesc* get_event_handler() { return get_method(EVENT_ID); }
};

} // end of namespace