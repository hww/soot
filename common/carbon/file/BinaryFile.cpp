#include "BinaryFile.hpp"

#include "common/carbon/lib/StringIdManager.hpp"
#include "fmt/format.h"
#include "util/Log.hpp"

#include <cstddef>
#include <cstring>
#include <string>
#include <expected>
#include <filesystem>
#include <fstream>
#include <immintrin.h>
#include <iostream>
#include <vector>
#include "BinaryFileInspector.hpp"

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
        file.fixup_header_pointers(); // <-- add this
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
        file.fixup_header_pointers(); // <-- add this
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

    /// @brief Ensure the header's m_pStartOfData is an absolute pointer.
    /// @details The pointer is expected to have been relocated by read_reloc_table().
    ///          If it still looks like a file offset (smaller than the base address
    ///          of m_bytes), we assume the emitter forgot to set the relocation bit
    ///          for this header field and patch it here.
    ///
    ///          A warning is logged every time this happens, so the underlying
    ///          emitter bug can be tracked down and fixed.
    void BinaryFile::fixup_header_pointers() noexcept {
        if (!m_dcheader) { return; }

        auto      *hdr = const_cast<DC_Header *>(m_dcheader);
        const auto ptr = reinterpret_cast<uintptr_t>(hdr->m_pStartOfData);
        const auto base = reinterpret_cast<uintptr_t>(m_bytes.get());

        // A relocated pointer is always >= base. Anything smaller is an offset.
        if (ptr != 0 && ptr < base) {
            hdr->m_pStartOfData = reinterpret_cast<DCEntry *>(base + ptr);
            lg::warn("BinaryFile::fixup_header_pointers: m_pStartOfData was not relocated "
                     "(offset 0x{:X}); patched on the fly. "
                     "Fix the emitter to set the relocation bit for the header field.",
                     ptr);
        }
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

    // ===========================================================================
    // Entry table access
    // ===========================================================================

    const DCEntry *BinaryFile::entries() const noexcept {
        if (!m_dcheader) { return nullptr; }

        const auto ptr = m_dcheader->m_pStartOfData;
        const auto base = reinterpret_cast<uintptr_t>(m_bytes.get());

        if (reinterpret_cast<uintptr_t>(ptr) < base) {
            // Похоже, релокация не применилась: ptr остался offset'ом.
            lg::error("BinaryFile::entries: m_pStartOfData = 0x{:X} is NOT relocated "
                      "(base = 0x{:X}, offset = 0x{:X}). "
                      "Likely a missing relocation bit for the header field.",
                      reinterpret_cast<uintptr_t>(ptr), base, reinterpret_cast<uintptr_t>(ptr));
            return nullptr;
        }

        return ptr;
    }

    u32 BinaryFile::entry_count() const noexcept {
        return m_dcheader ? m_dcheader->m_numEntries : 0;
    }

    const DCEntry *BinaryFile::find_entry_by_name(sid64 name) const noexcept {
        const DCEntry *table = entries();
        if (!table) { return nullptr; }
        for (u32 i = 0; i < m_dcheader->m_numEntries; ++i) {
            if (table[i].m_nameID == name) { return &table[i]; }
        }
        return nullptr;
    }

    // ===========================================================================
    // Entry payload
    // ===========================================================================

    BinaryFile::EntryKind BinaryFile::entry_kind(const DCEntry &entry) noexcept {
        if (entry.m_typeId == SID("script-lambda")) { return EntryKind::ScriptLambda; }
        if (entry.m_typeId == SID("state-script")) { return EntryKind::StateScript; }
        if (entry.m_typeId == SS_TYPE_SID) { return EntryKind::SsType; }
        if (entry.m_typeId == SID("map") || entry.m_typeId == SID("map-32")) {
            return EntryKind::Map;
        }
        if (entry.m_typeId != 0) {
            // Any other non-zero typeId is treated as an opaque data structure.
            return EntryKind::DataStruct;
        }
        return EntryKind::Unknown;
    }

    const ScriptLambda *BinaryFile::entry_as_lambda(const DCEntry &entry) const noexcept {
        if (entry_kind(entry) != EntryKind::ScriptLambda) { return nullptr; }
        if (!is_valid_ptr(entry.m_entryPtr, sizeof(ScriptLambda))) { return nullptr; }
        return reinterpret_cast<const ScriptLambda *>(entry.m_entryPtr);
    }

    const StateScript *BinaryFile::entry_as_state_script(const DCEntry &entry) const noexcept {
        if (entry_kind(entry) != EntryKind::StateScript) { return nullptr; }
        if (!is_valid_ptr(entry.m_entryPtr, sizeof(StateScript))) { return nullptr; }
        return reinterpret_cast<const StateScript *>(entry.m_entryPtr);
    }

    const SsType *BinaryFile::entry_as_ss_type(const DCEntry &entry) const noexcept {
        if (entry_kind(entry) != EntryKind::SsType) { return nullptr; }
        if (!is_valid_ptr(entry.m_entryPtr, sizeof(SsType))) { return nullptr; }
        return reinterpret_cast<const SsType *>(entry.m_entryPtr);
    }

    // ===========================================================================
    // SID and string helpers
    // ===========================================================================

    std::string BinaryFile::resolve_sid(sid64 id) const {
        if (id == 0) { return ""; }
        // Try the file-local cache first.
        const auto it = m_sidCache.find(id);
        if (it != m_sidCache.end()) { return it->second; }
        // Fall back to the global manager.
        return StringIdManager::instance().get_string(id);
    }

    std::string BinaryFile::read_string_at(location loc) const {
        if (loc.m_ptr == nullptr) { return ""; }
        if (loc.num() < reinterpret_cast<p64>(m_bytes.get()) ||
            loc.num() >= reinterpret_cast<p64>(m_bytes.get()) + m_size) {
            return "";
        }
        return std::string(loc.as<const char>());
    }

    // ===========================================================================
    // Debug
    // ===========================================================================

    std::vector<std::string> BinaryFile::validate() const {
        std::vector<std::string> violations;

        if (!m_dcheader) {
            violations.emplace_back("header is null");
            return violations;
        }

        const auto *hdr = m_dcheader;

        if (hdr->m_magic != DC_FILE_MAGIC) {
            violations.emplace_back(
                fmt::format("bad magic: 0x{:08X}, expected 0x{:08X}", hdr->m_magic, DC_FILE_MAGIC));
        }
        if (hdr->m_versionNumber != DC_FILE_VERSION) {
            violations.emplace_back(
                fmt::format("bad version: {}, expected {}", hdr->m_versionNumber, DC_FILE_VERSION));
        }
        if (hdr->m_textSize < sizeof(DC_Header)) {
            violations.emplace_back(fmt::format("m_textSize too small: 0x{:X}, expected >= 0x{:X}",
                                                hdr->m_textSize, sizeof(DC_Header)));
        }
        if (hdr->m_textSize > m_size) {
            violations.emplace_back(fmt::format(
                "m_textSize out of bounds: 0x{:X}, file size 0x{:X}", hdr->m_textSize, m_size));
        }
        if (hdr->m_stringsOffset >= hdr->m_textSize) {
            violations.emplace_back(fmt::format("m_stringsOffset >= m_textSize: 0x{:X} >= 0x{:X}",
                                                hdr->m_stringsOffset, hdr->m_textSize));
        }
        if (hdr->m_numEntries > 100000) {
            violations.emplace_back(
                fmt::format("m_numEntries suspiciously large: {}", hdr->m_numEntries));
        }

        const DCEntry *table = entries();
        if (table) {
            for (u32 i = 0; i < hdr->m_numEntries; ++i) {
                const DCEntry &e = table[i];
                if (e.m_entryPtr == nullptr) { continue; }
                if (!is_valid_ptr(e.m_entryPtr, 1)) {
                    violations.emplace_back(fmt::format("entry[{}].m_entryPtr outside file: 0x{:X}",
                                                        i,
                                                        reinterpret_cast<uintptr_t>(e.m_entryPtr)));
                    continue;
                }
                // SsType entries must be fully contained in the file.
                if (entry_kind(e) == EntryKind::SsType) {
                    if (!is_valid_ptr(e.m_entryPtr, sizeof(SsType))) {
                        violations.emplace_back(fmt::format(
                            "entry[{}] is SsType but payload is smaller than sizeof(SsType)", i));
                    }
                }
            }
        } else {
            violations.emplace_back("entry table pointer is null");
        }

        return violations;
    }

    void BinaryFile::dump_entries(std::ostream &os) const {
        const DCEntry *table = entries();
        if (!table) {
            os << "(no entries)\n";
            return;
        }
        const u32 n = entry_count();
        os << fmt::format("{:>4}  {:>18}  {:>18}  {:>18}  {}\n", "idx", "name", "type", "ptr",
                          "kind");
        for (u32 i = 0; i < n; ++i) {
            const DCEntry &e = table[i];
            const auto     kind = entry_kind(e);
            const char    *kind_str = "unknown";
            switch (kind) {
            case EntryKind::ScriptLambda: kind_str = "script-lambda"; break;
            case EntryKind::StateScript: kind_str = "state-script"; break;
            case EntryKind::SsType: kind_str = "ss-type"; break;
            case EntryKind::Map: kind_str = "map"; break;
            case EntryKind::DataStruct: kind_str = "data-struct"; break;
            case EntryKind::Unknown: kind_str = "unknown"; break;
            }
            os << fmt::format("{:>4}  {:>18}  {:>18}  {:>18}  {}\n", i, resolve_sid(e.m_nameID),
                              resolve_sid(e.m_typeId), fmt::format("{:p}", e.m_entryPtr), kind_str);
        }
    }

    bool BinaryFile::is_valid_ptr(const void *ptr, size_t size) const noexcept {
        if (!ptr) return false;
        const auto addr = reinterpret_cast<uintptr_t>(ptr);
        const auto base = reinterpret_cast<uintptr_t>(m_bytes.get());
        const auto end = base + m_size;
        return addr >= base && addr + size <= end;
    }
}
