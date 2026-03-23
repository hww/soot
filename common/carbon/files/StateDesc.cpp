#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "fmt/ranges.h"
#include <fmt/format.h>
#include <vector>
#include <string>

using namespace runtime::lib;

namespace runtime::files {

static std::string format_flags(StateFlags flags) {
    if (flags == StateFlags::None) {
        return "";
    }
    
    std::vector<std::string_view> active;
    
    if ((flags & StateFlags::Initial) != StateFlags::None) 
        active.push_back("Initial");
    if ((flags & StateFlags::Final) != StateFlags::None) 
        active.push_back("Final");
    if ((flags & StateFlags::History) != StateFlags::None) 
        active.push_back("History");
    if ((flags & StateFlags::DeepHistory) != StateFlags::None) 
        active.push_back("DeepHistory");
    if ((flags & StateFlags::Parallel) != StateFlags::None) 
        active.push_back("Parallel");
    if ((flags & StateFlags::Deferred) != StateFlags::None) 
        active.push_back("Deferred");
    
    return fmt::format(" ({})", fmt::join(active, " | "));
}

std::string StateDesc::to_string() const {
    return fmt::format("{} : {}", 
                       string_id::to_string(name), 
                       string_id::to_string(parent_state));
}

std::string StateDesc::inspect() const {
    return fmt::format(
        "StateDesc {{\n"
        "  name: {}\n"
        "  parent_state: {}\n"
        "  count: {:02x}\n"
        "  offset: 0x{:04x}\n"
        "  flags: 0x{:02x}{}\n"
        "}}",
        string_id::to_string(name),
        string_id::to_string(parent_state),
        count,
        offset,
        static_cast<uint32_t>(flags),
        format_flags(flags)
    );
}

void StateDesc::relocate_pointers(intptr_t delta) {
    // If offset is a pointer offset, adjust it
    if (offset != 0) {
        offset += delta;
    }
    
    // Note: StateDesc doesn't contain any direct pointers that need relocation
    // The offset field might be a pointer to handlers or methods
    // Adjust it if it points to memory within the module
}

} // end of namespace