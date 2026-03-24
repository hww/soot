#include "common/carbon/files/TypeDesc.hpp"
#include "fmt/ranges.h"
#include <vector>
#include <fmt/format.h>

using namespace carbon::lib;

namespace carbon::files {

static std::string format_type_flags(TypeFlags flags) {
    if (flags == TypeFlags::None) {
        return "";
    }
    
    std::vector<std::string_view> active;
    
    if ((flags & TypeFlags::IsReference) != TypeFlags::None) 
        active.push_back("IsReference");
    if ((flags & TypeFlags::IsLoadSigned) != TypeFlags::None) 
        active.push_back("IsLoadSigned");
    if ((flags & TypeFlags::IsConst) != TypeFlags::None) 
        active.push_back("IsConst");
    if ((flags & TypeFlags::IsVolatile) != TypeFlags::None) 
        active.push_back("IsVolatile");
    if ((flags & TypeFlags::IsPacked) != TypeFlags::None) 
        active.push_back("IsPacked");
    if ((flags & TypeFlags::IsAbstract) != TypeFlags::None) 
        active.push_back("IsAbstract");
    if ((flags & TypeFlags::IsSealed) != TypeFlags::None) 
        active.push_back("IsSealed");
    if ((flags & TypeFlags::HasFinalizer) != TypeFlags::None) 
        active.push_back("HasFinalizer");
    if ((flags & TypeFlags::HasStaticConstructor) != TypeFlags::None) 
        active.push_back("HasStaticConstructor");
    if ((flags & TypeFlags::IsValueType) != TypeFlags::None) 
        active.push_back("IsValueType");
    if ((flags & TypeFlags::IsEnum) != TypeFlags::None) 
        active.push_back("IsEnum");
    if ((flags & TypeFlags::IsInterface) != TypeFlags::None) 
        active.push_back("IsInterface");
    if ((flags & TypeFlags::IsGeneric) != TypeFlags::None) 
        active.push_back("IsGeneric");
    
    return fmt::format(" ({})", fmt::join(active, " | "));
}

std::string TypeDesc::to_string() const {
    return fmt::format("{} : {}", 
                       name, 
                       parent_type_id);
}

std::string TypeDesc::inspect() const {
    return fmt::format(
        "TypeDesc {{\n"
        "  name: {}\n"
        "  parent: {}\n"
        "  methods_offset: 0x{:08x}\n"
        "  states_offset: 0x{:08x}\n"
        "  methods_count: {}\n"
        "  states_count: {}\n"
        "  flags: 0x{:04x}{}\n"
        "  reg_class: {}\n"
        "  load_size: {}\n"
        "  in_memory_alignment: {}\n"
        "  inline_array_stride_alignment: {}\n"
        "  inline_array_start_alignment: {}\n"
        "  offset: {}\n"
        "}}",
        name,
        parent_type_id,
        methods_offset,
        states_offset,
        methods_count,
        states_count,
        static_cast<uint32_t>(flags),
        format_type_flags(flags),
        static_cast<int>(reg_class),
        load_size,
        in_memory_alignment,
        inline_array_stride_alignment,
        inline_array_start_alignment,
        offset
    );
}

void TypeDesc::relocate_pointers(intptr_t delta) {
    // Adjust offset fields that might point to other data
    if (methods_offset != 0) {
        methods_offset += delta;
    }
    if (states_offset != 0) {
        states_offset += delta;
    }
    
    // Note: TypeDesc contains offsets to methods and states arrays
    // These need to be adjusted when the module is relocated in memory
}

} // end of namespace