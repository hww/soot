// common/carbon/kernel/EventMessage.hpp
#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/lib/StringId.hpp"

using namespace carbon;

namespace carbon {
     class Process;   
/**
 * @brief Event message structure (like in GOAL)
 * Used to pass events between processes
 */
struct EventMessage {
    static const int MAX_PARAMS = 7;

    Process* from = nullptr;    // Sender process
    Process* to = nullptr;      // Receiver process
    StringId message;           // Event type
    int num_params = 0;         // Number of parameters
    
    // Event parameters as inline array (up to 7 as in GOAL)
    Variant params[MAX_PARAMS];
    
    /**
     * @brief Get parameter by index
     * @param index Parameter index (0-6)
     * @return Variant value or null if index out of range
     */
    Variant get_arg(int index) const {
        if (index >= 0 && index < MAX_PARAMS) {
            return params[index];
        }
        return Variant();
    }
    
    /**
     * @brief Set parameter by index
     * @param index Parameter index (0-6)
     * @param value Value to set
     */
    void set_arg(int index, const Variant& value) {
        if (index >= 0 && index < MAX_PARAMS) {
            params[index] = value;
        }
    }
    
    /**
     * @brief Clear all parameters
     */
    void clear() {
        from = nullptr;
        to = nullptr;
        num_params = 0;
        message = StringId();
        for (int i = 0; i < MAX_PARAMS; i++) {
            params[i] = Variant();
        }
    }
    
    /**
     * @brief Convert to string for debugging
     */
    std::string to_string() const ;
};

} // namespace carbon