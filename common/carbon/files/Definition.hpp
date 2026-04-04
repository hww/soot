#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/Ptr.hpp"

using namespace carbon::lib;
using namespace carbon::modules;

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
     * @brief Definition of a named entity in the FunctionDesc file
     *
     * Definitions can represent functions, global variables, constants,
     * or other named entities that are exported/imported between modules.
     */
    struct Definition {
        /** Unique name identifier within the module */
        StringId name;
        /** Type identifier (function, data, constant, etc.) */
        StringId type;
        /** Flags for the definition */
        SymbolFlags flags;
        u32 reserved;
        /** Pointer to the actual data or code for this definition */
        Ptr<u8> ptr;

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

        inline bool has_flag(SymbolFlags flag) const {
            return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
        }
        inline void set_flag(SymbolFlags flag) {
            flags |= flag;
        }
        inline void clear_flag(SymbolFlags flag) {
            flags &= flag;
        }
        /**
         * Every defition points to descriptor
         */
        void relocate_pointers(bool to_memory, intptr_t delta, Module* module);

        static void relocate_pointers_table(bool to_memory, intptr_t delta, Definition* definitions, size_t definitions_count, Module* module);
    };

     
} // end of namespace