#include "common/carbon/files/StateDesc.hpp"
#include "fmt/ranges.h"
#include <fmt/format.h>
#include <vector>
#include <string>

using namespace carbon::lib;

namespace carbon::files {

static std::string format_flags(StateFlags flags) {
    if (flags == StateFlags::None) {
        return "";
    }
    
    std::vector<std::string_view> active;
    
    if ((flags & StateFlags::Virtual) != StateFlags::None) 
        active.push_back("Virtual");
    if ((flags & StateFlags::Override) != StateFlags::None) 
        active.push_back("Override");
    
    return fmt::format(" ({})", fmt::join(active, " | "));
}

std::string StateDesc::to_string() const {
    return fmt::format("{} : {}", 
                       name, 
                       parent_state);
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
        name,
        parent_state,
        count,
        definitions.offset,
        static_cast<uint32_t>(flags),
        format_flags(flags)
    );
}

void StateDesc::relocate_pointers(intptr_t delta) {
    // If offset is a pointer offset, adjust it
    definitions.offset += delta;
    
    // Note: StateDesc doesn't contain any direct pointers that need relocation
    // The offset field might be a pointer to handlers or methods
    // Adjust it if it points to memory within the module
    for (uint i=0;i<count; i++)
    {
        auto def = get_definition(i);
        def->relocate_pointers(delta);
    }
}

} // end of namespace