#pragma once

#include <string>
#include <cstdint>
#include <ostream>
#include "common/util/Crc32.hpp"
#include "common/CommonTypes.hpp"

namespace runtime::lib {


    typedef uint32_t StringId;

    namespace string_id {
        

        const StringId NONE = SID("none");

        // ============================================================================
        // Runtime Interface
        // ============================================================================

        /**
         * Loads string table from file (call once at startup)
         * Format: "HEXCRC32 string"
         * Throws: std::runtime_error on file error
         */
        void load_table(const std::string& filename);

        /**
         * Saves current string table to file
         * Format: "HEXCRC32 string" sorted by string value
         * Throws: std::runtime_error on file error
         */
        void save_table(const std::string& filename);
        /**
         * Registers a string and returns its StringId
         * If string already exists, returns existing ID
         * If different string with same CRC32 exists, throws std::runtime_error
         */
        StringId register_string(const std::string& str);

        /**
         * Registers a C-string and returns its StringId
         */
        StringId register_string(const char* str);


        /**
         * Converts StringId back to string for debugging
         * For unknown IDs returns formatted string with hex value
         */
        std::string to_string(StringId id);

        /**
         * Fast conversion to C-string (for logging)
         * For unknown IDs returns "<unknown>"
         */
        const char* to_cstring(StringId id);

        /**
         * Checks if string table is loaded
         */
        bool is_table_loaded();

        /**
         * Gets number of strings in table
         */
        size_t get_string_count();

        /**
         * Clears string table (mainly for tests)
         */
        void clear_table();

        /**
         * Check the table
         */
        std::string inspect();

        static void initialize() {
            register_string("null");
            register_string("none");
            register_string("number");    // Base types
            register_string("integer");   // number::integer
            register_string("sinteger");  // integer::sinteger
            register_string("i64");       // integer::sinteger::s64
            register_string("i32");       // integer::sinteger::s32
            register_string("i16");       // integer::sinteger::s16
            register_string("i8");        // integer::sinteger::s8
            register_string("uinteger");  // integer::uinteger
            register_string("u64");       // integer::uinteger::i64
            register_string("u32");       // integer::uinteger::i32
            register_string("u16");       // integer::uinteger::i16
            register_string("u8");        // integer::uinteger::i8
            register_string("float");     // number::float
            register_string("bool");
            register_string("string_id"); // integer::sinteger::string_id
            register_string("native");
            register_string("string");
            register_string("function");
        }
    } // namespace string_id

    // ============================================================================
    // Debug Utilities  
    // ============================================================================

    /**
     * Convert StringId to string (global function)
     */
    inline std::string to_string(StringId sid) {
        return string_id::to_string(sid);
    }

    /**
     * Stream output for StringId
     */
    inline std::ostream& operator<<(std::ostream& os, StringId sid) {
        return os << to_string(sid);
    }

} // namespace vm