#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/StateDesc.hpp"
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
            result += fmt::format("    [{}] {}\n", i, methods_offset.ptr[i].inspect());
        }
    }
    
    if (states_offset.ptr && states_count > 0) {
        result += "  States:\n";
        for (uint32_t i = 0; i < states_count; i++) {
            result += fmt::format("    [{}] {}\n", i, states_offset.ptr[i].inspect());
        }
    }
    
    return result;
}

void TypeDesc::relocate_pointers(bool to_memory, intptr_t delta, Module* owner) {
    if (to_memory) {
        if (methods_offset.offset) {
            methods_offset.offset += delta;
            MethodDef::relocate_pointers_table(to_memory, delta, methods_offset.ptr, methods_count, owner);
        }
        if (states_offset.offset) {
            states_offset.offset += delta;
            StateDef::relocate_pointers_table(to_memory, delta, states_offset.ptr, states_count, owner);
        }
    } else {
        if (methods_offset.offset) {
            MethodDef::relocate_pointers_table(to_memory, delta, methods_offset.ptr, methods_count, owner);
            methods_offset.offset += delta;
        }
        if (states_offset.offset) {
            StateDef::relocate_pointers_table(to_memory, delta, states_offset.ptr, states_count, owner);
            states_offset.offset += delta;
        }
    }
}

} // namespace carbon::files