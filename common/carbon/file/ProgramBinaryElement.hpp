#pragma once

#include "CommonTypes.hpp"
#include "DCHeader.hpp"
#include "DCScript.hpp"
#include "lib/ByteUtils.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace carbon {

    struct function;
    struct global_state;

    /// @brief Serialised representation of one entry (function / struct / ...) in a DC file.
    /// @details Holds raw bytes, a parallel relocation bitmap (one bit per 8-byte slot),
    ///          and the DCEntry header that will be written into the entry table.
    struct ProgramBinaryElement {

        explicit ProgramBinaryElement(const u64 size) noexcept;

        ProgramBinaryElement(const ProgramBinaryElement &) = delete;
        ProgramBinaryElement &operator=(const ProgramBinaryElement &) = delete;

        ProgramBinaryElement(ProgramBinaryElement &&other) noexcept
            : m_entry(std::move(other.m_entry)), m_rawData(std::move(other.m_rawData)),
              m_stringOffsets(std::move(other.m_stringOffsets)),
              m_relocTable(std::move(other.m_relocTable)), m_byteOffset(other.m_byteOffset),
              m_bitOffset(other.m_bitOffset) {
            other.m_entry.m_entryPtr = nullptr;
            other.m_byteOffset = 0;
            other.m_bitOffset = 0;
        }

        /// @brief Append a POD value and its relocation bits.
        /// @details Copies the raw bytes of `data` into m_rawData, then appends
        ///          the relocation bits. The last bit in `b` covers
        ///          (sizeof(T) / 8) % 8 slots; all preceding bits cover 8 slots each.
        /// @example push_bytes(header, 0, 1, 1) — first 8-byte slot is not relocated,
        ///          second and third are.
        template <typename T, typename... bits> void push_bytes(const T &data, bits... b) noexcept {
            static_assert(std::is_trivially_copyable_v<T>,
                          "push_bytes requires a trivially-copyable T");

            const std::byte *p = reinterpret_cast<const std::byte *>(std::addressof(data));
            m_rawData.insert(m_rawData.end(), p, p + sizeof(T));

            const std::vector<u8> bits_list = {static_cast<u8>(b)...};
            for (u32 i = 0; i + 1 < bits_list.size(); ++i) {
                insert_into_reloctable(bits_list[i], 8);
            }
            if (!bits_list.empty()) {
                insert_into_reloctable(bits_list.back(), (sizeof(T) / 8) % 8);
            }
        }

        /// @brief Append a raw byte blob and a single relocation bit applied to every slot.
        void push_blob(const void *data, size_t size, u8 relocation_bit = 0) noexcept;

        /// @brief Sanity check: raw bytes / 8 must equal the number of relocation bits.
        void check_size() const;

        /// @brief Append `num_bits` relocation bits, all set to `bits` (0 or 1).
        void insert_into_reloctable(u8 bits, u64 num_bits) noexcept;

        /// @brief Remember the current end-of-buffer as a string-table reference.
        void insert_string_offset() noexcept;

        /// @brief Remember (current end-of-buffer + offset) as a string-table reference.
        void insert_string_offset(u64 offset) noexcept;

        /// @brief Add `offset` to every relocated pointer in m_rawData.
        void adjust_offsets(u64 offset) noexcept;

        /// @brief Return the size of the raw byte buffer.
        [[nodiscard]] size_t size() const noexcept { return m_rawData.size(); }

        /// @brief Return true if the raw byte buffer is empty.
        [[nodiscard]] bool is_empty() const noexcept { return m_rawData.empty(); }

        /// @brief Dump a hex view of the raw data plus the relocation bitmap.
        void dump(const std::string &title = "", size_t max_len = 256);

        /// @brief Allocate a fresh 64-byte-aligned copy of m_rawData.
        [[nodiscard]] byte_uptr to_byte_uptr() const;

        DCEntry                m_entry;    ///< header that will be written into the entry table
        std::vector<std::byte> m_rawData;  ///< serialised payload
        std::vector<u64>  m_stringOffsets; ///< offsets in m_rawData that hold string-table indices
        std::vector<bool> m_relocTable;    ///< one bit per 8-byte slot in m_rawData

        u64 m_byteOffset = 0; ///< running byte cursor (used by insert_into_reloctable)
        u8  m_bitOffset = 0;  ///< running bit cursor within m_byteOffset
    };

} // namespace carbon