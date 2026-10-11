#pragma once

#include "CommonTypes.hpp"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace carbon {

    // On-disk layout of a sidbase.bin file:
    //
    //   [0..8)                  : u64 numEntries
    //   [8..8+numEntries*16)    : SIDBaseEntry[numEntries]  (sorted by hash)
    //   [8+numEntries*16..)     : string pool, null-terminated strings
    //
    // Each SIDBaseEntry stores `offset` from the START OF THE FILE,
    // not from the start of the string pool. This is what
    // SIDBase::get_string expects.
    struct SIDBaseEntry {
        sid64 hash;
        u64   offset; // from the start of the file
    };

    // Read-only view of a sidbase.bin file.
    //
    // SIDBase is a FORMAT reader, not a container. It does not own
    // the entries in a mutable sense, it cannot insert or erase, and
    // it has no to_binary(). Writing a sidbase is the responsibility
    // of whoever owns the data (StringIdManager), because only they
    // know the correct layout and offsets.
    class SIDBase {
    private:
        u64                          m_numEntries;
        std::unique_ptr<std::byte[]> m_sidbytes; // whole file in memory
        SIDBaseEntry                *m_entries;  // = m_sidbytes + 8
        sid64                        m_lowestSid;
        sid64                        m_highestSid;

    public:
        SIDBase(const u64 num_entries, std::unique_ptr<std::byte[]> &&bytes, SIDBaseEntry *entries,
                const sid64 lowest, const sid64 highest)
            : m_numEntries(num_entries), m_sidbytes(std::move(bytes)), m_entries(entries),
              m_lowestSid(lowest), m_highestSid(highest) {}

        // ---- Reading ----
        [[nodiscard]] static std::expected<SIDBase, std::string>
        from_binary(const std::filesystem::path &path) noexcept;

        // ---- Lookup ----
        [[nodiscard]] const char *search(const sid64 hash) const noexcept;
        [[nodiscard]] bool        sid_exists(const sid64 hash) const noexcept;

        // ---- Access ----
        [[nodiscard]] u64 numEntries() const noexcept { return m_numEntries; }

        [[nodiscard]] SIDBaseEntry &operator[](std::size_t idx) noexcept { return m_entries[idx]; }
        [[nodiscard]] const SIDBaseEntry &operator[](std::size_t idx) const noexcept {
            return m_entries[idx];
        }

        // offset is from the START OF THE FILE.
        [[nodiscard]] const char *get_string(std::size_t offset) const noexcept {
            return reinterpret_cast<const char *>(m_sidbytes.get()) + offset;
        }

        // Convenience overload used by StringIdManager.
        [[nodiscard]] const char *get_string(const SIDBaseEntry &e) const noexcept {
            return get_string(e.offset);
        }

        // Alias kept for existing callers.
        [[nodiscard]] const char *get_string_by_offset(std::size_t offset) const noexcept {
            return get_string(offset);
        }

        [[nodiscard]] sid64 lowest_sid() const noexcept { return m_lowestSid; }
        [[nodiscard]] sid64 highest_sid() const noexcept { return m_highestSid; }
    };

} // namespace carbon