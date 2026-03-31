#pragma once
#include "common/CommonTypes.hpp"
#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "lib/Ptr.hpp"

#include <cstdint>
#include <string>

using namespace carbon::lib;
using namespace carbon::modules;
using namespace carbon::vm;

namespace carbon::files {

    /**
     * @brief Complete FunctionDesc representation with code, data, and debug info
     *
     * This is the core structure representing executable code in the VM.
     * It contains the actual FunctionDesc instructions, constant data, and
     * debugging information mapping back to source code.
     */
    struct FunctionDesc {
        /** Number of instructions in the code section */
        u32 code_count;
        /** Size of constant data in bytes */
        u32 data_size;
        /** Number of debug information entries */
        u32 debug_count;
        /** For allign */
        u32 reserved;
        /** Pointer to the instruction stream */
        Ptr<Instruction> code_ptr;
        /** Pointer to constant data section */
        Ptr<u8> data_ptr;
        /** Pointer to debug information array */
        Ptr<SourceLocation> debug_ptr;
        /** Module that owns this FunctionDesc (set during linking) */
        Module* owner_module;

        FunctionDesc();

        /**
         * @brief Get pointer to the code section
         * @return Pointer to instructions or nullptr if no code
         */
        Instruction* get_code_ptr() const { return code_ptr.get(); }

        /**
         * @brief Get pointer to the data section
         * @return Pointer to constant data or nullptr if no data
         */
        u8* get_data_ptr() const;

        /**
         * @brief Get pointer to debug information
         * @return Pointer to debug info or nullptr if no debug data
         */
        SourceLocation* get_debug_info() const;

        /**
         * @brief Find source location for a specific instruction
         * @param instruction_ip The instruction pointer/offset to lookup
         * @return Source location info or empty location if not found
         *
         * This enables mapping runtime errors back to source code
         * locations for better debugging experience.
         */
        SourceLocation find_source_location(u32 instruction_ip) const;

        /**
         * @brief Check if debug information is available
         * @return true if debug information is present
         */
        bool has_debug_info() const;

        /**
         * @brief Relocate pointers for memory management
         * @param to_memory Whether to relocate for memory allocation
         * @param delta The offset to adjust pointers by
         */
        void relocate_pointers(bool to_memory, intptr_t delta, Module* owner);

        /**
         * @brief Create detailed inspection string
         * @return Formatted string with complete FunctionDesc information
         */
        std::string inspect() const;

    };


} // end of namespace