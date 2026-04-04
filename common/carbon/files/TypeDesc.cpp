#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/StateDesc.hpp"
#include "files/FunctionDesc.hpp"
#include "fmt/format.h"

namespace carbon::files {

std::string TypeDesc::to_string() const {
    return fmt::format("TypeDesc({} : {})", name, parent_type_id);
}

std::string TypeDesc::inspect() const {
    std::string result = fmt::format(
        "TypeDesc {{\n"
        "  name: {}\n"
        "  parent: {}\n"
        "  flags: 0x{:016x}\n"
        "  size_in_memory: {}\n"
        "  methods_offset: {}\n"
        "  methods_count: {}\n"
        "  states_offset: {}\n"
        "  states_count: {}\n"
        "  heap_base: {}\n"
        "}}",
        name,
        parent_type_id,
        flags,
        size_in_memory,
        methods_offset.offset,
        methods_count,
        states_offset.offset,
        states_count,
        heap_base
    );
    
    if (methods_offset.ptr && methods_count > 0) {
        result += "  Methods:\n";
        for (uint32_t i = 0; i < methods_count; i++) {
            auto* method_desc = methods_offset.ptr[i].ptr;
            if (method_desc) {
                result += fmt::format("    [{}] {}\n", i, method_desc->inspect());
            } else {
                result += fmt::format("    [{}] <null>\n", i);
            }
        }
    }
    
    if (states_offset.ptr && states_count > 0) {
        result += "  States:\n";
        for (uint32_t i = 0; i < states_count; i++) {
            auto* state_desc = states_offset.ptr[i].ptr;
            if (state_desc) {
                result += fmt::format("    [{}] {}\n", i, state_desc->inspect());
            } else {
                result += fmt::format("    [{}] <null>\n", i);
            }
        }
    }
    
    return result;
}

void TypeDesc::relocate_pointers(bool to_memory, intptr_t delta, Module* owner) {
    if (to_memory) {
        if (methods_offset.offset) {
            methods_offset.offset += delta;
            relocate_methods_table(to_memory, delta, owner);
        }
        if (states_offset.offset) {
            states_offset.offset += delta;
            relocate_states_table(to_memory, delta, owner);
        }
    } else {
        if (methods_offset.offset) {
            relocate_methods_table(to_memory, delta,  owner);
            methods_offset.offset += delta;
        }
        if (states_offset.offset) {
            relocate_states_table(to_memory, delta, owner);
            states_offset.offset += delta;
        }
    }
}

void TypeDesc::relocate_methods_table(bool to_memory, intptr_t delta, Module* module) {
    // Применяем ко всем определениям
    for (u32 i = 0; i < methods_count; i++) {
        if (methods_offset.ptr[i].offset) {
            FunctionDesc* def = methods_offset.ptr[i].ptr;
            def->relocate_pointers(to_memory, delta, module);
        }
    }                  
}
void TypeDesc::relocate_states_table(bool to_memory, intptr_t delta, Module* module) {
    // Применяем ко всем определениям
    for (u32 i = 0; i < states_count; i++) {
        if (states_offset.ptr[i].offset) {
            StateDesc* def = states_offset.ptr[i].ptr;
            def->relocate_pointers(to_memory, delta, module);
        }
    }                  
}   

} // namespace carbon::files