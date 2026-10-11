#include "gc/ArenaManager.hpp"
#include "util/Log.hpp"

#include <cassert>
#include <cstring>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace carbon::gc {

    // ------------------------------------------------------------------------
    // Size class helpers
    // ------------------------------------------------------------------------

    int size_class_for(std::size_t size) noexcept {
        for (int i = 0; i < static_cast<int>(kNumSizeClasses); ++i) {
            if (size <= kSizeClasses[i]) return i;
        }
        return -1;
    }

    std::size_t size_of_class(int idx) noexcept {
        if (idx < 0 || idx >= static_cast<int>(kNumSizeClasses)) return 0;
        return kSizeClasses[idx];
    }

    // ------------------------------------------------------------------------
    // Region reservation
    // ------------------------------------------------------------------------
    //
    // Мы резервируем `max_mb` виртуального адресного пространства.
    // На Linux — mmap(PROT_NONE). На Windows — VirtualAlloc(MEM_RESERVE).
    // Реальная память выделяется по мере записи (page fault сам коммитит).
    //
    static std::byte *reserve_region(std::size_t bytes) {
#if defined(_WIN32)
        void *p = VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
        if (!p) return nullptr;
        // Коммитим первый chunk — иначе запись упадёт.
        // Пусть будет initial_mb (не max_mb). Пока просто резервируем всё.
        void *c = VirtualAlloc(p, bytes, MEM_COMMIT, PAGE_READWRITE);
        if (!c) {
            VirtualFree(p, 0, MEM_RELEASE);
            return nullptr;
        }
        return static_cast<std::byte *>(p);
#else
        void *p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (p == MAP_FAILED) return nullptr;
        return static_cast<std::byte *>(p);
#endif
    }

    static void release_region(std::byte *p, std::size_t bytes) {
#if defined(_WIN32)
        VirtualFree(p, 0, MEM_RELEASE);
#else
        munmap(p, bytes);
#endif
    }

    // ------------------------------------------------------------------------
    // ArenaManager
    // ------------------------------------------------------------------------

    ArenaManager::ArenaManager(std::size_t initial_mb, std::size_t max_mb)
        : m_region_start(nullptr), m_region_end(nullptr), m_bump(nullptr), m_region_size(0),
          m_arena_size(0) {
        if (max_mb < initial_mb) max_mb = initial_mb;
        if (max_mb == 0) max_mb = 1024;
        if (initial_mb == 0) initial_mb = 16;

        m_region_size = max_mb * 1024ull * 1024ull;
        m_region_start = reserve_region(m_region_size);
        if (!m_region_start) {
            lg::error("gc::ArenaManager: failed to reserve {} MB", max_mb);
            m_region_size = 0;
            return;
        }
        m_region_end = m_region_start + m_region_size;
        m_bump = m_region_start;

        // Размер арены по умолчанию — 1 MB. Для больших классов
        // арены будут крупнее (пропорционально размеру блока),
        // но начнём с фиксированного.
        m_arena_size = 1ull * 1024ull * 1024ull;

        lg::info("gc::ArenaManager: reserved {} MB", max_mb);
    }

    ArenaManager::~ArenaManager() {
        if (m_region_start) {
            release_region(m_region_start, m_region_size);
            m_region_start = nullptr;
            m_region_end = nullptr;
            m_bump = nullptr;
        }
    }

    Arena *ArenaManager::grow_class(int idx) {
        assert(idx >= 0 && idx < static_cast<int>(kNumSizeClasses));

        const std::size_t block_size = kSizeClasses[idx];
        std::size_t       arena_size = m_arena_size;

        // Для больших классов — арена чуть больше блока, иначе теряем
        // слишком много места на "хвост" из-за выравнивания.
        if (block_size > arena_size / 8) {
            arena_size = block_size * 64; // минимум 64 блока
        }

        // Проверяем, хватает ли места в регионе.
        std::size_t remaining = static_cast<std::size_t>(m_region_end - m_bump);
        if (remaining < arena_size) {
            lg::warn("gc::ArenaManager: region exhausted (need {}, have {})", arena_size,
                     remaining);
            return nullptr;
        }

        std::byte *start = m_bump;
        std::byte *end = start + arena_size;
        m_bump = end;

        auto arena = std::make_unique<Arena>(start, end, block_size,
                                             /*align=*/block_size < 16 ? 16 : block_size);

        Arena *raw = arena.get();
        m_arenas[idx].push_back(std::move(arena));
        m_all_arenas.push_back(raw);

        lg::debug("gc::ArenaManager: grew class {} (block={}, size={})", idx, block_size,
                  arena_size);
        return raw;
    }

    void *ArenaManager::allocate(std::size_t size, std::size_t align) {
        (void)align; // выравнивание уже обеспечено внутри арены

        int idx = size_class_for(size);
        if (idx < 0) {
            lg::error("gc::ArenaManager: size {} exceeds max class ({})", size,
                      kSizeClasses[kNumSizeClasses - 1]);
            return nullptr;
        }

        // Пробуем последнюю арену этого класса.
        auto &arenas = m_arenas[idx];
        if (!arenas.empty()) {
            void *block = arenas.back()->allocate();
            if (block) return block;
        }

        // Растим новый класс.
        Arena *a = grow_class(idx);
        if (!a) return nullptr;

        return a->allocate();
    }

    void ArenaManager::deallocate(void *ptr) {
        if (!ptr) return;
        Arena *a = find_arena(ptr);
        if (!a) {
            lg::warn("gc::ArenaManager: deallocate of foreign pointer {}", ptr);
            return;
        }
        a->deallocate(ptr);
    }

    bool ArenaManager::owns(const void *ptr) const noexcept {
        auto *p = static_cast<const std::byte *>(ptr);
        return p >= m_region_start && p < m_region_end;
    }

    Arena *ArenaManager::find_arena(const void *ptr) const noexcept {
        // Пока — линейный поиск. Позже можно заменить на дерево
        // интервалов, если арен станет очень много.
        for (Arena *a : m_all_arenas) {
            if (a->owns(ptr)) return a;
        }
        return nullptr;
    }

    std::size_t ArenaManager::committed() const noexcept {
        return static_cast<std::size_t>(m_bump - m_region_start);
    }

    std::size_t ArenaManager::live_count() const noexcept {
        std::size_t total = 0;
        for (Arena *a : m_all_arenas) { total += a->used(); }
        return total;
    }

} // namespace carbon::gc