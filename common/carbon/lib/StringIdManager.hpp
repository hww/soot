// StringIdManager.hpp
#pragma once

#include "CommonTypes.hpp"

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace carbon {

    /// @brief Process-wide registry that maps SID64 values to their original strings.
    /// @details Used both for hashing strings (register_string) and for reverse
    ///          lookup during disassembly / logging (get_string). Thread-safe:
    ///          all public methods take a shared or unique lock on the internal
    ///          mutex. The manager is a singleton; call instance() to access it.
    class StringIdManager {
    public:
        /// @return the process-wide singleton instance.
        static StringIdManager &instance() {
            static StringIdManager inst;
            return inst;
        }

        StringIdManager(const StringIdManager &) = delete;
        StringIdManager &operator=(const StringIdManager &) = delete;

        // -----------------------------------------------------------------------
        // Registration
        // -----------------------------------------------------------------------

        /// @brief Compute the SID64 of `str` and remember it for reverse lookup.
        /// @details Logs an error if a different string already maps to the same hash.
        sid64 register_string(const std::string &str);
        sid64 register_string(const char *str);

        /// @brief Same as register_string, but returns a truncated 32-bit SID.
        sid32 register_string32(const std::string &str);
        sid32 register_string32(const char *str);

        // -----------------------------------------------------------------------
        // Lookup
        // -----------------------------------------------------------------------

        /// @return the string registered for `id`, or "<unknown:0x...>" if not found.
        std::string get_string(u64 id) const;

        /// @return a thread-local C-string for `id`; valid until the next call on the same thread.
        const char *get_cstring(u64 id) const;

        /// @return true if `id` has a registered string.
        bool has_string(u64 id) const;

        // -----------------------------------------------------------------------
        // Serialization (text format)
        // -----------------------------------------------------------------------

        /// @brief Write all entries as "HEX8 name\n" lines to `filename`.
        bool save_table(const std::string &filename) const;

        /// @brief Replace the internal table with entries read from `filename`.
        bool load_table(const std::string &filename);

        // -----------------------------------------------------------------------
        // Serialization (dconstruct binary sidbase format)
        // -----------------------------------------------------------------------

        /// @brief Load a sidbase.bin in the dconstruct binary format.
        bool load_dconstruct_sidbase(const std::string &filename);

        /// @brief Save the current table as a sidbase.bin in the dconstruct binary format.
        bool save_dconstruct_sidbase(const std::string &filename) const;

        // -----------------------------------------------------------------------
        // Inspection
        // -----------------------------------------------------------------------

        /// @return a human-readable summary of the table (first 20 entries + count).
        std::string inspect() const;

        /// @return the number of registered entries.
        size_t size() const;

        // -----------------------------------------------------------------------
        // Iteration
        // -----------------------------------------------------------------------

        using const_iterator = std::unordered_map<u64, std::string>::const_iterator;
        const_iterator begin() const { return reverse_lookup_.begin(); }
        const_iterator end() const { return reverse_lookup_.end(); }

        // -----------------------------------------------------------------------
        // Cleanup
        // -----------------------------------------------------------------------

        /// @brief Drop all registered entries.
        void clear();

    private:
        StringIdManager() {
            // register_known_sids is a static method that writes
            // through instance(). Since instance() has already
            // returned this object, the recursive call is harmless.
            register_known_sids();
        }

        /// @brief Register the well-known SID strings used by the DC
        ///        file format: type names (script-lambda, ss-type, ...),
        ///        SsDeclaration types (int32, float, ...), DCEntry
        ///        types (map, vector3, ...), BlockType names, and
        ///        common SIDs (int, object, structure, ...).
        ///
        /// @details Called once by the static initializer of the
        ///          StringIdManager singleton, so every process that
        ///          uses the manager has these names available for
        ///          reverse lookup. Without it, any SID computed by the
        ///          compile-time SID() macro (which does not register
        ///          anything) would print as "<unknown:0x...>".
        ///
        ///          The list is canonical: it is the single source of
        ///          truth for the human-readable form of every built-in
        ///          SID. If you add a new one, add it here.
        void register_known_sids();

        /// @brief Print all entries to the log (debug only).
        void debug_dump() const;

        mutable std::shared_mutex              mutex_;          ///< guards reverse_lookup_
        std::unordered_map<sid64, std::string> reverse_lookup_; ///< SID64 -> original string
    };

} // namespace carbon