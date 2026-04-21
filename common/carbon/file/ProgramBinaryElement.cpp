#include "ProgramBinaryElement.hpp"
#include "carbon/lib/StringId.hpp"
#include "lib/StringIdManager.hpp"

namespace carbon {

ProgramBinaryElement::ProgramBinaryElement(const u64 size) noexcept {
    m_rawData.reserve(size);
    m_relocTable.reserve(size / 64);
}

void ProgramBinaryElement::insert_into_reloctable(const u8 bits, const u64 num_bits) noexcept {
    // const u8 bit_space_remaining = (8 - m_bitOffset % 8);
    // if (bit_space_remaining >= num_bits) {
    //     m_relocTable[m_byteOffset] |= bits << m_bitOffset;
    //     m_bitOffset += num_bits;
    //     assert(m_bitOffset <= 8);
    //     if (m_bitOffset == 8) {
    //         m_bitOffset = 0;
    //         m_byteOffset++;
    //     }
    // } else {
    //     m_relocTable[m_byteOffset++] |= bits << m_bitOffset;
    //     m_relocTable[m_byteOffset] |= bits >> bit_space_remaining;
    //     m_bitOffset = num_bits - bit_space_remaining;
    // }
    for (u64 i = 0; i < num_bits; ++i) {
        m_relocTable.push_back((bits >> i) & 0x1);
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
    for (u64 i = 0; i < chunks; ++i) {
        if (m_relocTable[i]) {
            u64* ptr = reinterpret_cast<u64*>(m_rawData.data() + i * sizeof(u64));
            if (*ptr != 0) {
                *ptr += offset;
            }
        }
    }
}

byte_uptr ProgramBinaryElement::to_byte_uptr() const {
        struct LocalDeleter {
            void operator()(std::byte* p) const noexcept {
                ::operator delete[](p, std::align_val_t(64));
            }
        };
        
        std::unique_ptr<std::byte[], LocalDeleter> bytes(
            static_cast<std::byte*>(::operator new[](m_rawData.size(), std::align_val_t(64))),
            LocalDeleter()
        );
        std::memcpy(bytes.get(), m_rawData.data(), m_rawData.size());
        
        // Нужно сконвертировать в BinaryFile::byte_uptr
        // Это сложно, так как типы deleter'ов разные
        return byte_uptr(bytes.release());
    }
}

void ProgramBinaryElement::dump(const std::string& title) {
    if (!title.empty()) {
        printf("=== %s ===\n", title.c_str());
    } else {
        printf("=== ProgramBinaryElement Dump ===\n");
    }
    
    printf("  Raw Data: %zu bytes\n", m_rawData.size());
    printf("  Reloc Table: %zu bits\n", m_relocTable.size());
    printf("  String Offsets: %zu\n", m_stringOffsets.size());
    
    // Entry
    printf("  Entry: {\n");
    printf("    nameID: {}\n", StringIdManager::instance().get_cstring(m_entry.m_nameID));
    printf("    typeId: {}\n", StringIdManager::instance().get_cstring(m_entry.m_typeId));
    printf("    ptr: %p\n", m_entry.m_entryPtr);
    printf("  }\n");
    
    // Raw data hex dump
    printf("  Raw Data (hex):\n");
    size_t dump_size = std::min(m_rawData.size(), size_t(128));
    for (size_t i = 0; i < dump_size; i++) {
        if (i % 16 == 0) printf("    %04zx: ", i);
        printf("%02X ", static_cast<unsigned char>(m_rawData[i]));
        if ((i + 1) % 8 == 0 && (i + 1) % 16 != 0) printf(" ");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (dump_size % 16 != 0) printf("\n");
    if (m_rawData.size() > 128) printf("    ... (%zu more bytes)\n", m_rawData.size() - 128);
    
    // Relocation table summary
    size_t reloc_count = 0;
    for (bool b : m_relocTable) if (b) reloc_count++;
    printf("  Relocations: %zu / %zu bits (%.1f%%)\n", 
           reloc_count, m_relocTable.size(), 
           m_relocTable.empty() ? 0 : 100.0 * reloc_count / m_relocTable.size());
    
    printf("=== End Dump ===\n");
}