#pragma once

#include "CommonTypes.hpp"

#include <cstring>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace sootc {

    // =========================================================================
    // StringPool
    // =========================================================================
    //
    // A deduplicated pool of null-terminated strings. Each string is
    // appended once; subsequent adds of the same content return the
    // existing offset. Offsets are byte positions inside the pool's
    // own buffer, not file offsets.
    //
    // The buffer is a flat byte array. Strings are stored back-to-back,
    // each followed by '\0'. No alignment is applied — strings have no
    // alignment requirement, and putting them first in the file keeps
    // the pool compact.
    class StringPool {
    public:
        /// @brief Add `s` if not present; return its offset in the pool.
        /// @details The offset is stable for the lifetime of the pool.
        ///          Adding the same string twice returns the same offset.
        u32 lookup_or_add(const std::string &s) {
            auto it = m_index.find(s);
            if (it != m_index.end()) return it->second;

            const u32 offset = static_cast<u32>(m_bytes.size());
            m_bytes.insert(m_bytes.end(), s.begin(), s.end());
            m_bytes.push_back('\0');
            m_offsets.push_back(offset);
            m_index[s] = offset;
            return offset;
        }

        /// @brief Return the offset of an already-added string.
        /// @throws std::runtime_error if the string is not present.
        u32 get_offset(const std::string &s) const {
            auto it = m_index.find(s);
            if (it == m_index.end()) {
                throw std::runtime_error("StringPool: string not present: " + s);
            }
            return it->second;
        }

        [[nodiscard]] bool empty() const noexcept { return m_bytes.empty(); }
        [[nodiscard]] u64  size() const noexcept { return m_bytes.size(); }

        [[nodiscard]] const std::vector<char> &bytes() const noexcept { return m_bytes; }
        [[nodiscard]] const std::vector<u32>  &offsets() const noexcept { return m_offsets; }

        [[nodiscard]] const char *get_at(u32 offset) const noexcept {
            return m_bytes.data() + offset;
        }

    private:
        std::vector<char>                    m_bytes;   ///< flat byte buffer
        std::vector<u32>                     m_offsets; ///< start offset of each string
        std::unordered_map<std::string, u32> m_index;   ///< content -> offset
    };

    // =========================================================================
    // BlobPool
    // =========================================================================
    //
    // A non-deduplicated pool of arbitrary binary blobs. Used for
    // anonymous static structures referenced from a pointer field of
    // another static structure. Each appended blob is aligned to 8
    // bytes so a pointer can address it.
    //
    // Blobs are stored back-to-back. The 8-byte alignment is achieved
    // by padding between blobs with zero bytes. There is no indexing:
    // callers remember the offset they got back from append() and
    // store it in the referring pointer slot.
    class BlobPool {
    public:
        /// @brief Append a blob and return its 8-byte-aligned offset
        ///        inside the pool.
        ///
        /// @details The pool is padded with zeros to the next 8-byte
        ///          boundary before the blob is written, so the
        ///          returned offset is always a multiple of 8. The
        ///          blob itself is written verbatim, no terminator.
        ///
        ///          Blobs are never deduplicated: two identical blobs
        ///          appended twice occupy two different offsets. This
        ///          is deliberate — anonymous structures are identified
        ///          by identity (the pointer slot that refers to them),
        ///          not by content.
        ///
        /// @return offset of the blob inside the pool (multiple of 8).
        u32 append(const void *data, size_t size) {
            // Pad to the next 8-byte boundary.
            while (m_bytes.size() % 8 != 0) { m_bytes.push_back('\0'); }

            const u32 offset = static_cast<u32>(m_bytes.size());

            if (size > 0) {
                const char *p = static_cast<const char *>(data);
                m_bytes.insert(m_bytes.end(), p, p + size);
            }
            return offset;
        }

        [[nodiscard]] bool empty() const noexcept { return m_bytes.empty(); }
        [[nodiscard]] u64  size() const noexcept { return m_bytes.size(); }

        [[nodiscard]] const std::vector<char> &bytes() const noexcept { return m_bytes; }

    private:
        std::vector<char> m_bytes;
    };

    // =========================================================================
    // DataSegment
    // =========================================================================
    //
    // Per-file data segment: a facade over two independent pools.
    //
    //     * StringPool  — deduplicated, null-terminated strings.
    //                     Referenced from a string-constant slot in a
    //                     function's constant pool.
    //
    //     * BlobPool    — non-deduplicated, 8-byte-aligned blobs.
    //                     Referenced from a struct-pointer slot in a
    //                     static structure's payload.
    //
    // The two pools are kept separate all the way to file assembly.
    // FileNode::make_binary writes them as two adjacent sections:
    //
    //     [ ... entry payloads ... ]
    //     [ string pool ]              <- starts at m_stringsOffset
    //     [ padding to 8 ]             <- aligns the blob section
    //     [ blob pool ]                <- starts at m_blobsOffset
    //     [ padding to 4 ]             <- aligns the reloc table
    //     [ relocation table ]         <- starts at m_textSize
    //
    // Keeping the sections separate is what makes the file inspector
    // work: it can walk the string pool from m_stringsOffset to
    // m_blobsOffset (or to m_textSize when there are no blobs) without
    // ever touching a binary blob and trying to print it as text.
    //
    // The segment also carries a small index from string content to
    // (kind, offset) so callers can ask "where is this string?" with
    // a single lookup. Blobs have no names, so they are not indexed;
    // callers keep the offset returned by add_blob().
    class DataSegment {
    public:
        // ---- Strings ----

        /// @brief Add a string to the string pool.
        /// @return offset within the string pool.
        u32 add_string(const std::string &s) { return m_strings.lookup_or_add(s); }

        /// @brief Return the offset of an already-added string.
        /// @throws std::runtime_error if not present.
        u32 find_string(const std::string &s) const { return m_strings.get_offset(s); }

        // ---- Assembly-side accessors ----

        [[nodiscard]] const StringPool &strings() const noexcept { return m_strings; }

        [[nodiscard]] bool has_strings() const noexcept { return !m_strings.empty(); }

        [[nodiscard]] u64 string_size() const noexcept { return m_strings.size(); }

    private:
        StringPool m_strings;
    };

} // namespace sootc