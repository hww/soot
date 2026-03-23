#pragma once

#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"
#include "files/Base.hpp"
#include "type_system/Config.hpp"
#include <cstdint>
#include <string>

// Forward declarations
class Symbol;

using namespace runtime::lib;

namespace runtime::files {

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
struct TypeDesc : public Descriptor {
    int64_t methods_offset;                      // Offset to methods
    int64_t states_offset;                       // Offset to states
    uint32_t methods_count;                      // Number of methods
    uint32_t states_count;                       // Number of states
    StringId name;                                 // Type name
    StringId parent_type_id;                       // Parent type name
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

    void relocate_pointers(intptr_t delta);
};

} // end of namespace