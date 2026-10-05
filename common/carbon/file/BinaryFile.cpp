#include "BinaryFile.hpp"

#include "fmt/format.h"
#include "util/Log.hpp"

#include <cstddef>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <immintrin.h>

namespace carbon {

    [[nodiscard]] std::expected<BinaryFile, std::string>
    BinaryFile::from_path(const std::filesystem::path &path) noexcept {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) { return std::unexpected{"couldn't open " + path.string()}; }

        const u64 size = std::filesystem::file_size(path);
        if (size == 0) { return std::unexpected{path.string() + " is empty"}; }

        auto bytes =
            byte_uptr(static_cast<std::byte *>(::operator new[](size, std::align_val_t(64))));
        in.read(reinterpret_cast<char *>(bytes.get()), static_cast<std::streamsize>(size));

        auto *dcheader = reinterpret_cast<DC_Header *>(bytes.get());

        if (dcheader->m_magic != DC_FILE_MAGIC) {
            return std::unexpected{"not a DC-file: magic is 0x" +
                                   fmt::format("{:08X}", dcheader->m_magic) +
                                   ", expected 0x44433030"};
        }

        if (dcheader->m_versionNumber != DC_FILE_VERSION) {
            return std::unexpected{"not a DC-file: version is " +
                                   std::to_string(dcheader->m_versionNumber) + ", expected " +
                                   std::to_string(DC_FILE_VERSION)};
        }

        BinaryFile file(path, size, std::move(bytes), dcheader);
        file.read_reloc_table();
        file.replace_newlines_in_stringtable();
        return file;
    }
    
    [[nodiscard]] std::expected<BinaryFile, std::string>
    BinaryFile::from_buffer(const std::filesystem::path &path, byte_uptr bytes,
                            size_t size) noexcept {
        auto *dcheader = reinterpret_cast<DC_Header *>(bytes.get());

        if (dcheader->m_magic != DC_FILE_MAGIC) {
            return std::unexpected{"not a DC-file: magic is 0x" +
                                   fmt::format("{:08X}", dcheader->m_magic) +
                                   ", expected 0x44433030"};
        }

        if (dcheader->m_versionNumber != DC_FILE_VERSION) {
            return std::unexpected{"not a DC-file: version is " +
                                   std::to_string(dcheader->m_versionNumber) + ", expected " +
                                   std::to_string(DC_FILE_VERSION)};
        }

        BinaryFile file(path, size, std::move(bytes), dcheader);
        file.read_reloc_table();
        file.replace_newlines_in_stringtable();
        return file;
    }

    [[nodiscard]] bool BinaryFile::save(const std::filesystem::path &path) noexcept {
        try {
            std::ofstream out(path, std::ios::binary);
            if (!out.is_open()) { return false; }

            // Re-map relocated pointers back to file offsets before writing.
            const byte_uptr unmapped = get_unmapped();

            out.write(reinterpret_cast<const char *>(unmapped.get()),
                      static_cast<std::streamsize>(m_size));

            return out.good();
        } catch (const std::exception &e) {
            lg::error("BinaryFile::save: exception for '{}': {}", path.string(), e.what());
            return false;
        }
    }

    /// Replace '\n' with ' ' inside the string table.
    /// The engine does the same after loading, so we mirror it for consistency.
    void BinaryFile::replace_newlines_in_stringtable() noexcept {
        // The reloc table has a u32 size prefix; m_relocTable points after it,
        // so (m_relocTable - 4) is the end of the string table.
        constexpr u64 table_size_prefix = 4;
        const u64     table_size = m_relocTable.num() - table_size_prefix - m_strings.num();

        char *string_table = reinterpret_cast<char *>(const_cast<std::byte *>(m_strings.m_ptr));
        for (u64 i = 0; i < table_size; ++i) {
            if (string_table[i] == '\n') { string_table[i] = ' '; }
        }
    }

    /// @return true if the file offset `loc` is the target of a relocated pointer.
    /// @details m_pointedAtTable is a bitmap where bit N corresponds to byte offset N*8.
    [[nodiscard]] bool BinaryFile::gets_pointed_at(const location loc) const noexcept {
        const p64 byte_offset = loc.num() - reinterpret_cast<p64>(m_bytes.get());
        const p64 slot_index = byte_offset / 8;
        const u8  byte = static_cast<u8>(m_pointedAtTable[slot_index / 8]);
        return (byte & (1u << (slot_index % 8))) != 0;
    }
    
    /// @return true if `loc` points to a relocated 8-byte slot in the file.
    /// @details m_relocTable is a bitmap where bit N covers byte offset N*8.
    [[nodiscard]] bool BinaryFile::is_file_ptr(const location loc) const noexcept {
        const p64 byte_offset = loc.num() - reinterpret_cast<p64>(m_bytes.get());
        if (byte_offset < 0 || static_cast<u64>(byte_offset) >= m_size) { return false; }

        const u64 slot_index = static_cast<u64>(byte_offset) / 8;
        const u8  byte = m_relocTable.get<u8>(slot_index / 8);
        return (byte & (1u << (slot_index % 8))) != 0;
    }

    
    /// @return true if `loc` lies inside the string table.
    [[nodiscard]] bool BinaryFile::is_string(const location loc) const noexcept {
        return loc >= m_strings;
    }

    /// Parse the relocation bitmap and apply relocations in-place.
    /// Each set bit N in the bitmap means the 8-byte slot at file offset N*8
    /// holds a file-relative pointer that must be turned into an absolute one.
    /// Also builds m_pointedAtTable (bitmap of "some pointer points here")
    /// and initialises m_strings from m_dcheader->m_stringsOffset.
    void BinaryFile::read_reloc_table() noexcept {
        std::byte *reloc_data = m_bytes.get() + m_dcheader->m_textSize;

        const u32 table_size = *reinterpret_cast<u32 *>(reloc_data);
        m_pointedAtTable =
            byte_uptr(static_cast<std::byte *>(::operator new[](table_size, std::align_val_t(64))));
        std::memset(m_pointedAtTable.get(), 0, table_size);

        m_relocTable = location(reloc_data + 4);

#ifdef AVX512
        const __m512i one = _mm512_set1_epi64(0x1);
        const __m512i base = _mm512_set1_epi64(reinterpret_cast<p64>(m_bytes.get()));

        std::byte *data_segment_ptr = m_bytes.get();
        for (u64 i = 0; i < table_size; ++i) {
            const __mmask8 reloc_byte = m_relocTable.get<__mmask8>(i);

            __m512i data_segment = _mm512_load_epi64(data_segment_ptr);
            __m512i data_segment_masked = _mm512_maskz_mov_epi64(reloc_byte, base);
            data_segment_masked = _mm512_add_epi64(data_segment_masked, data_segment);
            _mm512_store_epi64(data_segment_ptr, data_segment_masked);
            _mm512_mask_i64scatter_epi64(m_pointedAtTable.get(), reloc_byte, data_segment, one,
                                         sizeof(std::byte));

            data_segment_ptr += 64;
        }
#else
        for (u64 slot = 0; slot < static_cast<u64>(table_size) * 8; ++slot) {
            if (m_relocTable.get<u8>(slot / 8) & (1u << (slot % 8))) {
                u64      *entry = reinterpret_cast<u64 *>(m_bytes.get() + slot * 8);
                const u64 offset = *entry;
                *entry = reinterpret_cast<u64>(m_bytes.get() + offset);

                reinterpret_cast<u8 *>(m_pointedAtTable.get())[offset / 64] |=
                    static_cast<u8>(1u << ((offset / 8) % 8));
            }
        }
#endif
        m_strings = location(m_bytes.get() + m_dcheader->m_stringsOffset);
    }

    /// @return a copy of the file bytes with all relocated pointers converted
    ///         back to file-relative offsets (i.e. exactly as on disk).
    [[nodiscard]] byte_uptr BinaryFile::get_unmapped() const {
        auto unmapped_bytes =
            byte_uptr(static_cast<std::byte *>(::operator new[](m_size, std::align_val_t(64))));

        std::memcpy(unmapped_bytes.get(), m_bytes.get(), m_size);

        std::byte *reloc_data = unmapped_bytes.get() + m_dcheader->m_textSize;
        const u32  table_size = *reinterpret_cast<u32 *>(reloc_data);

        for (u64 slot = 0; slot < static_cast<u64>(table_size) * 8; ++slot) {
            if (m_relocTable.get<u8>(slot / 8) & (1u << (slot % 8))) {
                auto *entry = reinterpret_cast<p64 *>(unmapped_bytes.get() + slot * 8);
                *entry -= reinterpret_cast<p64>(m_bytes.get());
            }
        }

        return unmapped_bytes;
    }
}
