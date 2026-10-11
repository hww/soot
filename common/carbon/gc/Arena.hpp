#pragma once

#include "CommonTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace carbon::gc {

    class Arena {
    public:
        Arena(std::byte *start, std::byte *end, std::size_t block_size, std::size_t align);

        void *allocate();
        void  deallocate(void *block);

        // ---- Интроспекция ----
        [[nodiscard]] std::size_t block_size() const noexcept { return m_block_size; }
        [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }
        [[nodiscard]] std::size_t used() const noexcept { return m_used; }
        [[nodiscard]] std::size_t free_count() const noexcept { return m_free_count; }

        [[nodiscard]] std::byte *begin() const noexcept { return m_data_begin; }
        [[nodiscard]] std::byte *end() const noexcept { return m_data_end; }

        [[nodiscard]] bool owns(const void *ptr) const noexcept {
            auto *p = static_cast<const std::byte *>(ptr);
            return p >= m_data_begin && p < m_data_end;
        }

        [[nodiscard]] std::byte *region_start() const noexcept { return m_region_start; }
        [[nodiscard]] std::byte *region_end() const noexcept { return m_region_end; }

        /// @brief Занят ли блок по индексу (для sweep).
        [[nodiscard]] bool is_allocated(std::size_t idx) const noexcept {
            return m_bitmap[idx >> 3] & (1u << (idx & 7));
        }

        /// @brief Количество блоков (capacity).
        [[nodiscard]] std::size_t total_blocks() const noexcept { return m_capacity; }

    private:
        void set_allocated(std::size_t idx, bool val) noexcept;

        std::byte  *m_region_start;
        std::byte  *m_region_end;
        std::byte  *m_data_begin;
        std::byte  *m_data_end;
        std::byte  *m_bump;
        void       *m_free_head;
        std::size_t m_block_size;
        std::size_t m_align;
        std::size_t m_capacity;
        std::size_t m_used;
        std::size_t m_free_count;

        // Bitmap: 1 бит на блок. 1 = занят, 0 = свободен.
        std::vector<std::uint8_t> m_bitmap;
    };

} // namespace carbon::gc