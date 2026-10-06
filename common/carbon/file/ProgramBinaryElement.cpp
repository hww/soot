#include "ProgramBinaryElement.hpp"

#include "lib/StringIdManager.hpp"
#include "lib/Variant.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <util/Log.hpp>

namespace carbon {

    ProgramBinaryElement::ProgramBinaryElement(const u64 size) noexcept {
        m_rawData.reserve(size);
        m_relocTable.reserve(size / 64);
        m_byteOffset = 0;
        m_bitOffset = 0;
        m_entry.m_entryPtr = nullptr;
        m_entry.m_nameID = TypeIds::none;
        m_entry.m_typeId = TypeIds::none;
    }

    void ProgramBinaryElement::insert_into_reloctable(const u8 bits, const u64 num_bits) noexcept {
        for (u64 i = 0; i < num_bits; ++i) { m_relocTable.push_back((bits >> i) & 0x1); }
    }

    void ProgramBinaryElement::push_blob(const void *data, size_t size,
                                         u8 relocation_bit) noexcept {
        const auto *p = reinterpret_cast<const std::byte *>(data);
        m_rawData.insert(m_rawData.end(), p, p + size);

        // One relocation bit per 8-byte slot, rounded up.
        const size_t num_slots = (size + 7) / 8;
        for (size_t i = 0; i < num_slots; ++i) { insert_into_reloctable(relocation_bit, 1); }

        check_size();
    }

    void ProgramBinaryElement::check_size() const {
        const size_t data_slots = (m_rawData.size() + 7) / 8;
        const size_t reloc_slots = m_relocTable.size();
        if (data_slots != reloc_slots) {
            throw std::runtime_error(
                fmt::format("ProgramBinaryElement: raw_data has {} slots, reloc table has {} bits",
                            data_slots, reloc_slots));
        }
    }

    void ProgramBinaryElement::insert_string_offset() noexcept {
        m_stringOffsets.emplace_back(m_rawData.size());
    }

    void ProgramBinaryElement::insert_string_offset(const u64 offset) noexcept {
        m_stringOffsets.emplace_back(m_rawData.size() + offset);
    }

    void ProgramBinaryElement::adjust_offsets(const u64 offset) noexcept {
        const u64 chunks = m_rawData.size() / sizeof(u64);
        const u64 reloc_size = m_relocTable.size();

        for (u64 i = 0; i < chunks && i < reloc_size; ++i) {
            if (m_relocTable[i]) {
                auto *ptr = reinterpret_cast<u64 *>(m_rawData.data() + i * sizeof(u64));
                if (*ptr != 0) { *ptr += offset; }
            }
        }
    }

    byte_uptr ProgramBinaryElement::to_byte_uptr() const {
        auto bytes = byte_uptr(
            static_cast<std::byte *>(::operator new[](m_rawData.size(), std::align_val_t(64))));
        std::memcpy(bytes.get(), m_rawData.data(), m_rawData.size());
        return bytes;
    }

    void ProgramBinaryElement::dump(const std::string &title, size_t max_len) {
        std::printf("%s\n", title.empty() ? "=== ProgramBinaryElement ==="
                                          : ("=== " + title + " ===").c_str());

        std::printf("  Raw Data:       %zu bytes\n", m_rawData.size());
        std::printf("  Reloc Table:    %zu bits\n", m_relocTable.size());
        std::printf("  String Offsets: %zu\n", m_stringOffsets.size());

        const std::string nameStr = StringIdManager::instance().get_string(m_entry.m_nameID);
        const std::string typeStr = StringIdManager::instance().get_string(m_entry.m_typeId);
        std::printf("  Entry:\n");
        std::printf("    nameID: 0x%016llX `%s`\n",
                    static_cast<unsigned long long>(m_entry.m_nameID), nameStr.c_str());
        std::printf("    typeId: 0x%016llX `%s`\n",
                    static_cast<unsigned long long>(m_entry.m_typeId), typeStr.c_str());
        std::printf("    ptr:    %p\n", m_entry.m_entryPtr);

        // Raw-data hex dump with a '+' mark on relocatable slots.
        std::printf("  Raw Data (hex, '+' = relocatable):\n");
        const size_t dump_size = std::min(m_rawData.size(), max_len);
        for (size_t i = 0; i < dump_size; ++i) {
            if (i % 16 == 0) std::printf("    %04zx: ", i);

            const bool reloc = (i / 8 < m_relocTable.size()) && m_relocTable[i / 8];
            std::printf(reloc ? "+%02X" : " %02X", static_cast<unsigned char>(m_rawData[i]));

            if ((i + 1) % 8 == 0 && (i + 1) % 16 != 0) std::printf(" ");
            if ((i + 1) % 16 == 0) std::printf("\n");
        }
        if (dump_size % 16 != 0) std::printf("\n");
        if (m_rawData.size() > max_len) {
            std::printf("    ... (%zu more bytes)\n", m_rawData.size() - max_len);
        }

        const size_t reloc_count =
            static_cast<size_t>(std::count(m_relocTable.begin(), m_relocTable.end(), true));
        std::printf("  Relocations: %zu / %zu bits (%.1f%%)\n", reloc_count, m_relocTable.size(),
                    m_relocTable.empty()
                        ? 0.0
                        : 100.0 * static_cast<double>(reloc_count) / m_relocTable.size());

        for (size_t i = 0; i < m_relocTable.size(); ++i) {
            if (i % 16 == 0) std::printf("    %04zx: ", i);
            std::printf("%01X ", static_cast<unsigned>(m_relocTable[i]));
            if ((i + 1) % 8 == 0 && (i + 1) % 16 != 0) std::printf(" ");
            if ((i + 1) % 16 == 0) std::printf("\n");
        }
        std::printf("\n=== End Dump ===\n");
    }

} // namespace carbon