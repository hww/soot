#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"

using namespace carbon::lib;

namespace carbon::files {

    /**
     * @brief Aligns size to 4-byte boundary
     * @param n Original size to align
     * @return Size aligned to 4 bytes
     *
     * This is critical for proper memory alignment in the FunctionDesc format
     * as many architectures require 4-byte aligned memory access for
     * optimal performance and correctness.
     */
    constexpr u32 align_size(u32 n) {
        return (n + 3) & ~3;
    }

    /* Errors */
    /**
     * @brief Exception class for FunctionDesc loading and validation errors
     *
     * Thrown when FunctionDesc format is invalid, corrupted, or incompatible
     * with the current runtime version.
     */
    class FunctionDescError : public std::exception {
    public:
        explicit FunctionDescError(const std::string& msg);
        const char* what() const noexcept override;
    private:
        std::string message;
    };

    /**
     * Definition flags
     */
    enum class SymbolFlags {
        None = 0,
        Local = 1 << 0,    // определен в этом модуле
        Import = 1 << 1,   // получен из другого модуля
        Export = 1 << 2    // доступен другим модулям
    };
    
    ENUM_FLAG_OPERATORS(SymbolFlags);

    /**
     * Method flags
     */
    enum class MethodFlags {
        None = 0,
        Virtual = 1 << 0,    // виртуальный метод
        Override = 1 << 1,   // переопределенный метод
    };
    
    ENUM_FLAG_OPERATORS(MethodFlags);



    /**
     * @brief Source code location information for debugging
     *
     * Maps FunctionDesc instructions back to original source code locations.
     * This enables meaningful stack traces and debugging information.
     */
    struct SourceLocation {
        /** First instruction index */
        u32 start;
        /** Character offset in the original source file */
        u32 offset;
        /** Line number in the original source file (1-based) */
        u32 line;
        /** File identifier where this code originated */
        StringId file;

        /**
         * @brief Convert to human-readable string representation
         * @return Formatted string with location details
         */
        std::string to_string() const;
    };
    
    
    /**
     * @brief Base class for entities that can be relocated in memory
     *
     * Used during module loading and dynamic linking to adjust pointers
     * when code is moved in memory.
     */
    struct Descriptor {
        /** Size of this descriptor in bytes */
        u32 desc_size;
        /** Name of descriptor as crc32 */
        StringId name;
        /**
         * @brief Adjust all internal pointers by a delta value
         * @param delta The amount to add to all pointers
         *
         * This is essential for position-independent code and dynamic
         * loading where the base address may change.
         */
        virtual void relocate_pointers(intptr_t delta) = 0;

        virtual ~Descriptor() = default;
    };
} // end of namespace