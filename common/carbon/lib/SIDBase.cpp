#include "SIDBase.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace carbon {

    [[nodiscard]] std::expected<SIDBase, std::string>
    SIDBase::from_binary(const std::filesystem::path &path) noexcept {
        std::ifstream sidfile(path, std::ios::binary);
        if (!sidfile.is_open()) {
            return std::unexpected{"couldn't open sidbase at path '" + path.string() + "'"};
        }

        const std::size_t fsize = std::filesystem::file_size(path);
        if (fsize < sizeof(u64)) {
            return std::unexpected{"sidbase file too small: " + path.string()};
        }

        std::byte *temp_buffer = new std::byte[fsize];
        sidfile.read(reinterpret_cast<char *>(temp_buffer), static_cast<std::streamsize>(fsize));
        if (!sidfile) {
            delete[] temp_buffer;
            return std::unexpected{"failed to read sidbase: " + path.string()};
        }

        const u64 num_entries = *reinterpret_cast<const u64 *>(temp_buffer);
        if (num_entries == 0) {
            delete[] temp_buffer;
            return std::unexpected{"sidbase has zero entries: " + path.string()};
        }

        const u64 entries_bytes = num_entries * sizeof(SIDBaseEntry);
        if (sizeof(u64) + entries_bytes > fsize) {
            delete[] temp_buffer;
            return std::unexpected{"sidbase table exceeds file size: " + path.string()};
        }

        auto entries = reinterpret_cast<SIDBaseEntry *>(temp_buffer + sizeof(u64));
        auto bytes = std::unique_ptr<std::byte[]>(temp_buffer);

        const sid64 lowest = entries[0].hash;
        const sid64 highest = entries[num_entries - 1].hash;

        return SIDBase{num_entries, std::move(bytes), entries, lowest, highest};
    }

    [[nodiscard]] const char *SIDBase::search(const sid64 hash) const noexcept {
        u64 low = 0;
        u64 high = m_numEntries - 1;
        u64 mid = 0;
        while (low <= high) {
            mid = low + (high - low) / 2;
            const SIDBaseEntry *current = m_entries + mid;
            if (current->hash == hash) [[unlikely]] { return get_string(current->offset); }
            if (current->hash < hash) {
                low = mid + 1;
            } else {
                if (mid == 0) break;
                high = mid - 1;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool SIDBase::sid_exists(const sid64 hash) const noexcept {
        return search(hash) != nullptr;
    }

} // namespace carbon