// DCHeader.hpp

#pragma once

#include "CommonTypes.hpp"
#include "fmt/format.h"
#include <string>


// Thanks to icemesh!

namespace carbon {

    /// @brief One entry in the DC entry table (array of these follows the header).
    /// @details The entry table itself is preceded by a sid64 marker SID("array").
    ///          Entry[0] starts at file offset 0x28 (sizeof(DC_Header) + sizeof(sid64)).
    struct DCEntry {
        sid64 m_nameID;         ///< <c>0x00</c>: SID64 of the entry name (e.g. SID("#7C28D25188889230"))
        sid64 m_typeId;         ///< <c>0x08</c>: SID64 of the entry type (SID("script-lambda"),
                                ///< SID("state-script"), ...)
        const void *m_entryPtr; ///< <c>0x10</c>: pointer to the payload; cast to
                                ///< ScriptLambda*/StateScript*/etc. based on m_typeId

        std::string to_string() const {
            return fmt::format("<Entry name 0x{:016X} type 0x{:016X} ptr {:p}>", m_nameID, m_typeId,
                               m_entryPtr);
        }
    };

    /// @brief DC container header. Always located at the start of a .bin DC file.
    /// @details File layout (offsets from file start):
    ///            0x00  DC_Header            (0x20 bytes)
    ///            0x20  sid64 SID("array")   (8-byte array marker)
    ///            0x28  DCEntry[m_numEntries]
    ///            ...   function bodies (ScriptLambda + instructions + symbol tables)
    ///            m_stringsOffset  string table
    ///            m_textSize       relocation table (u32 size + bitmap)
    struct DC_Header {
        uint32_t m_magic;           ///< <c>0x00</c>: magic, must be 0x44433030 ("DC00")
        uint32_t m_versionNumber;   ///< <c>0x04</c>: format version, always 0x1
        uint32_t m_textSize;        ///< <c>0x08</c>: size of the whole relocatable block (header + entries
                                    ///< + bodies + strings + padding);
                                    ///<                  the relocation table starts exactly at this offset
        uint32_t m_stringsOffset;   ///< <c>0x0C</c>: absolute offset of the string table from the
                                    ///< start of the file (== data_size)

        uint32_t m_always1;         ///< <c>0x10</c>: always 1
        uint32_t m_numEntries;      ///< <c>0x14</c>: number of DCEntry records
        DCEntry *m_pStartOfData;    ///< <c>0x18</c>: pointer to Entry[0] (the entry table), not to the
                                    ///< payload
    };

    static constexpr u32 DC_FILE_MAGIC = 0x44433030;
    static constexpr u32 DC_FILE_VERSION = 0x01;
}



