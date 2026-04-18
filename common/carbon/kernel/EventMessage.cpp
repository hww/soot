#include "common/carbon/kernel/EventMessage.hpp"
#include "common/carbon/kernel/Process.hpp"

namespace carbon {
    /**
     * @brief Convert to string for debugging
     */
    std::string EventMessage::to_string() const {
        std::string result = fmt::format(
            "EventMessage(from:{}, to:{}, message:{}, num_params:{})",
            from ? from->name_str() : "null",
            to   ? to->name_str()   : "null",
            message.to_string(),
            num_params
        );
        
        for (int i = 0; i < num_params && i < MAX_PARAMS; i++) {
            result += fmt::format(", param{}:{}", i, params[i].to_string());
        }
        
        return result;
    }
}