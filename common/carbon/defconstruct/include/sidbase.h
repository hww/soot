#pragma once
#include "base.h"
#include <memory>
#include <filesystem>
#include <expected>
#include <stdexcept>

namespace dconstruct {
    struct SIDBaseEntry {
        sid64 hash;
        u64 offset;
    };

    class SIDBase {

    private:
        u64 m_numEntries;
        std::unique_ptr<std::byte[]> m_sidbytes;
        SIDBaseEntry* m_entries;
        sid64 m_lowestSid;
        sid64 m_highestSid;

    public:
        //explicit SIDBase(const std::filesystem::path& path);
        
        SIDBase(const u64 num_entries, std::unique_ptr<std::byte[]>&& bytes, SIDBaseEntry* entries, const sid64 lowest, const sid64 highest) : 
        m_numEntries(num_entries), m_sidbytes(std::move(bytes)), m_entries(entries), m_lowestSid(lowest), m_highestSid(highest) {};

        [[nodiscard]] static std::expected<SIDBase, std::string> from_binary(const std::filesystem::path& path) noexcept;
        //[[nodiscard]] static SIDBase from_uc4_binary(const std::filesystem::path& path) noexcept;
        
        [[nodiscard]] const char* search(const sid64 hash) const noexcept;
        [[nodiscard]] bool sid_exists(const sid64 hash) const noexcept;
        u64 numEntries() { return m_numEntries; }
        SIDBaseEntry& operator[](size_t idx) {
            return m_entries[idx];
        }

        const SIDBaseEntry& operator[](size_t idx) const {
            return m_entries[idx];
        }

        const char* get_string(size_t offset) const {
            return reinterpret_cast<const char*>(m_sidbytes.get()) + offset;
        }

        const char* get_string_by_offset(size_t offset) {
            return reinterpret_cast<char*>(m_sidbytes.get()) + offset;
        }
    };
}

