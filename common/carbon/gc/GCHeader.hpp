#pragma once

#include "CommonTypes.hpp"
#include "lib/StringId.hpp"

#include <cstddef>
#include <cstdint>

namespace carbon::gc {

    struct TypeDescriptor; // forward

    // ========================================================================
    // GCHeader — заголовок перед каждым объектом в heap.
    // ========================================================================
    //
    // Layout (32 байта, выравнивание 16):
    //
    //   +------------------+------------------+
    //   | GCHeader (32 b)  |  payload (N b)   |
    //   +------------------+------------------+
    //   ^                  ^
    //   header             user_ptr (возвращается из alloc)
    //
    // GCHeader получается из user_ptr как  header = user_ptr - sizeof(GCHeader).
    //
    // Поля:
    //   pass_cycle  — номер прохода GC, в котором объект был помечен.
    //                 0 = объект создан после начала текущего прохода
    //                     (не удаляется в этом проходе).
    //   size_class  — индекс класса в таблице kSizeClasses.
    //   flags       — флаги объекта (см. GCHeaderFlags).
    //   type_desc   — указатель на TypeDescriptor. nullptr = объект
    //                 не сканируется (не содержит указателей).
    //   reserved    — выравнивание до 32 байт.
    //
    enum GCHeaderFlags : u32 {
        GC_FLAG_MARKED = 1u << 0, // помечен в текущем проходе (не обязательно)
        GC_FLAG_DEAD = 1u << 1,   // явно освобождён через free()
        GC_FLAG_PINNED = 1u << 2, // зарезервировано
    };

    struct alignas(16) GCHeader {
        u64             pass_cycle;
        u32             size_class;
        u32             flags;
        TypeDescriptor *type_desc;
        u64             reserved;
    };

    static_assert(sizeof(GCHeader) == 32, "GCHeader must be 32 bytes");
    static_assert(alignof(GCHeader) == 16, "GCHeader must be 16-aligned");

    /// @brief Получить заголовок по указателю на payload.
    [[nodiscard]] inline GCHeader *header_of(void *payload) noexcept {
        return reinterpret_cast<GCHeader *>(static_cast<std::byte *>(payload) - sizeof(GCHeader));
    }

    [[nodiscard]] inline const GCHeader *header_of(const void *payload) noexcept {
        return reinterpret_cast<const GCHeader *>(static_cast<const std::byte *>(payload) -
                                                  sizeof(GCHeader));
    }

    /// @brief Получить payload по указателю на заголовок.
    [[nodiscard]] inline void *payload_of(GCHeader *header) noexcept {
        return static_cast<std::byte *>(static_cast<void *>(header)) + sizeof(GCHeader);
    }

} // namespace carbon::gc