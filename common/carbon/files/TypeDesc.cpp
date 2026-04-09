#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/StateDesc.hpp"
#include "common/util/Formatter.hpp"
#include "files/FunctionDesc.hpp"
#include "fmt/format.h"

using namespace util;

namespace carbon::files {

std::string TypeDesc::to_string() const {
    return fmt::format("TypeDesc({} : {})", name, parent_type_id);
}

std::string TypeDesc::inspect() const {
    std::string result;
    result += Formatter::instance().format("TypeDesc {{\n");
    Formatter::instance().inc_column(2);
    result += Formatter::instance().format("name: {}\n", name);
    result += Formatter::instance().format("parent: {}\n", parent_type_id);
    result += Formatter::instance().format("flags: 0x{:016x}\n", flags);
    result += Formatter::instance().format("size_in_memory: {}\n", size_in_memory);
    result += Formatter::instance().format("methods_offset: {:016X}\n", methods_ptr.offset);
    result += Formatter::instance().format("methods_count: {}\n", methods_count);
    result += Formatter::instance().format("states_offset: {:016X}\n", states_ptr.offset);
    result += Formatter::instance().format("states_count: {}\n", states_count);
    result += Formatter::instance().format("heap_base: {:016X}\n",heap_base);
    

    if (methods_ptr.ptr && methods_count > 0) {
        Formatter::instance().inc_column(2);
        result += Formatter::instance().format("Methods:\n");
        Formatter::instance().inc_column(2);
        for (uint32_t i = 0; i < methods_count; i++) {
            auto* method_desc = methods_ptr.ptr[i].ptr;
            if (method_desc) {
                result += Formatter::instance().format("[{}]\n", i);
                result += method_desc->inspect();
            } else {
                result += Formatter::instance().format("[{}] <null>\n", i);
            }
        }
        Formatter::instance().inc_column(-2);
        Formatter::instance().inc_column(-2);
    }
    
    if (states_ptr.ptr && states_count > 0) {
        Formatter::instance().inc_column(2);
        result += Formatter::instance().format("States:\n");
        Formatter::instance().inc_column(2);
        for (uint32_t i = 0; i < states_count; i++) {
            auto* state_desc = states_ptr.ptr[i].ptr;
            if (state_desc) {
                result += Formatter::instance().format("[{}]\n", i);
                result += state_desc->inspect();
            } else {
                result += Formatter::instance().format("[{}] <null>\n", i);
            }
        }
        Formatter::instance().inc_column(-2);
        Formatter::instance().inc_column(-2);
    }


    Formatter::instance().inc_column(-2);
    result += Formatter::instance().format("}}\n");
    
    return result;
}

void TypeDesc::relocate_pointers(bool to_memory, intptr_t delta, Module* owner) {

    if (to_memory) {
        if (methods_ptr.offset) {
            methods_ptr.offset += delta;
            relocate_methods_table(to_memory, delta, owner);
        }
        if (states_ptr.offset) {
            states_ptr.offset += delta;
            relocate_states_table(to_memory, delta, owner);
        }
    } else {
        if (methods_ptr.offset) {
            relocate_methods_table(to_memory, delta,  owner);
            methods_ptr.offset += delta;
        }
        if (states_ptr.offset) {
            relocate_states_table(to_memory, delta, owner);
            states_ptr.offset += delta;
        }
    }
    lg::info("[TypeDesc] relocate_pointers #methods {:016X} #states {:016X}", methods_ptr.offset+delta, states_ptr.offset+delta, name);
}

void TypeDesc::relocate_methods_table(bool to_memory, intptr_t delta, Module* module) {
    // Применяем ко всем определениям
    for (u32 i = 0; i < methods_count; i++) {
        Ptr<FunctionDesc>* def = &methods_ptr.ptr[i];
        if (to_memory) {
            if (def->offset) {
                def->offset += delta;
                def->ptr->relocate_pointers(to_memory, delta, module);
            }
        } else {
            if (def->offset) {
                def->ptr->relocate_pointers(to_memory, delta, module);
                def->offset += delta;
            }
        }
    }                  
}

void TypeDesc::relocate_states_table(bool to_memory, intptr_t delta, Module* module) {
    // Применяем ко всем определениям
    for (u32 i = 0; i < states_count; i++) {
        Ptr<StateDesc>* def = &states_ptr.ptr[i];
        if (to_memory) {
            if (def->offset) {
                def->offset += delta;
                def->ptr->relocate_pointers(to_memory, delta, module);
            }
        } else {
            if (def->offset) {
                def->ptr->relocate_pointers(to_memory, delta, module);
                def->offset += delta;
            }
        }
    }                  
}   

} // namespace carbon::files