#pragma once

#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/files/Definition.hpp"
#include "type_system/Config.hpp"
#include <cstdint>
#include <string>

namespace carbon::modules { 
    class Module;
}

namespace carbon::files {

struct MethodDef;
struct StateDef;

/**
 * @brief Serialized type descriptor
 * 
 * Pure data structure - no logic, only serialization fields
 */
struct TypeDesc {
  // === Identification ===
    StringId name;                    // Type name
    StringId parent_type_id;          // Parent type name
    
    // === Type flags ===
    uint64_t flags;                   // Raw 64-bit flags value
    
    // === Runtime layout ===
    int size_in_memory;               // Total size in memory
    int heap_base;                    // Heap base offset (for process types)
    
    // === Methods ===
    Ptr<MethodDef> methods_offset;    // Pointer to methods array
    uint32_t methods_count;           // Number of methods
    
    // === States ===
    Ptr<StateDef> states_offset;      // Pointer to states array
    uint32_t states_count;            // Number of states
    
    // === Serialization methods ===
    std::string to_string() const;
    std::string inspect() const;
    void relocate_pointers(bool to_memory, intptr_t delta, Module* owner);

// ===================================================================
    // Methods
    // ===================================================================
    /**
     * @brief Resolve a method by index
     * @param index Method index (0-based)
     * @return Pointer to MethodDesc or nullptr if index out of bounds
     */
    MethodDef* get_method_def(u32 index) const {
        if (methods_offset == 0 || index >= methods_count) {
            return nullptr;
        }

        return &methods_offset.ptr[index];
    }
    /**
     * @brief Resolve a method by name
     * @param name Method name to find
     * @return Pointer to MethodDesc or nullptr if not found
     */
    MethodDef* resolve_method_def(StringId name) const {
        if (methods_offset == 0 || methods_count == 0) {
            return nullptr;
        }
        
        MethodDef* methods = methods_offset.ptr;
        
        for (u32 i = 0; i < methods_count; i++) {
            if (methods[i].name == name) {
                return &methods[i];
            }
        }
        return nullptr;
    }

    /**
     * @brief Resolve a method by index
     * @param index Method index (0-based)
     * @return Pointer to MethodDesc or nullptr if index out of bounds
     */
    FunctionDesc* get_method_function(u32 index) const {
        auto def = get_method_def(index);
        return def != nullptr ? def->ptr.ptr : nullptr;
    }
    
    /**
     * @brief Get all methods as a span/vector
     * @return Pointer to methods array and count
     */
    std::pair<MethodDef*, u32> get_methods() const {
        if (methods_offset == 0 || methods_count == 0) {
            return {nullptr, 0};
        }
        return {methods_offset.ptr, methods_count};
    }
    
    // ===================================================================
    // States
    // ===================================================================

    /**
     * @brief Resolve a state by index
     * @param index State index (0-based)
     * @return Pointer to StateDesc or nullptr if index out of bounds
     */
    StateDef* get_state_def(u32 index) const {
        if (states_offset == 0 || index >= states_count) {
            return nullptr;
        }
        return &states_offset.ptr[index];
    }
        /**
     * @brief Resolve a state by index
     * @param index State index (0-based)
     * @return Pointer to StateDesc or nullptr if index out of bounds
     */
    StateDesc* get_state(u32 index) const {
        if (states_offset == 0 || index >= states_count) {
            return nullptr;
        }
        StateDef* def = &states_offset.ptr[index];
        return def!=nullptr ? def->get_state() : nullptr;
    }

    /**
     * @brief Resolve a state by name
     * @param name State name to find
     * @return Pointer to StateDesc or nullptr if not found
     */
    StateDesc* resolve_state(StringId name) const {
        if (states_offset == 0 || states_count == 0) {
            return nullptr;
        }
        
        StateDef* defs = states_offset.ptr;
        
        for (u32 i = 0; i < states_count; i++) {
            if (defs[i].name == name) {
                return defs[i].get_state();
            }
        }
        return nullptr;
    }

    /**
     * @brief Get all states as a span/vector
     * @return Pointer to states array and count
     */
    std::pair<StateDef*, u32> get_states() const {
        if (states_offset == 0 || states_count == 0) {
            return {nullptr, 0};
        }
        return {states_offset.ptr, states_count};
    }

};

} // namespace carbon::files