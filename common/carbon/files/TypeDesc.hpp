#pragma once

#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/files/Definition.hpp"
#include "files/BinaryFile.hpp"
#include "files/FunctionDesc.hpp"
#include "files/StateDesc.hpp"
#include "type_system/Config.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

// Forward declarations
class Symbol;

using namespace carbon::lib;

namespace carbon::files {

enum class TypeFlags : uint32_t {
    None                    = 0x0000,
    IsReference             = 0x0001,
    IsLoadSigned            = 0x0002,
    IsConst                 = 0x0004,
    IsVolatile              = 0x0008,
    IsPacked                = 0x0010,
    IsAbstract              = 0x0020,
    IsSealed                = 0x0040,
    HasFinalizer            = 0x0080,
    HasStaticConstructor    = 0x0100,
    IsValueType             = 0x0200,
    IsEnum                  = 0x0400,
    IsInterface             = 0x0800,
    IsGeneric               = 0x1000
};
ENUM_FLAG_OPERATORS(TypeFlags);

/**
 * @brief Type header for type system metadata
 * 
 * Contains metadata about a type including its methods, states, and layout.
 */
struct TypeDesc  {
    Ptr<MethodDef> methods_offset;               // Offset to methods
    Ptr<StateDef> states_offset;                 // Offset to states
    uint32_t methods_count;                      // Number of methods
    uint32_t states_count;                       // Number of states
    StringId name;                               // Type name
    StringId parent_type_id;                     // Parent type name
    TypeFlags flags;                             // Type flags
    RegClass reg_class;                          // Preferred register class
    int load_size;                               // Size when loaded
    int in_memory_alignment;                     // Alignment in memory
    int inline_array_stride_alignment;           // Inline array stride alignment
    int inline_array_start_alignment;            // Inline array start alignment
    int offset;                                  // Offset within type
   
    /**
     * @brief Check if this is a reference type
     * @return true if reference type, false otherwise
     */
    inline bool is_reference() const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(TypeFlags::IsReference)) != 0;
    }
    
    /**
     * @brief Get load size
     * @return Load size in bytes
     */
    inline int get_load_size() const {
        return load_size;
    }
    
    /**
     * @brief Check if load is signed
     * @return true if signed load, false otherwise
     */
    inline bool get_load_signed() const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(TypeFlags::IsLoadSigned)) != 0;
    }
    
    /**
     * @brief Get size in memory
     * @return Size in memory in bytes
     */
    inline int get_size_in_memory() const {
        return in_memory_alignment;
    }
    
    /**
     * @brief Get preferred register class
     * @return Register class
     */
    inline RegClass get_preferred_reg_class() const {
        return reg_class;
    }
    
    /**
     * @brief Get offset within type
     * @return Offset in bytes
     */
    inline int get_offset() const {
        return offset;
    }
    
    /**
     * @brief Get in-memory alignment
     * @return Alignment in bytes
     */
    inline int get_in_memory_alignment() const {
        return in_memory_alignment;
    }
    
    /**
     * @brief Get inline array stride alignment
     * @return Stride alignment in bytes
     */
    inline int get_inline_array_stride_alignment() const {
        return inline_array_stride_alignment;
    }
    
    /**
     * @brief Get inline array start alignment
     * @return Start alignment in bytes
     */
    inline int get_inline_array_start_alignment() const {
        return inline_array_start_alignment;
    }
    
    /**
     * @brief Convert to simple string representation
     * @return Basic string representation
     */
    std::string to_string() const;
    
    /**
     * @brief Create detailed inspection string
     * @return Detailed formatted string for debugging
     */
    std::string inspect() const;

    /**
     * @brief Check if a specific flag is set
     * @param flag The flag to check
     * @return true if the flag is set, false otherwise
     */
    inline bool has_flag(TypeFlags flag) const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }
    
    /**
     * @brief Set a specific flag
     * @param flag The flag to set
     */
    inline void set_flag(TypeFlags flag) {
        flags = flags | flag;
    }
    
    /**
     * @brief Clear a specific flag
     * @param flag The flag to clear
     */
    inline void clear_flag(TypeFlags flag) {
        flags = flags & (~flag);
    }

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
        return def != nullptr ? def->data.ptr : nullptr;
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

} // end of namespace