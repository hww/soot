#include "common/CommonTypes.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "util/Log.hpp"

using namespace carbon::lib;

namespace carbon::files {
    // =============================================================================
    // FunctionDescError Implementation
    // =============================================================================

    FunctionDescError::FunctionDescError(const std::string& msg) : message(msg) {}

    const char* FunctionDescError::what() const noexcept {
        return message.c_str();
    }

    // =============================================================================
    // SourceLocation Implementation  
    // =============================================================================

    std::string SourceLocation::to_string() const {
        return fmt::format("SourceLocation(ip:{:04x}, line:{}, file:{})",
            offset, line, file);
    }

    // =============================================================================
    // Definition Implementation  
    // =============================================================================
    
    void Definition::relocate_pointers_table(bool to_memory, intptr_t delta, Definition* definitions, size_t definitions_count, Module* module) {
        // Применяем ко всем определениям
        for (u32 i = 0; i < definitions_count; i++) {

            Definition* def = &definitions[i];
            def->relocate_pointers(to_memory, delta, module);
        }                  
    }

    inline bool has_flag(SymbolFlags flags, SymbolFlags flag) {
        return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
    }

    std::string get_symbol_flags_string(SymbolFlags flags) {
        if (flags == SymbolFlags::None) return "none";
        
        std::string result;
        if (has_flag(flags, SymbolFlags::Local)) result += "local";
        if (has_flag(flags, SymbolFlags::Import)) {
            if (!result.empty()) result += "|";
            result += "import";
        }
        if (has_flag(flags, SymbolFlags::Export)) {
            if (!result.empty()) result += "|";
            result += "export";
        }
        return result;
    }

    std::string Definition::to_string() const {
        return std::format("Definition(:name '{}', :type '{}', :ptr {:x} :flags {})",
            name, type, (u64)data.offset, get_symbol_flags_string(flags));
    }

    std::string Definition::inspect() const {
        auto result = std::format("(definition {} :type {} :ptr {:x} :flags {})",
            name, type, (u64)data.offset, get_symbol_flags_string(flags));

        if (data.ptr) {
            if (data.offset) {
                if (type == TypeIds::function)
                    result += (reinterpret_cast<FunctionDesc*>(data.ptr))->inspect();    
                else if (type == TypeIds::type)
                    result += (reinterpret_cast<TypeDesc*>(data.ptr))->inspect();    
                else if (type == TypeIds::state)
                    result += (reinterpret_cast<StateDesc*>(data.ptr))->inspect();  
                else
                    lg::error("unexpected definition type {}\n", type);
            }            
        }
        return result;
    }

    void Definition::relocate_pointers(bool to_memory, intptr_t delta, Module* module) {
        fmt::print("[Definition] relocate_pointers :name {} :offset {:08X}\n", name, data.offset);

        if (to_memory) {
            if (data.offset != 0) 
                data.offset+=delta;
        }

        if (data.offset) {
            if (type == TypeIds::function)
                (reinterpret_cast<FunctionDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);    
            else if (type == TypeIds::type)
                (reinterpret_cast<TypeDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);    
            else if (type == TypeIds::state)
                (reinterpret_cast<StateDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);  
            else
                lg::error("unexpected definition type {}\n", type);
        }

        if (!to_memory) {
            if (data.offset != 0) 
                data.offset+=delta;
        }

        fmt::print("[Definition] relocate_pointers :name {} :offset {:08X}\n", name, data.offset);
    }

    // =============================================================================
    // MethoodDefinition Implementation
    // =============================================================================
    
    void MethodDef::relocate_pointers_table(bool to_memory, intptr_t delta, MethodDef* definitions, size_t definitions_count, Module* module) {
        // Применяем ко всем определениям
        for (u32 i = 0; i < definitions_count; i++) {

            MethodDef* def = &definitions[i];
            def->relocate_pointers(to_memory, delta, module);
        }                  
    }

    inline bool has_flag(MethodFlags flags, MethodFlags flag) {
        return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
    }

    std::string get_symbol_flags_string(MethodFlags flags) {
        if (flags == MethodFlags::None) return "none";
        
        std::string result;
        if (has_flag(flags, MethodFlags::Virtual)) result += "virtual";
        if (has_flag(flags, MethodFlags::Override)) {
            if (!result.empty()) result += "|";
            result += "override";
        }
        return result;
    }

    std::string MethodDef::to_string() const {
        return std::format("Definition(:name '{}', :type '{}', :ptr {:x} :flags {})",
            name, type, (u64)data.offset, get_symbol_flags_string(flags));
    }

    std::string MethodDef::inspect() const {
        auto result = std::format("(definition {} :type {} :ptr {:x} :flags {})",
            name, type, (u64)data.offset, get_symbol_flags_string(flags));
        if (data.ptr!=nullptr)
            result += data.ptr->inspect();
        return result;
    }

    void MethodDef::relocate_pointers(bool to_memory, intptr_t delta, Module* module) {
        fmt::print("[MethodDef] relocate_pointers :name {} :offset {:08X}\n", name, data.offset);

        if (to_memory) {
            if (data.offset != 0) 
                data.offset+=delta;
        }

        if (data.offset) {
            if (type == TypeIds::function)
                (reinterpret_cast<FunctionDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);    
            else if (type == TypeIds::type)
                (reinterpret_cast<TypeDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);    
            else if (type == TypeIds::state)
                (reinterpret_cast<StateDesc*>(data.ptr))->relocate_pointers(to_memory, delta, module);  
            else
                lg::error("unexpected definition type {}\n", type);
        }

        if (!to_memory) {
            if (data.offset != 0) 
                data.offset+=delta;
        }

        fmt::print("[MethodDef] relocate_pointers :name {} :offset {:08X}\n", name, data.offset);
    }    

} // end of namespace