#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/util/Formatter.hpp"
#include "fmt/ranges.h"
#include <cstddef>
#include <fmt/format.h>
#include <vector>
#include <string>

using namespace carbon::lib;
using namespace util;

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
    std::string result;
    result += Formatter::instance().format("StateDesc '{}'{{\n", name);
    Formatter::instance().inc_column(4);
    result += Formatter::instance().format("parent_state {}\n", parent_state);
    result += Formatter::instance().format("count:       {:02x}\n", defs_count);
    result += Formatter::instance().format("offset:      0x{:04x}\n", definitions.offset);
    result += Formatter::instance().format("flags:       0x{:02x} ({})\n",  static_cast<uint32_t>(flags), format_flags(flags));

    Formatter::instance().inc_column(4);

    if (definitions.ptr && defs_count > 0) {
        for (size_t i=0; i<defs_count && i<=EVENT_ID; i++) {
            result += Formatter::instance().format("[{}]\n", i);
            result += definitions.ptr[i].inspect();
        }
    }

    Formatter::instance().inc_column(-4);
    Formatter::instance().inc_column(-4);
    result += Formatter::instance().format("}}\n");
    return result;
}

void StateDesc::relocate_pointers(bool to_memory, intptr_t delta, Module* module) {
    (void)module;
   
    if (to_memory)
    {
        if (definitions.offset!=0) {
            definitions.offset += delta;
            Definition::relocate_pointers_table(to_memory, delta, definitions.ptr, defs_count, module);
        }

    } else {
        if (definitions.offset!=0) {
            Definition::relocate_pointers_table(to_memory, delta, definitions.ptr, defs_count, module);
            definitions.offset += delta;
        }
    }
    lg::print("[StateDesc] relocate_pointers #definitions {:016X} {}\n", definitions.offset, name);            
}

} // end of namespace