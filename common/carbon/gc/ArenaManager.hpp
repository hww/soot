#pragma once

#include "CommonTypes.hpp"
#include "gc/Arena.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace carbon::gc {

    // ========================================================================
    // Size-classes
    // ========================================================================
    //
    // Округляем размер запроса до ближайшего класса. Классы — степени двойки.
    // Внутри каждого класса — своя арена (или несколько, если одна
    // заполнилась). Все арены лежат в одном регионе.
    //
    constexpr std::size_t kNumSizeClasses = 9;
    constexpr std::size_t kSizeClasses[kNumSizeClasses] = {16,  32,   64,   128, 256,
                                                           512, 1024, 2048, 4096};

    /// @brief Вернуть индекс класса для запрошенного размера.
    ///        -1, если размер больше максимального класса.
    int size_class_for(std::size_t size) noexcept;

    /// @brief Вернуть размер класса по индексу.
    std::size_t size_of_class(int idx) noexcept;

    // ========================================================================
    // ArenaManager
    // ========================================================================
    //
    // Держит:
    //   * регион памяти (один mmap / VirtualAlloc);
    //   * по одной или более Arena на каждый size-class;
    //   * внутренний bump-указатель региона, чтобы нарезать арены.
    //
    // Регион резервируется целиком при инициализации. Реальная память
    // коммитится по мере роста (mmap сам разбирается, mprotect не нужен,
    // если мы просто пишем в зарезервированную область).
    //
    // API:
    //   * allocate(size, align) — вернуть блок нужного размера из
    //     соответствующей арены. Внутри — с учётом GCHeader, который
    //     вызывающий добавляет сам.
    //   * deallocate(ptr) — вернуть блок в арену-владельца.
    //   * owns(ptr) — проверить, что ptr в регионе.
    //   * class_arenas(idx) — получить все арены класса (для sweep).
    //
    class ArenaManager {
    public:
        ArenaManager(std::size_t initial_mb, std::size_t max_mb);
        ~ArenaManager();

        ArenaManager(const ArenaManager &) = delete;
        ArenaManager &operator=(const ArenaManager &) = delete;

        /// @brief Выделить блок размером `size` (payload).
        /// @param size   размер payload
        /// @param align  выравнивание
        /// @return указатель на блок или nullptr
        void *allocate(std::size_t size, std::size_t align);

        /// @brief Вернуть блок в арену-владельца.
        void deallocate(void *ptr);

        /// @brief Проверить, что указатель принадлежит региону.
        [[nodiscard]] bool owns(const void *ptr) const noexcept;

        /// @brief Найти арену, которой принадлежит указатель.
        [[nodiscard]] Arena *find_arena(const void *ptr) const noexcept;

        /// @brief Доступ к аренам конкретного класса (для sweep).
        [[nodiscard]] const std::vector<std::unique_ptr<Arena>> &
        arenas_of_class(int idx) const noexcept {
            return m_arenas[idx];
        }

        /// @brief Все арены — плоский список. Для итерации без разбора классов.
        [[nodiscard]] const std::vector<Arena *> &all_arenas() const noexcept {
            return m_all_arenas;
        }

        /// @brief Сколько памяти зарезервировано под регион.
        [[nodiscard]] std::size_t region_size() const noexcept { return m_region_size; }

        /// @brief Сколько памяти уже нарезано под арены.
        [[nodiscard]] std::size_t committed() const noexcept;

        /// @brief Сколько блоков выдано (приблизительно, для статистики).
        [[nodiscard]] std::size_t live_count() const noexcept;

    private:
        /// @brief Создать новую арену под класс `idx`.
        /// @return арена или nullptr, если регион кончился.
        Arena *grow_class(int idx);

        std::byte  *m_region_start;
        std::byte  *m_region_end;
        std::byte  *m_bump; // куда нарезать следующую арену
        std::size_t m_region_size;

        // m_arenas[idx] — все арены класса idx.
        std::vector<std::unique_ptr<Arena>> m_arenas[kNumSizeClasses];
        // Плоский список для быстрого обхода.
        std::vector<Arena *>                m_all_arenas;

        // Размер арены по умолчанию для каждого класса (подстраивается).
        std::size_t m_arena_size;
    };

} // namespace carbon::gc