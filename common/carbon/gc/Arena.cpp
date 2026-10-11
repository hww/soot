#include "gc/Arena.hpp"

#include <cassert>
#include <cstring>

namespace carbon::gc {

    Arena::Arena(std::byte *start, std::byte *end, std::size_t block_size, std::size_t align)
        : m_region_start(start), m_region_end(end), m_block_size(block_size), m_align(align),
          m_capacity(0), m_used(0), m_free_count(0) {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(start);
        p = (p + align - 1) & ~(align - 1);
        m_data_begin = reinterpret_cast<std::byte *>(p);

        std::uintptr_t end_p = reinterpret_cast<std::uintptr_t>(end);
        end_p &= ~(align - 1);

        if (end_p <= p) {
            m_data_end = m_data_begin;
            m_bump = m_data_begin;
            m_free_head = nullptr;
            return;
        }

        std::size_t bytes = end_p - p;
        m_capacity = bytes / block_size;
        m_data_end = m_data_begin + m_capacity * block_size;
        m_bump = m_data_begin;
        m_free_head = nullptr;

        // Bitmap: ceil(capacity / 8) байт.
        m_bitmap.assign((m_capacity + 7) / 8, 0);
    }

    void Arena::set_allocated(std::size_t idx, bool val) noexcept {
        auto      &byte = m_bitmap[idx >> 3];
        const auto mask = static_cast<std::uint8_t>(1u << (idx & 7));
        if (val) byte |= mask;
        else
            byte &= ~mask;
    }

    void *Arena::allocate() {
        // Сначала freelist.
        if (m_free_head) {
            void *block = m_free_head;
            m_free_head = *reinterpret_cast<void **>(m_free_head);
            --m_free_count;
            ++m_used;

            std::size_t idx = (static_cast<std::byte *>(block) - m_data_begin) / m_block_size;
            set_allocated(idx, true);
            return block;
        }

        // Bump.
        if (m_bump + m_block_size <= m_data_end) {
            void *block = m_bump;
            m_bump += m_block_size;
            ++m_used;

            std::size_t idx = (static_cast<std::byte *>(block) - m_data_begin) / m_block_size;
            set_allocated(idx, true);
            return block;
        }

        return nullptr;
    }

    void Arena::deallocate(void *block) {
        if (!owns(block)) return;

        std::size_t idx = (static_cast<std::byte *>(block) - m_data_begin) / m_block_size;
        set_allocated(idx, false);

        *reinterpret_cast<void **>(block) = m_free_head;
        m_free_head = block;
        ++m_free_count;
        --m_used;
    }

} // namespace carbon::gc