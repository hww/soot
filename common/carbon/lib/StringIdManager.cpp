// StringIdManager.cpp
#include "common/carbon/lib/StringIdManager.hpp"

#include "CommonTypes.hpp"
#include "common/carbon/lib/SIDBase.hpp"
#include "common/util/Log.hpp"
#include "common/util/StringIdHash.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

namespace carbon {

    // ===========================================================================
    // Registration
    // ===========================================================================

    sid64 StringIdManager::register_string(const std::string &str) {
        const sid64 hash = util::ToStringId64(str);

        std::unique_lock<std::shared_mutex> lock(mutex_);

        const auto it = reverse_lookup_.find(hash);
        if (it != reverse_lookup_.end()) {
            if (it->second != str) {
                lg::error(
                    "StringIdManager: CRC32 collision detected! ID 0x{:08X} for both '{}' and '{}'",
                    hash, it->second, str);
            }
            return hash;
        }

        reverse_lookup_[hash] = str;
        lg::debug("StringIdManager: registered '{}' as ID 0x{:016X}", str, hash);
        return hash;
    }

    sid64 StringIdManager::register_string(const char *str) {
        if (!str || *str == '\0') { return 0; }
        return register_string(std::string(str));
    }

    sid32 StringIdManager::register_string32(const std::string &str) {
        return static_cast<sid32>(register_string(str));
    }

    sid32 StringIdManager::register_string32(const char *str) {
        return static_cast<sid32>(register_string(str));
    }

    // ===========================================================================
    // Lookup
    // ===========================================================================

    std::string StringIdManager::get_string(u64 id) const {
        if (id == 0) { return ""; }

        std::shared_lock<std::shared_mutex> lock(mutex_);

        const auto it = reverse_lookup_.find(id);
        if (it != reverse_lookup_.end()) { return it->second; }
        return fmt::format("<unknown:0x{:016X}>", id);
    }

    const char *StringIdManager::get_cstring(u64 id) const {
        // Thread-local buffer so that callers can hold the pointer until the next
        // call on the same thread. NOT safe to store across threads.
        static thread_local std::string buffer;
        buffer = get_string(id);
        return buffer.c_str();
    }

    bool StringIdManager::has_string(u64 id) const {
        if (id == 0) { return false; }

        std::shared_lock<std::shared_mutex> lock(mutex_);
        return reverse_lookup_.find(id) != reverse_lookup_.end();
    }

    void StringIdManager::debug_dump() const {
        std::shared_lock lock(mutex_);
        lg::info("=== StringIdManager Dump ({} entries) ===", reverse_lookup_.size());
        for (const auto &[id, str] : reverse_lookup_) { lg::info("  0x{:016X} -> '{}'", id, str); }
    }

    // ===========================================================================
    // Serialization (text format: "HEX8 name")
    // ===========================================================================

    bool StringIdManager::save_table(const std::string &filename) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::ofstream file(filename);
        if (!file.is_open()) {
            lg::error("StringIdManager: cannot open file '{}' for writing", filename);
            return false;
        }

        for (const auto &[id, str] : reverse_lookup_) {
            file << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id << " "
                 << str << "\n";

            if (!file.good()) {
                lg::error("StringIdManager: error writing to file '{}'", filename);
                return false;
            }
        }

        lg::debug("StringIdManager: saved {} entries to '{}'", reverse_lookup_.size(), filename);
        return true;
    }

    bool StringIdManager::load_table(const std::string &filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            lg::warn("StringIdManager: cannot open file '{}' for reading", filename);
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        reverse_lookup_.clear();

        std::string line;
        int         line_num = 0;
        int         valid_entries = 0;

        while (std::getline(file, line)) {
            ++line_num;

            // Trim leading/trailing whitespace.
            const size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) {
                continue; // empty line
            }
            const size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);

            // Split on the first space or tab.
            const size_t space_pos = line.find_first_of(" \t");
            if (space_pos == std::string::npos) {
                lg::error("StringIdManager: invalid format at line {}: '{}'", line_num, line);
                continue;
            }

            const std::string hex_str = line.substr(0, space_pos);
            std::string       name = line.substr(space_pos + 1);

            // Trim the name.
            const size_t name_start = name.find_first_not_of(" \t\r\n");
            if (name_start != std::string::npos) {
                const size_t name_end = name.find_last_not_of(" \t\r\n");
                name = name.substr(name_start, name_end - name_start + 1);
            } else {
                name.clear();
            }

            if (hex_str.empty()) {
                lg::error("StringIdManager: empty hex value at line {}", line_num);
                continue;
            }

            bool valid_hex = true;
            for (char c : hex_str) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) {
                    valid_hex = false;
                    break;
                }
            }
            if (!valid_hex) {
                lg::error("StringIdManager: invalid hex value at line {}: '{}'", line_num, hex_str);
                continue;
            }

            if (name.empty()) {
                lg::error("StringIdManager: empty name at line {}", line_num);
                continue;
            }

            u32               id = 0;
            std::stringstream ss;
            ss << std::hex << hex_str;
            ss >> id;

            reverse_lookup_[id] = name;
            ++valid_entries;
        }

        lg::debug("StringIdManager: loaded {} valid entries from '{}' (total lines: {})",
                  valid_entries, filename, line_num);
        return valid_entries > 0;
    }

    // ===========================================================================
    // Serialization (dconstruct binary sidbase format)
    // ===========================================================================

    bool StringIdManager::save_dconstruct_sidbase(const std::string &filename) const {
        std::shared_lock lock(mutex_);

        // Build entries and a string pool.
        std::vector<SIDBaseEntry> entries;
        entries.reserve(reverse_lookup_.size());

        std::vector<char> string_pool;
        for (const auto &[id, name] : reverse_lookup_) {
            SIDBaseEntry entry;
            entry.hash = id;
            entry.offset = string_pool.size();
            entries.push_back(entry);

            string_pool.insert(string_pool.end(), name.begin(), name.end());
            string_pool.push_back('\0');
        }

        // Sort by hash for binary search in the runtime.
        std::sort(entries.begin(), entries.end(),
                  [](const auto &a, const auto &b) { return a.hash < b.hash; });

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            lg::error("StringIdManager: cannot open '{}' for writing", filename);
            return false;
        }

        const u64 num_entries = entries.size();
        file.write(reinterpret_cast<const char *>(&num_entries), sizeof(num_entries));
        file.write(reinterpret_cast<const char *>(entries.data()),
                   static_cast<std::streamsize>(entries.size() * sizeof(SIDBaseEntry)));
        file.write(string_pool.data(), static_cast<std::streamsize>(string_pool.size()));

        return file.good();
    }

    bool StringIdManager::load_dconstruct_sidbase(const std::string &filename) {
        auto result = SIDBase::from_binary(filename);
        if (!result) { return false; }

        auto &sidbase = *result;

        std::unique_lock lock(mutex_);
        reverse_lookup_.clear();

        for (u64 i = 0; i < sidbase.numEntries(); ++i) {
            const auto &entry = sidbase[i];
            const char *name = sidbase.get_string_by_offset(entry.offset);

            // Truncate 64-bit hash to 32 bits (the game uses 32-bit SIDs internally).
            const u32 id32 = static_cast<u32>(entry.hash);
            reverse_lookup_[id32] = name;
        }

        return true;
    }

    // ===========================================================================
    // Inspection
    // ===========================================================================

    std::string StringIdManager::inspect() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::ostringstream oss;
        oss << "StringIdManager: " << reverse_lookup_.size() << " entries\n";

        constexpr size_t max_to_show = 20;
        size_t           shown = 0;

        for (const auto &[id, str] : reverse_lookup_) {
            if (shown >= max_to_show) {
                oss << "  ... and " << (reverse_lookup_.size() - shown) << " more\n";
                break;
            }
            oss << "  0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id
                << std::dec << " = \"" << str << "\"\n";
            ++shown;
        }

        return oss.str();
    }

    size_t StringIdManager::size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return reverse_lookup_.size();
    }

    void StringIdManager::clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        reverse_lookup_.clear();
        lg::debug("StringIdManager: cleared all entries");
    }

} // namespace carbon