// BinaryFile.cpp

#include "BinaryFile.hpp"

#include "common/carbon/lib/StringIdManager.hpp"
#include "fmt/format.h"
#include "util/Log.hpp"

#include "BinaryFileInspector.hpp"
#include <cstddef>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <vector>

namespace carbon {

    // =========================================================================
    // Move semantics
    // =========================================================================
    //
    // BinaryFile holds raw pointers into m_bytes:
    //     m_dcheader   -> (const DC_Header *) m_bytes.get()
    //     m_relocTable -> m_bytes.get() + m_textSize + sizeof(u32)
    //     m_strings    -> m_bytes.get() + m_stringsOffset
    //
    // A defaulted move constructor would copy those pointers verbatim and
    // leave them pointing at the old (moved-from) buffer, which is then
    // freed. We therefore re-anchor them after the move.
    void BinaryFile::rebuild_pointers_from_bytes() noexcept {
        if (!m_bytes || m_size == 0) {
            m_dcheader = nullptr;
            m_relocTable = location();
            m_strings = location();
            return;
        }

        m_dcheader = reinterpret_cast<const DC_Header *>(m_bytes.get());

        const u64 text_size = m_dcheader->m_textSize;
        if (text_size + sizeof(u32) <= m_size) {
            m_relocTable = location(m_bytes.get() + text_size + sizeof(u32));
        } else {
            m_relocTable = location();
        }

        const u64 strings_offset = m_dcheader->m_stringsOffset;
        if (strings_offset <= m_size) {
            m_strings = location(m_bytes.get() + strings_offset);
        } else {
            m_strings = location();
        }
    }

    BinaryFile::BinaryFile(BinaryFile &&other) noexcept
        : m_path(std::move(other.m_path)), m_dcheader(other.m_dcheader),
          m_dcscript(other.m_dcscript), m_size(other.m_size), m_bytes(std::move(other.m_bytes)),
          m_pointedAtTable(std::move(other.m_pointedAtTable)), m_strings(other.m_strings),
          m_relocTable(other.m_relocTable), m_sidCache(std::move(other.m_sidCache)),
          m_emittedStructs(std::move(other.m_emittedStructs)),
          m_dataStructs(std::move(other.m_dataStructs)) {
        // m_dcheader was copied from `other` and still points into the old
        // buffer; rebuild it (and the other raw pointers) from m_bytes.
        rebuild_pointers_from_bytes();
        other.m_dcheader = nullptr;
        other.m_dcscript = nullptr;
        other.m_size = 0;
    }

    BinaryFile &BinaryFile::operator=(BinaryFile &&other) noexcept {
        if (this != &other) {
            m_path = std::move(other.m_path);
            m_dcheader = other.m_dcheader;
            m_dcscript = other.m_dcscript;
            m_size = other.m_size;
            m_bytes = std::move(other.m_bytes);
            m_pointedAtTable = std::move(other.m_pointedAtTable);
            m_strings = other.m_strings;
            m_relocTable = other.m_relocTable;
            m_sidCache = std::move(other.m_sidCache);
            m_emittedStructs = std::move(other.m_emittedStructs);
            m_dataStructs = std::move(other.m_dataStructs);

            rebuild_pointers_from_bytes();
            other.m_dcheader = nullptr;
            other.m_dcscript = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

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
        try {
            file.read_reloc_table();
        } catch (const std::exception &e) {
            return std::unexpected{std::string("read_reloc_table failed: ") + e.what()};
        }
        file.replace_newlines_in_stringtable();

        // Explicit move: BinaryFile holds raw pointers into its buffer, so
        // the move constructor must re-anchor them. Returning by value would
        // otherwise let the compiler pick the (deleted) copy path.
        return std::move(file);
    }

    [[nodiscard]] std::expected<BinaryFile, std::string>
    BinaryFile::from_buffer(const std::filesystem::path &path, byte_uptr bytes,
                            size_t size) noexcept {
        if (!bytes) { return std::unexpected{"from_buffer: bytes is null"}; }
        if (size < sizeof(DC_Header)) {
            return std::unexpected{fmt::format(
                "from_buffer: buffer is too small ({} bytes) for a DC_Header ({} bytes)", size,
                sizeof(DC_Header))};
        }

        auto *dcheader = reinterpret_cast<DC_Header *>(bytes.get());

        if (dcheader->m_magic != DC_FILE_MAGIC) {
            return std::unexpected{
                fmt::format("not a DC-file: magic is 0x{:08X}, expected 0x{:08X}",
                            dcheader->m_magic, DC_FILE_MAGIC)};
        }

        if (dcheader->m_versionNumber != DC_FILE_VERSION) {
            return std::unexpected{fmt::format("not a DC-file: version is {}, expected {}",
                                               dcheader->m_versionNumber, DC_FILE_VERSION)};
        }

        // ------------------------------------------------------------------
        // Sanity-check the header before we start touching memory.
        // ------------------------------------------------------------------
        if (dcheader->m_textSize < sizeof(DC_Header)) {
            return std::unexpected{
                fmt::format("header m_textSize = 0x{:X} is smaller than sizeof(DC_Header) = 0x{:X}",
                            dcheader->m_textSize, sizeof(DC_Header))};
        }
        if (dcheader->m_textSize > size) {
            return std::unexpected{
                fmt::format("header m_textSize = 0x{:X} exceeds buffer size 0x{:X}",
                            dcheader->m_textSize, size)};
        }
        if (dcheader->m_stringsOffset > dcheader->m_textSize) {
            return std::unexpected{
                fmt::format("header m_stringsOffset = 0x{:X} exceeds m_textSize = 0x{:X}",
                            dcheader->m_stringsOffset, dcheader->m_textSize)};
        }
        if (dcheader->m_numEntries > 1000000) {
            return std::unexpected{fmt::format(
                "header m_numEntries = {} looks bogus (limit 1000000)", dcheader->m_numEntries)};
        }

        // The relocation table (size prefix + bitmap) must fit in the buffer.
        if (dcheader->m_textSize + sizeof(u32) > size) {
            return std::unexpected{fmt::format(
                "no room for relocation table size prefix: m_textSize = 0x{:X}, size = 0x{:X}",
                dcheader->m_textSize, size)};
        }
        const u32 table_size = *reinterpret_cast<const u32 *>(bytes.get() + dcheader->m_textSize);
        if (table_size == 0) {
            return std::unexpected{"relocation table size prefix is zero; the file is malformed"};
        }
        if (dcheader->m_textSize + sizeof(u32) + table_size > size) {
            return std::unexpected{
                fmt::format("relocation table does not fit: m_textSize + 4 + table_size = 0x{:X}, "
                            "buffer size = 0x{:X}",
                            dcheader->m_textSize + sizeof(u32) + table_size, size)};
        }

        BinaryFile file(path, size, std::move(bytes), dcheader);
        try {
            file.read_reloc_table();
        } catch (const std::exception &e) {
            return std::unexpected{std::string("read_reloc_table failed: ") + e.what()};
        }
        file.replace_newlines_in_stringtable();

        // See the comment in from_path.
        return std::move(file);
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
    /// @brief Parse the relocation bitmap and apply relocations in-place.
    /// @details Every memory access is bounds-checked. If anything is off, the
    ///          function throws with a precise message that names the offending
    ///          offset, the size, and the buffer limit. No silent corruption.
    void BinaryFile::read_reloc_table() {
        if (!m_dcheader) { throw std::runtime_error("[BinaryFile] read_reloc_table: null header"); }

        // ------------------------------------------------------------------
        // 1. Locate the relocation table.
        // ------------------------------------------------------------------
        const u64 text_size = m_dcheader->m_textSize;
        if (text_size + sizeof(u32) > m_size) {
            throw std::runtime_error(
                fmt::format("[BinaryFile] read_reloc_table: m_textSize = 0x{:X} leaves no room "
                            "for the u32 size prefix (buffer size = 0x{:X}).",
                            text_size, m_size));
        }

        std::byte *reloc_data = m_bytes.get() + text_size;
        const u32  table_size = *reinterpret_cast<const u32 *>(reloc_data);

        if (table_size == 0) {
            throw std::runtime_error("[BinaryFile] read_reloc_table: table_size is zero");
        }
        if (text_size + sizeof(u32) + table_size > m_size) {
            throw std::runtime_error(
                fmt::format("[BinaryFile] read_reloc_table: table_size = {} (bytes) does not fit: "
                            "m_textSize + 4 + table_size = 0x{:X}, buffer size = 0x{:X}.",
                            table_size, text_size + sizeof(u32) + table_size, m_size));
        }

        // m_pointedAtTable is the same size as the bitmap itself. Allocate a
        // 64-byte-aligned buffer for it (matching the rest of the file).
        m_pointedAtTable =
            byte_uptr(static_cast<std::byte *>(::operator new[](table_size, std::align_val_t(64))));
        std::memset(m_pointedAtTable.get(), 0, table_size);

        m_relocTable = location(reloc_data + sizeof(u32));

        // ------------------------------------------------------------------
        // 2. Apply relocations.
        // ------------------------------------------------------------------
        // For each set bit N, the 8-byte slot at offset N*8 in the text section
        // holds a file-relative pointer that must be turned into an absolute one.
        // We also mark the target offset in m_pointedAtTable.
        const u64 total_slots = static_cast<u64>(table_size) * 8;
        const u64 text_slots = text_size / 8; // number of 8-byte slots in the text section

        for (u64 slot = 0; slot < total_slots; ++slot) {
            const u8   byte = m_relocTable.get<u8>(slot / 8);
            const bool is_set = (byte & (1u << (slot % 8))) != 0;
            if (!is_set) { continue; }

            // The slot must lie inside the text section.
            const u64 slot_offset = slot * 8;
            if (slot_offset + 8 > text_size) {
                throw std::runtime_error(
                    fmt::format("[BinaryFile] read_reloc_table: relocation bit {} refers to "
                                "byte offset 0x{:X}, which is past the end of the text section "
                                "(m_textSize = 0x{:X}).",
                                slot, slot_offset, text_size));
            }

            auto     *entry = reinterpret_cast<u64 *>(m_bytes.get() + slot_offset);
            const u64 offset = *entry;

            lg::debug("reloc: slot={}, slot_offset=0x{:X}, offset=0x{:X}", slot, slot_offset,
                     offset);
            // The stored offset must point inside the buffer.
            if (offset >= m_size) {
                throw std::runtime_error(
                    fmt::format("[BinaryFile] read_reloc_table: relocation bit {} at byte offset "
                                "0x{:X} stores target offset 0x{:X}, which is outside the buffer "
                                "(size = 0x{:X}).",
                                slot, slot_offset, offset, m_size));
            }

            *entry = reinterpret_cast<u64>(m_bytes.get() + offset);

            // Mark the target in m_pointedAtTable. The target offset must be
            // inside the table (which is table_size bytes wide, one bit per
            // 8-byte slot of the text section).
            const u64 target_slot = offset / 8;
            if (target_slot >= total_slots) {
                throw std::runtime_error(
                    fmt::format("[BinaryFile] read_reloc_table: relocation bit {} points at "
                                "offset 0x{:X} (slot {}), which is outside the bitmap "
                                "({} slots).",
                                slot, offset, target_slot, total_slots));
            }
            reinterpret_cast<u8 *>(m_pointedAtTable.get())[target_slot / 8] |=
                static_cast<u8>(1u << (target_slot % 8));
        }

        // ------------------------------------------------------------------
        // 3. Locate the string table.
        // ------------------------------------------------------------------
        if (m_dcheader->m_stringsOffset > m_size) {
            throw std::runtime_error(
                fmt::format("[BinaryFile] read_reloc_table: m_stringsOffset = 0x{:X} is past "
                            "the buffer end (size = 0x{:X}).",
                            m_dcheader->m_stringsOffset, m_size));
        }
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
} // namespace carbon