#pragma once

#include "CommonTypes.hpp"
#include "gc/ArenaManager.hpp"
#include "gc/GCHeader.hpp"
#include "gc/TypeRegistry.hpp"
#include "lib/StringId.hpp"

#include <cstddef>
#include <cstdint>

namespace carbon::gc {

    // ========================================================================
    // Lifecycle
    // ========================================================================

    void initialize(std::size_t initial_mb = 16, std::size_t max_mb = 1024);
    void shutdown();

    // ========================================================================
    // Allocation
    // ========================================================================

    void *alloc(sid64 sid, std::size_t size = 0, std::size_t align = 0);
    void  free(void *ptr);

    [[nodiscard]] bool is_in_heap(const void *ptr);

    // ========================================================================
    // Pass API
    // ========================================================================
    //
    // Один проход GC:
    //
    //   u64 pass = gc::start_pass();     // новый проход, возвращает его id
    //
    //   // VM помечает корни. Для каждого heap-указателя — mark_object.
    //   for (auto* f : all_frames) {
    //       if (f->mark_id == pass) continue;
    //       f->mark_id = pass;
    //       for (auto& reg : f->registers) {
    //           if (reg.is_ptr()) {
    //               void* p = reg.get_ptr();
    //               if (gc::is_in_heap(p)) gc::mark_object(p);
    //           }
    //       }
    //   }
    //
    //   // Sweep — по частям или до конца.
    //   while (!gc::sweep_pass(1024)) {}
    //
    //   gc::finish_pass();
    //
    // Между start_pass и finish_pass VM не должна вызывать alloc? Нет —
    // может. Новые объекты получают pass_cycle = 0 (см. alloc), поэтому
    // не удаляются в этом проходе.

    /// @brief Начать новый проход. Возвращает id прохода (>= 1).
    u64 start_pass();

    /// @brief Пометить объект и всё, на что он ссылается (рекурсивно).
    ///        Если ptr не в heap — no-op.
    void mark_object(void *payload);

    /// @brief Обработать до budget блоков в sweep-фазе.
    /// @return true, если sweep завершён.
    bool sweep_pass(int budget = 64);

    /// @brief Завершить проход. Сбросить состояние в IDLE.
    void finish_pass();

    /// @brief Текущий id прохода. 0 — GC не работает.
    [[nodiscard]] u64 current_pass();

    /// @brief Склолько живых
    [[nodiscard]] std::size_t live_objects();

    // ========================================================================
    // Full collection (STW, без корней — только sweep)
    // ========================================================================
    //
    // Для тестов: start_pass + sweep_pass(∞) + finish_pass.
    // Не помечает ничего, поэтому соберёт всё, что не помечено вручную.
    void collect();

    // ========================================================================
    // Queries
    // ========================================================================

    [[nodiscard]] bool        is_in_heap(const void *ptr);
    [[nodiscard]] std::size_t live_objects(); // ← ДОБАВИТЬ

    struct Stats {
        std::size_t total_allocated = 0;
        std::size_t total_freed = 0;
        std::size_t live_objects = 0;
        std::size_t total_passes = 0;
        std::size_t bytes_committed = 0;
    };
    Stats stats();

} // namespace carbon::gc