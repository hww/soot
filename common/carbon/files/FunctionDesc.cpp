#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include <fmt/format.h>
#include <string>

using namespace carbon::lib;
using namespace carbon::vm;

namespace carbon::files {

    
FunctionDesc::FunctionDesc() 
    : code_count(0)
    , data_size(0)
    , debug_count(0)
    , code_ptr(nullptr)
    , data_ptr(nullptr)
    , debug_ptr(nullptr)
    , owner_module(nullptr) {
    desc_size = sizeof(FunctionDesc);
}

u8* FunctionDesc::get_data_ptr() const {
    return data_ptr.get();
}

SourceLocation* FunctionDesc::get_debug_info() const {
    return debug_ptr.get();
}

bool FunctionDesc::has_debug_info() const {
    return debug_ptr != nullptr && debug_count > 0;
}

void FunctionDesc::relocate_pointers(intptr_t delta) {
    // Adjust all pointer offsets by the delta value
    // This is used when the FunctionDesc is moved in memory
    if (code_ptr.offset != 0) {
        code_ptr.offset += delta;
    }
    if (data_ptr.offset != 0) {
        data_ptr.offset += delta;
    }
    if (debug_ptr.offset != 0) {
        debug_ptr.offset += delta;
    }
}

SourceLocation FunctionDesc::find_source_location(u32 instruction_ip) const {
    auto debug_info = get_debug_info();
    if (!debug_info) {
        return SourceLocation{ 0, 0, 0, 0 };
    }
    // Linear search through debug info to find matching instruction offset
    // In production this could be optimized with binary search if entries are sorted
    for (u32 i = 0; i < debug_count; ++i) {
        if (debug_info[i].start == instruction_ip) {
            return debug_info[i];
        }
    }
    // No debug info found for this instruction
    return SourceLocation{ 0, 0, 0, 0 };
}

std::string FunctionDesc::inspect() const {
    return fmt::format(
        "FunctionDesc {{\n"
        "  code_count: {}\n"
        "  data_size: {}\n"
        "  debug_count: {}\n"
        "  code_ptr: {}\n"
        "  data_ptr: {}\n"
        "  debug_ptr: {}\n"
        "  owner_module: {}\n"
        "}}",
        code_count,
        data_size,
        debug_count,
        fmt::ptr(code_ptr.get()),
        fmt::ptr(data_ptr.get()),
        fmt::ptr(debug_ptr.get()),
        fmt::ptr(owner_module)
    );
}

} // namespace carbon::files
