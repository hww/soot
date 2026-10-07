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

    struct StructFieldInfo {
        std::string name;
        std::string type_name;
        u32         offset;
        u32         size;
        bool        is_inline;
        bool        is_array;
        int         array_size;
    };

    struct StructLayoutInfo {
        std::string                  type_name;
        u32                          total_size;
        std::vector<StructFieldInfo> fields;
    };

    // =========================================================================
    // PtrSlot — describes one pointer field inside a POD.
    // =========================================================================

    struct PtrSlot {
        size_t offset;
        size_t size;
    };

#define PTR_FIELD(T, field)                                                                        \
    ::carbon::PtrSlot { offsetof(T, field), sizeof(static_cast<T *>(nullptr)->field) }

    // =========================================================================
    // ProgramBinaryElement
    // =========================================================================
    //
    // Holds the serialised payload of one entry, plus a parallel relocation
    // bitmap (one bit per 8-byte slot).
    //
    // INVARIANT (checked after every mutation):
    //     m_relocTable.size() == (m_rawData.size() + 7) / 8
    //
    // If this invariant is ever broken, check_size() throws with a precise
    // message — no silent corruption.
    //
    // Every push_* method computes the number of new slots from the size of
    // the data being written. The caller never passes a slot count or a bit
    // count, so there is no way to get it wrong.

    struct ProgramBinaryElement {

        explicit ProgramBinaryElement(const u64 size) noexcept;

        ProgramBinaryElement(const ProgramBinaryElement &) = delete;
        ProgramBinaryElement &operator=(const ProgramBinaryElement &) = delete;

        ProgramBinaryElement(ProgramBinaryElement &&other) noexcept
            : m_entry(std::move(other.m_entry)), m_rawData(std::move(other.m_rawData)),
              m_stringOffsets(std::move(other.m_stringOffsets)),
              m_relocTable(std::move(other.m_relocTable)),
              m_structLayout(std::move(other.m_structLayout)), m_byteOffset(other.m_byteOffset),
              m_bitOffset(other.m_bitOffset) {
            other.m_entry.m_entryPtr = nullptr;
            other.m_byteOffset = 0;
            other.m_bitOffset = 0;
        }

        // =====================================================================
        // Public API: append a value.
        // =====================================================================

        /// @brief Append a POD value. No relocations are added.
        template <typename T> void push_value(const T &data) {
            static_assert(std::is_trivially_copyable_v<T>,
                          "push_value requires a trivially-copyable T");

            const size_t     slots_before = (m_rawData.size() + 7) / 8;
            const std::byte *p = reinterpret_cast<const std::byte *>(std::addressof(data));
            m_rawData.insert(m_rawData.end(), p, p + sizeof(T));
            const size_t slots_after = (m_rawData.size() + 7) / 8;
            const size_t new_slots = slots_after - slots_before;

            for (size_t i = 0; i < new_slots; ++i) { insert_into_reloctable(0, 1); }

            check_size();
        }

        /// @brief Append a POD value with one or more pointer fields.
        /// @details Each PtrSlot names a pointer member of T. The slot containing
        ///          that member is marked as relocatable. The pointer field must
        ///          be 8 bytes wide and 8-byte aligned; otherwise an exception is
        ///          thrown with a clear message.
        template <typename T, typename... PtrSlots>
        void push_value_with_ptr(const T &data, PtrSlots... slots) {
            static_assert(std::is_trivially_copyable_v<T>,
                          "push_value_with_ptr requires a trivially-copyable T");

            const size_t base_offset = m_rawData.size();
            const size_t slots_before = (base_offset + 7) / 8;

            const std::byte *p = reinterpret_cast<const std::byte *>(std::addressof(data));
            m_rawData.insert(m_rawData.end(), p, p + sizeof(T));
            const size_t slots_after = (m_rawData.size() + 7) / 8;
            const size_t new_slots = slots_after - slots_before;

            for (size_t i = 0; i < new_slots; ++i) { insert_into_reloctable(0, 1); }

            (verify_and_mark_ptr(base_offset, slots), ...);

            check_size();
        }

        /// @brief Append a raw byte blob with a single relocation bit per new slot.
        void push_blob(const void *data, size_t size, u8 relocation_bit = 0);

        /// @brief Append `num_bits` relocation bits, all set to `bits` (0 or 1).
        void insert_into_reloctable(u8 bits, u64 num_bits) noexcept;

        void insert_string_offset() noexcept;
        void insert_string_offset(u64 offset) noexcept;

        /// @brief Add `offset` to every relocated pointer in m_rawData.
        void adjust_offsets(u64 offset);

        [[nodiscard]] size_t size() const noexcept { return m_rawData.size(); }
        [[nodiscard]] bool   is_empty() const noexcept { return m_rawData.empty(); }

        /// @brief Verify the invariant: relocTable.size() == (rawData.size()+7)/8.
        /// @throws std::runtime_error with precise coordinates if broken.
        void check_size() const;

        void dump(const std::string &title = "", size_t max_len = 256);

        [[nodiscard]] byte_uptr to_byte_uptr() const;

        DCEntry                m_entry;
        std::vector<std::byte> m_rawData;
        std::vector<u64>       m_stringOffsets;
        std::vector<bool>      m_relocTable;

        std::optional<StructLayoutInfo> m_structLayout;

        u64 m_byteOffset = 0;
        u8  m_bitOffset = 0;

    private:
        /// @brief Mark the slot containing a pointer field as relocatable.
        /// @throws std::runtime_error with a human-readable message if the field
        ///         is not 8-byte aligned, is not 8 bytes wide, or falls outside
        ///         the payload that was just written.
        template <typename PtrSlotT>
        void verify_and_mark_ptr(size_t base_offset, const PtrSlotT &slot)
        {
            const size_t field_offset = base_offset + slot.offset;
            const size_t field_size = slot.size;
            const size_t payload_end = m_rawData.size();

            if (field_size != 8) {
                throw std::runtime_error(fmt::format(
                    "[ProgramBinaryElement] pointer field at byte offset {} has size {} "
                    "bytes; expected 8. Only 8-byte pointers can be relocated.",
                    field_offset, field_size));
            }

            if (field_offset % 8 != 0) {
                throw std::runtime_error(
                    fmt::format("[ProgramBinaryElement] pointer field at byte offset {} is not "
                                "8-byte aligned; a relocation bit cannot describe it.",
                                field_offset));
            }

            if (field_offset + field_size > payload_end) {
                throw std::runtime_error(
                    fmt::format("[ProgramBinaryElement] pointer field at byte offset {} (size {}) "
                                "extends past the payload end ({}).",
                                field_offset, field_size, payload_end));
            }

            const size_t slot_index = field_offset / 8;
            if (slot_index >= m_relocTable.size()) {
                throw std::runtime_error(
                    fmt::format("[ProgramBinaryElement] internal error — slot index {} is out of "
                                "range (relocTable has {} entries).",
                                slot_index, m_relocTable.size()));
            }
            m_relocTable[slot_index] = true;
        }
    };

} // namespace carbon