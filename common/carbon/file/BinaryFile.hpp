#pragma once
#include "CommonTypes.hpp"
#include "DCHeader.hpp"
#include "DCScript.hpp"
#include "common/carbon/lib/SIDBase.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "lib/ByteUtils.hpp"


#include <memory>
#include <string>
#include <map>
#include <set>
#include <iostream>
#include "ProgramBinaryElement.hpp"

namespace carbon {

    /// @brief Lightweight typed pointer into the mapped file buffer.
    /// @details Wraps a raw byte pointer and provides typed read helpers.
    ///          All accessors are noexcept and do not bounds-check.
    struct location {
        const std::byte *m_ptr = nullptr;

        location() noexcept = default;
        location(const void *ptr) noexcept : m_ptr(reinterpret_cast<const std::byte *>(ptr)) {}

        /// Rebind this location to (rhs + offset).
        [[nodiscard]] location &from(const location &rhs, const i32 offset = 0) noexcept {
            m_ptr = rhs.get<std::byte *>() + offset;
            return *this;
        }

        /// Reinterpret as T* at (this + offset).
        template <typename T> [[nodiscard]] const T *as(const i32 offset = 0) const noexcept {
            return reinterpret_cast<const T *>(m_ptr + offset);
        }

        /// Read T at (this + offset).
        template <typename T> [[nodiscard]] const T &get(const i32 offset = 0) const noexcept {
            return *reinterpret_cast<const T *>(m_ptr + offset);
        }

        /// Raw pointer as an integer.
        [[nodiscard]] p64 num() const noexcept { return reinterpret_cast<p64>(m_ptr); }

        /// Round down to the nearest 8-byte boundary.
        [[nodiscard]] location aligned() const noexcept { return location(m_ptr - num() % 8); }

        [[nodiscard]] location operator+(const u64 rhs) const noexcept {
            return location(m_ptr + rhs);
        }
        [[nodiscard]] location operator-(const u64 rhs) const noexcept {
            return location(m_ptr - rhs);
        }

        [[nodiscard]] bool operator>(const location &rhs) const noexcept {
            return num() > rhs.num();
        }
        [[nodiscard]] bool operator>=(const location &rhs) const noexcept {
            return num() >= rhs.num();
        }

        [[nodiscard]] bool is_aligned() const noexcept { return num() % 8 == 0; }
    };


     /// @brief Kind of value stored in a symbol-table entry.
    enum class symbol_type {
        B8,     ///< bool*
        I32,    ///< i32*
        F32,    ///< f32*
        SS,     ///< StateScript*
        HASH,   ///< u64* (SID)
        LAMBDA, ///< ScriptLambda*
        UNKNOWN ///< type not determined
    };

    /// @brief One symbol-table entry. `type` selects the active pointer in the union.
    struct symbol {
        symbol_type type; ///< active kind
        sid64       id;   ///< SID64 of the symbol name

        union {
            i32          *i32_ptr;    ///< valid when type == I32
            f32          *f32_ptr;    ///< valid when type == F32
            bool         *b8_ptr;     ///< valid when type == B8
            StateScript  *ss_ptr;     ///< valid when type == SS
            ScriptLambda *lambda_ptr; ///< valid when type == LAMBDA
            uint64_t     *hash_ptr;   ///< valid when type == HASH
            DCEntry       raw_entry;  ///< raw entry for debugging / round-trip
        };
    };

    /// @brief One data-struct entry with its serialised layout.
    /// @details Populated at load time from the entry table. The layout comes
    ///          from ProgramBinaryElement::m_structLayout (attached at generation
    ///          time by DataDeclarationNode), so BinaryFileInspector can decode
    ///          the payload WITHOUT consulting TypeSystem.
    struct DataStructEntry {
        std::string      name;      ///< entry name, e.g. "foo"
        std::string      type_name; ///< type name, e.g. "vec"
        u64              offset;    ///< file offset of the payload
        StructLayoutInfo layout;    ///< field layout
    };

    /// @brief In-memory representation of a loaded DC file.
    /// @details Owns the mapped bytes and provides typed access to the header,
    ///          relocation table, string table, and entry list.
    class BinaryFile
    {
    public:
        /// @brief Classification of an entry payload, based on its m_typeId.
        enum class EntryKind {
            Unknown,      ///< unrecognised typeId
            ScriptLambda, ///< m_typeId == SID("script-lambda")
            StateScript,  ///< m_typeId == SID("state-script")
            SsType,       ///< m_typeId == SID("ss-type")
            Map,          ///< m_typeId == SID("map") or SID("map-32")
            DataStruct,   ///< any other non-zero typeId (raw payload)
        };

        BinaryFile() = default;

        BinaryFile(std::filesystem::path path, const u64 size, byte_uptr &&bytes,
                   DC_Header *dcheader) noexcept
            : m_path(std::move(path)), m_dcheader(dcheader), m_size(size),
              m_bytes(std::move(bytes)) {}

        BinaryFile(const BinaryFile &) = delete;
        BinaryFile &operator=(const BinaryFile &) = delete;
        BinaryFile(BinaryFile &&) noexcept = default;
        BinaryFile &operator=(BinaryFile &&) noexcept = default;
        ~BinaryFile() = default;

        [[nodiscard]] static std::expected<BinaryFile, std::string>
        from_path(const std::filesystem::path &path) noexcept;

        [[nodiscard]] static std::expected<BinaryFile, std::string>
        from_buffer(const std::filesystem::path &path, byte_uptr bytes, size_t size) noexcept;

        /// Re-serialise to disk, mapping relocation pointers back to file offsets.
        [[nodiscard]] bool save(const std::filesystem::path &path) noexcept;

        // -------------------------------------------------------------------
        // Entry table access
        // -------------------------------------------------------------------

        /// @return pointer to Entry[0], or nullptr if the header is missing.
        [[nodiscard]] const DCEntry *entries() const noexcept;

        /// @return number of entries, or 0 if the header is missing.
        [[nodiscard]] u32 entry_count() const noexcept;

        /// @brief Find the first entry with m_nameID == name.
        /// @return pointer to the entry, or nullptr if not found.
        [[nodiscard]] const DCEntry *find_entry_by_name(sid64 name) const noexcept;

        // -------------------------------------------------------------------
        // Entry payload
        // -------------------------------------------------------------------

        /// @brief Classify an entry by its typeId.
        [[nodiscard]] static EntryKind entry_kind(const DCEntry &entry) noexcept;

        /// @return the entry payload as ScriptLambda*, or nullptr if kind != ScriptLambda.
        [[nodiscard]] const ScriptLambda *entry_as_lambda(const DCEntry &entry) const noexcept;

        /// @return the entry payload as StateScript*, or nullptr if kind != StateScript.
        [[nodiscard]] const StateScript *entry_as_state_script(const DCEntry &entry) const noexcept;

        /// @return the entry payload as SsType*, or nullptr if kind != SsType.
        [[nodiscard]] const SsType *entry_as_ss_type(const DCEntry &entry) const noexcept;

        // -------------------------------------------------------------------
        // SID and string helpers
        // -------------------------------------------------------------------

        /// @brief Resolve a SID64 to a string via m_sidCache, then StringIdManager.
        [[nodiscard]] std::string resolve_sid(sid64 id) const;

        /// @brief Read a null-terminated string at an absolute file location.
        /// @return the string, or "" if the location is outside the string table.
        [[nodiscard]] std::string read_string_at(location loc) const;

        // -------------------------------------------------------------------
        // Debug
        // -------------------------------------------------------------------

        /// @brief Verify structural invariants. Returns a list of violations.
        [[nodiscard]] std::vector<std::string> validate() const;

        /// @brief Print the entry table (name, type, ptr, kind) to `os`.
        void dump_entries(std::ostream &os = std::cout) const;

        std::filesystem::path               m_path;           ///< source path (for diagnostics)
        const DC_Header*                    m_dcheader = nullptr; ///< pointer into m_bytes
        const StateScript*                  m_dcscript = nullptr; ///< set by the disassembler when a state-script is found
        std::size_t                         m_size = 0;       ///< size of the mapped buffer in bytes
        byte_uptr                           m_bytes;          ///< owned file bytes
        byte_uptr                           m_pointedAtTable; ///< bitmap: which file offsets are pointed at
        location                            m_strings;        ///< start of the string table
        location                            m_relocTable;     ///< start of the relocation bitmap (after its u32 size)
        std::map<sid64, const std::string>  m_sidCache;       ///< SID -> resolved name
        std::set<p64>                       m_emittedStructs; ///< used by emit-once mode
        std::vector<DataStructEntry>        m_dataStructs;    ///< populated at load time

        /// @return true if the given location is a relocated pointer (not a raw value).
        [[nodiscard]] bool is_file_ptr(location loc) const noexcept;

        /// @return true if the given location points into the string table.
        [[nodiscard]] bool is_string(location loc) const noexcept;

        /// @return true if the given location is the target of at least one relocated pointer.
        [[nodiscard]] bool gets_pointed_at(location loc) const noexcept;

        /// @return a copy of the file bytes with all relocated pointers mapped back to file offsets.
        [[nodiscard]] byte_uptr get_unmapped() const;

    private:
        /// Parse the relocation bitmap at m_dcheader->m_textSize and apply relocations.
        void read_reloc_table() noexcept;

        /// Replace '\n' with ' ' inside the string table (the game does this too).
        void replace_newlines_in_stringtable() noexcept;

        /// @return true if [ptr, ptr + size) lies within the mapped file buffer.
        [[nodiscard]] bool is_valid_ptr(const void *ptr, size_t size) const noexcept;
    };

}
