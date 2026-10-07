// FunctionNode.cpp

#pragma once

#include "CommonTypes.hpp"
#include "DCHeader.hpp"
#include "DCScript.hpp"
#include "lib/ByteUtils.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace carbon {

    struct function;
    struct global_state;
    // =========================================================================
    // Struct layout metadata
    // =========================================================================
    // Serialised alongside a data-struct payload so that tools (like
    // BinaryFileInspector) can decode the payload WITHOUT consulting TypeSystem.
    // TypeSystem is a compiler-side concept; libcarbon must not depend on it.

    /// @brief One field of a structure.
    struct StructFieldInfo {
        std::string name;       ///< field name, e.g. "x"
        std::string type_name;  ///< type as written, e.g. "int"
        u32         offset;     ///< byte offset within the struct
        u32         size;       ///< size in bytes (arrays: total size)
        bool        is_inline;  ///< true if this field is an inline struct
        bool        is_array;   ///< true if this field is an array
        int         array_size; ///< number of elements (0 if not an array)
    };

    /// @brief Layout of one data-struct, attached to its ProgramBinaryElement.
    struct StructLayoutInfo {
        std::string                  type_name;  ///< e.g. "vec"
        u32                          total_size; ///< total size in bytes
        std::vector<StructFieldInfo> fields;
    };

    /// @brief Serialised representation of one entry (function / struct / ...) in a DC file.
    /// @details Holds raw bytes, a parallel relocation bitmap (one bit per 8-byte slot),
    ///          and the DCEntry header that will be written into the entry table.
    ///
    ///          Invariant: m_relocTable.size() == (m_rawData.size() + 7) / 8.
    ///          This is checked by check_size() and MUST hold after every mutation.
    struct ProgramBinaryElement {

        explicit ProgramBinaryElement(const u64 size) noexcept;

        ProgramBinaryElement(const ProgramBinaryElement &) = delete;
        ProgramBinaryElement &operator=(const ProgramBinaryElement &) = delete;

        ProgramBinaryElement(ProgramBinaryElement &&other) noexcept
            : m_entry(std::move(other.m_entry)), m_rawData(std::move(other.m_rawData)),
              m_stringOffsets(std::move(other.m_stringOffsets)),
              m_relocTable(std::move(other.m_relocTable)),
              m_structLayout(std::move(other.m_structLayout)), // <-- ADD THIS
              m_byteOffset(other.m_byteOffset), m_bitOffset(other.m_bitOffset) {
            other.m_entry.m_entryPtr = nullptr;
            other.m_byteOffset = 0;
            other.m_bitOffset = 0;
        }
        /// @brief Append a POD value and its relocation bits.
        /// @details Copies the raw bytes of `data` into m_rawData, then appends
        ///          one relocation bit per newly-introduced 8-byte slot.
        ///
        ///          `bits` is an initializer list of 0/1 values — one per slot,
        ///          left to right. If fewer bits than slots are supplied, the
        ///          remaining slots default to 0 (not relocatable).
        ///
        ///          Using an initializer list (rather than a variadic template)
        ///          makes the API impossible to misuse: passing a single integer
        ///          like 0b1000 is a compile error, not a silent "one bit" call.
        ///
        /// @example push_bytes(header, {0, 0, 0, 1});  // only slot 3 relocates
        template <typename T>
        void push_bytes(const T &data, std::initializer_list<u8> bits) noexcept {
            static_assert(std::is_trivially_copyable_v<T>,
                          "push_bytes requires a trivially-copyable T");

            // Snapshot the slot count BEFORE adding data, so we know how many
            // new slots this push actually introduces.
            const size_t slots_before = (m_rawData.size() + 7) / 8;

            const std::byte *p = reinterpret_cast<const std::byte *>(std::addressof(data));
            m_rawData.insert(m_rawData.end(), p, p + sizeof(T));

            const size_t slots_after = (m_rawData.size() + 7) / 8;
            const size_t new_slots = slots_after - slots_before;

            size_t i = 0;
            for (u8 bit : bits) {
                if (i >= new_slots) { break; }
                insert_into_reloctable(bit, 1);
                ++i;
            }
            for (; i < new_slots; ++i) { insert_into_reloctable(0, 1); }

            check_size();
        }

        /// @brief Append a POD value with no relocation bits (all slots = 0).
        /// @details Convenience overload for the common case of plain data.
        template <typename T> void push_bytes(const T &data) noexcept { push_bytes(data, {}); }

        /// @brief Append a raw byte blob and a single relocation bit applied to every new slot.
        /// @details Same slot-count logic as push_bytes: only the *new* slots get bits.
        void push_blob(const void *data, size_t size, u8 relocation_bit = 0) noexcept;

        /// @brief Sanity check: (raw bytes + 7) / 8 must equal the number of relocation bits.
        /// @throws std::runtime_error if the invariant is broken.
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
        
        /// @brief Layout of the payload if this element is a data-struct.
        /// @details Empty for functions and other element kinds.
        std::optional<StructLayoutInfo> m_structLayout; 

        u64 m_byteOffset = 0; ///< running byte cursor (used by insert_into_reloctable)
        u8  m_bitOffset = 0;  ///< running bit cursor within m_byteOffset
    };

} // namespace carbon