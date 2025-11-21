#pragma once

#include "types.hpp"
#include "util/assert.h"
#include "util/log.h"
#include "util/crc32.h"  
#include <string>
#include <unordered_map>
#include <format>
#include <mutex>

namespace vm {

    // ============================================================================
    // StringId Type
    // ============================================================================

    using StringId = u32;

    // ============================================================================
    // StringId Utilities
    // ============================================================================

    namespace string_id {

        // ----------------------------------------------------------------------------
        // Creation Functions
        // ----------------------------------------------------------------------------

        inline StringId from_string(const std::string& str) {
            return util::compute_crc32(str);
        }

        inline StringId from_cstring(const char* str) {
            return util::compute_crc32(str);
        }

        inline StringId from_literal(const char* str, size_t len) {
            return util::compute_crc32(str, len);
        }

        // ----------------------------------------------------------------------------
        // Global String Table (for debugging)
        // ----------------------------------------------------------------------------

        class StringTable {
        public:
            static StringTable& instance() {
                static StringTable table;
                return table;
            }

            void register_string(const std::string& str, StringId sid) {
                std::lock_guard lock(mutex_);
                auto [it, inserted] = hash_to_string_.emplace(sid, str);

                if (!inserted && it->second != str) {
                    lg::error("StringId collision: '{}' and '{}' both hash to {:08x}",
                        it->second, str, sid);
                    ASSERT_MSG(false, "StringId collision detected");
                }

                if (inserted) {
                    string_to_hash_.emplace(str, sid);
                }
            }

            std::string get_string(StringId sid) const {
                std::lock_guard lock(mutex_);
                auto it = hash_to_string_.find(sid);
                if (it != hash_to_string_.end()) {
                    return it->second;
                }
                return std::format("<unknown:{:08x}>", sid);
            }

            const char* get_cstring(StringId sid) const {
                std::lock_guard lock(mutex_);
                auto it = hash_to_string_.find(sid);
                if (it != hash_to_string_.end()) {
                    return it->second.c_str();
                }
                return "<unknown>";
            }

            bool contains(StringId sid) const {
                std::lock_guard lock(mutex_);
                return hash_to_string_.contains(sid);
            }

            void clear() {
                std::lock_guard lock(mutex_);
                hash_to_string_.clear();
                string_to_hash_.clear();
            }

        private:
            mutable std::mutex mutex_;
            std::unordered_map<StringId, std::string> hash_to_string_;
            std::unordered_map<std::string, StringId> string_to_hash_;
        };

        // ----------------------------------------------------------------------------
        // Utility Functions
        // ----------------------------------------------------------------------------

        inline std::string to_string(StringId sid) {
            return StringTable::instance().get_string(sid);
        }

        inline const char* to_cstring(StringId sid) {
            return StringTable::instance().get_cstring(sid);
        }

        inline void register_string(const std::string& str, StringId sid) {
            StringTable::instance().register_string(str, sid);
        }

        inline bool is_registered(StringId sid) {
            return StringTable::instance().contains(sid);
        }

    } // namespace string_id

    // ============================================================================
    // Literal Operator for StringId
    // ============================================================================

    inline StringId operator"" _sid(const char* str, size_t len) {
        StringId sid = util::compute_crc32(str, len);
        string_id::register_string(std::string(str, len), sid);
        return sid;
    }

    /**
     * Convert the string to the string ID
     * @params str - const char* as c string value
     */
#define SID(str) str##_sid

    // ============================================================================
    // Utility Functions (global scope)
    // ============================================================================

    inline std::string string_id_to_string(StringId sid) {
        return string_id::to_string(sid);
    }

    inline std::ostream& operator<<(std::ostream& os, StringId sid) {
        return os << string_id_to_string(sid);
    }

} // namespace vm

