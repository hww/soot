#pragma once

#include "CommonTypes.hpp"
#include "gc/ArenaManager.hpp"
#include "gc/GCHeader.hpp"
#include "gc/TypeRegistry.hpp"
#include "lib/StringId.hpp"

#include <cstddef>
#include <cstdint>

namespace carbon::gc {

    // Lifecycle
    void               initialize(std::size_t initial_mb = 16, std::size_t max_mb = 1024);
    void               shutdown();
    [[nodiscard]] bool is_initialized();

    // Allocation
    void              *alloc(sid64 sid, std::size_t size = 0, std::size_t align = 0);
    void               release(void *ptr);
    [[nodiscard]] bool is_in_heap(const void *ptr);

    // Pass API
    u32  start_pass();
    void mark_object(void *payload);
    bool sweep_pass(int budget = 64);
    void finish_pass();

    [[nodiscard]] u32         current_pass();
    [[nodiscard]] std::size_t live_objects();

    // Stop-the-world, sweep-only collection (no roots).
    void collect();

    // Queries
    [[nodiscard]] std::size_t capacity_bytes();

    struct Stats {
        std::size_t total_allocated = 0;
        std::size_t total_freed = 0;
        std::size_t live_objects = 0;
        std::size_t total_passes = 0;
        std::size_t bytes_committed = 0;
    };
    Stats stats();

    using GcVisitorFn = void (*)(sid64 type_sid, void *payload, std::size_t size, void *user);
    void                     for_each_live(GcVisitorFn visitor, void *user);

} // namespace carbon::gc