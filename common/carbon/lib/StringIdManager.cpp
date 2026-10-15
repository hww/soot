// StringIdManager.cpp
#include "carbon/lib/StringIdManager.hpp"

#include "CommonTypes.hpp"
#include "carbon/lib/SIDBase.hpp"
#include "util/Log.hpp"
#include "util/StringIdHash.hpp"

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

        // Sort by hash so the result can be binary-searched by SIDBase.
        std::vector<std::pair<sid64, std::string>> sorted;
        sorted.reserve(reverse_lookup_.size());
        for (const auto &[id, name] : reverse_lookup_) { sorted.emplace_back(id, name); }
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });

        // Layout: [u64 count][SIDBaseEntry x N][string pool]
        //
        // IMPORTANT: each entry's offset is from the START OF THE FILE,
        // not from the start of the string pool.
        const u64 header_size = sizeof(u64);
        const u64 entries_size = sorted.size() * sizeof(SIDBaseEntry);
        u64       current_offset = header_size + entries_size;

        std::vector<char>         string_pool;
        std::vector<SIDBaseEntry> entries;
        entries.reserve(sorted.size());

        u64 total_string_bytes = 0;
        for (const auto &[id, name] : sorted) total_string_bytes += name.size() + 1;
        string_pool.reserve(total_string_bytes);

        for (const auto &[id, name] : sorted) {
            SIDBaseEntry entry;
            entry.hash = id;
            entry.offset = current_offset; // <-- FROM START OF FILE
            entries.push_back(entry);

            string_pool.insert(string_pool.end(), name.begin(), name.end());
            string_pool.push_back('\0');
            current_offset += name.size() + 1;
        }

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

        lg::debug("StringIdManager: saved {} entries to '{}'", entries.size(), filename);
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

            // Keep the FULL 64-bit hash. Do not truncate to u32.
            if (name == nullptr || *name == '\0') {
                // Skip entries whose offset does not point at a real string.
                // This catches sidbases written by older, buggy versions
                // (offset-from-pool instead of offset-from-file).
                lg::warn("StringIdManager: entry {} has empty string (offset {}), skipping", i,
                         entry.offset);
                continue;
            }
            reverse_lookup_[entry.hash] = name;
        }

        lg::debug("StringIdManager: loaded {} entries from '{}'", sidbase.numEntries(), filename);
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

    void StringIdManager::register_known_sids() {
        static bool done = false;
        if (done) { return; }
        done = true;

 

        // ---- Well-known SIDs used across the DC format ----
        //
        // These SIDs are computed at compile time by the SID() macro,
        // which does NOT register the string. Any place that resolves
        // a SID back to text (the inspector, the disassembler, runtime
        // logs) would otherwise print "<unknown:0x...>". Registering
        // them here gives them a canonical text form.
        //
        // The constants are defined in DCScript.hpp; the names below
        // MUST match the strings used inside SID("...") there. If you
        // rename one, rename both.
        register_string("script-lambda"); // SCRIPT_LAMBDA_SID
        register_string("array");         // ARRAY_SID
        register_string("global");        // GLOBAL_SID
        register_string("function");      // FUNCTION_SID
        register_string("ss-type");       // SS_TYPE_SID

        // ---- SsDeclaration type names ----
        register_string("boolean");
        register_string("int32");
        register_string("uint64");
        register_string("float");
        register_string("timer");
        register_string("bound-frame");
        register_string("symbol");
        register_string("string");
        register_string("vector");
        register_string("quat");
        register_string("point");

        // ---- DCEntry type names ----
        register_string("map");
        register_string("map-32");
        register_string("vector3");
        register_string("vector4");
        register_string("transform");
        register_string("matrix");
        register_string("quaternion");
        register_string("locator");
        register_string("color");

        // ---- BlockType names (SsOnBlock.m_blockType) ----
        register_string("start");
        register_string("end");
        register_string("event");
        register_string("update");
        register_string("virtual");
        register_string("code");
        register_string("exit");
        register_string("post");

        // ---- Common SIDs ----
        register_string("none");
        register_string("unknown");
        register_string("unnamed");
        register_string("int");
        register_string("object");
        register_string("structure");
        register_string("basic");
        register_string("type");
        register_string("null");
        register_string("cvector");
    }
} // namespace carbon