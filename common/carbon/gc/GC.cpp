#include "gc/GC.hpp"
#include "util/Log.hpp"

#include <cassert>
#include <cstring>

namespace carbon::gc {

    namespace {

        enum class Phase { IDLE, MARK, SWEEP };

        struct GCState {
            ArenaManager *arena_mgr = nullptr;
            u32           current_pass = 0; // 0 = not started, else 1..15
            Phase         phase = Phase::IDLE;

            std::size_t sweep_arena_idx = 0;
            std::size_t sweep_block_idx = 0;

            std::size_t total_allocated = 0;
            std::size_t total_freed = 0;
            std::size_t total_passes = 0;
        };

        GCState &state() {
            static GCState s;
            return s;
        }

        // Trampoline the mark delegates call into.
        void mark_visitor(sid64 /*type_sid*/, void *payload, std::size_t /*size*/,
                          void * /*user*/) {
            mark_object(payload);
        }

    } // namespace

    // ========================================================================
    // Lifecycle
    // ========================================================================

    void initialize(std::size_t initial_mb, std::size_t max_mb) {
        GCState &s = state();
        if (s.arena_mgr) {
            lg::warn("gc::initialize called twice - ignoring");
            return;
        }
        s.arena_mgr = new ArenaManager(initial_mb, max_mb);
        s.current_pass = 0;
        s.phase = Phase::IDLE;
        s.total_allocated = 0;
        s.total_freed = 0;
        s.total_passes = 0;
        lg::info("gc initialized ({} MB initial, {} MB max)", initial_mb, max_mb);
    }

    void shutdown() {
        GCState &s = state();
        if (!s.arena_mgr) return;
        delete s.arena_mgr;
        s.arena_mgr = nullptr;
        s.current_pass = 0;
        s.phase = Phase::IDLE;
        lg::info("gc shutdown");
    }

    bool is_initialized() { return state().arena_mgr != nullptr; }

    // ========================================================================
    // Allocation
    // ========================================================================

    void *alloc(sid64 sid, std::size_t size, std::size_t align) {
        GCState &s = state();
        if (!s.arena_mgr) {
            lg::error("gc::alloc before initialize");
            return nullptr;
        }

        const u32       type_index = type_registry().find_index(sid);
        TypeDescriptor *td = type_registry().find_by_index(type_index);

        if (size == 0 && td) size = td->size;
        if (align == 0 && td) align = td->align;
        if (size == 0) {
            lg::error("gc::alloc: unknown type and zero size (sid=0x{:016X})", sid);
            return nullptr;
        }

        const std::size_t total = sizeof(GCHeader) + size;
        const std::size_t aligned_total = (total + 15) & ~std::size_t{15};

        void *block = s.arena_mgr->allocate(aligned_total, 16);
        if (!block) {
            lg::error("gc::alloc: out of memory (sid=0x{:016X}, size={})", sid, size);
            return nullptr;
        }

        GCHeader *h = static_cast<GCHeader *>(block);
        h->type_index = type_index;
        h->flags = 0;

        // Do not pre-assign a cycle. 0 means the object has not yet participated
        // in any pass; start_pass() will generate the first non-zero cycle, and
        // anything not marked by then is correctly collected.
        gc_set_pass_cycle(h->flags, s.current_pass);
        gc_set_size_class(h->flags, static_cast<u32>(size_class_for(aligned_total)));

#if CARBON_GC_DEBUG_NAME
        h->name = sid;
#endif

        void *payload = payload_of(h);
        std::memset(payload, 0, size);

        ++s.total_allocated;
        return payload;
    }

    void release(void *ptr) {
        GCState &s = state();
        if (!s.arena_mgr || !ptr) return;
        if (!s.arena_mgr->owns(ptr)) {
            lg::warn("gc::release: pointer {} not in heap", ptr);
            return;
        }
        GCHeader *h = header_of(ptr);
        if (h->flags & GC_FLAG_DEAD) return;
        h->flags |= GC_FLAG_DEAD;
        s.arena_mgr->deallocate(h);
        ++s.total_freed;
    }

    bool is_in_heap(const void *ptr) {
        GCState &s = state();
        return s.arena_mgr && s.arena_mgr->owns(ptr);
    }

    // ========================================================================
    // Pass API
    // ========================================================================

    u32 start_pass() {
        GCState &s = state();
        if (!s.arena_mgr) return 0;
        s.current_pass = gc_next_cycle(s.current_pass);
        s.phase = Phase::MARK;
        s.sweep_arena_idx = 0;
        s.sweep_block_idx = 0;
        ++s.total_passes;
        return s.current_pass;
    }

    u32 current_pass() { return state().current_pass; }

    void mark_object(void *payload) {
        GCState &s = state();
        if (!s.arena_mgr || !payload) return;
        if (!s.arena_mgr->owns(payload)) return;

        GCHeader *h = header_of(payload);
        if (h->flags & GC_FLAG_DEAD) return;
        if (gc_get_pass_cycle(h->flags) == s.current_pass) return;

        gc_set_pass_cycle(h->flags, s.current_pass);
        h->flags |= GC_FLAG_MARKED;

        TypeDescriptor *td = type_registry().find_by_index(h->type_index);
        if (td && td->mark) { td->mark(payload, &mark_visitor, nullptr); }
    }

    // ========================================================================
    // Sweep
    // ========================================================================

    bool sweep_pass(int budget) {
        GCState &s = state();
        if (!s.arena_mgr) return true;
        if (budget <= 0) return false;

        const auto &all = s.arena_mgr->all_arenas();

        while (s.sweep_arena_idx < all.size()) {
            Arena            *a = all[s.sweep_arena_idx];
            std::byte        *begin = a->begin();
            const std::size_t bs = a->block_size();
            const std::size_t total_blocks = a->total_blocks();

            while (s.sweep_block_idx < total_blocks) {
                if (!a->is_allocated(s.sweep_block_idx)) {
                    ++s.sweep_block_idx;
                    continue;
                }

                std::byte *block = begin + s.sweep_block_idx * bs;
                GCHeader  *h = reinterpret_cast<GCHeader *>(block);

                if (h->flags & GC_FLAG_DEAD) {
                    // Already released.
                } else if (gc_get_pass_cycle(h->flags) == s.current_pass) {
                    // Live in this pass. Clear MARKED so the next pass starts clean.
                    h->flags &= ~GC_FLAG_MARKED;
                } else {
                    // Garbage. Call the destroy delegate before reclaiming.
                    TypeDescriptor *td = type_registry().find_by_index(h->type_index);
                    if (td && td->destroy) { td->destroy(payload_of(h)); }
                    s.arena_mgr->deallocate(h);
                    ++s.total_freed;
                }

                ++s.sweep_block_idx;
                if (--budget <= 0) return false;
            }

            ++s.sweep_arena_idx;
            s.sweep_block_idx = 0;
        }

        s.phase = Phase::IDLE;
        return true;
    }

    void finish_pass() {
        GCState &s = state();
        s.phase = Phase::IDLE;
        s.sweep_arena_idx = 0;
        s.sweep_block_idx = 0;
    }

    void collect() {
        start_pass();
        while (!sweep_pass(1 << 20)) {}
        finish_pass();
    }

    // ========================================================================
    // Queries
    // ========================================================================

    std::size_t live_objects() {
        GCState &s = state();
        return s.arena_mgr ? s.arena_mgr->live_count() : 0;
    }

    Stats stats() {
        GCState &s = state();
        Stats    out;
        out.total_allocated = s.total_allocated;
        out.total_freed = s.total_freed;
        out.live_objects = s.arena_mgr ? s.arena_mgr->live_count() : 0;
        out.total_passes = s.total_passes;
        out.bytes_committed = s.arena_mgr ? s.arena_mgr->committed() : 0;
        return out;
    }

    std::size_t capacity_bytes() {
        GCState &s = state();
        return s.arena_mgr ? s.arena_mgr->region_size() : 0;
    }

    void for_each_live(GcVisitorFn visitor, void *user) {
        GCState &s = state();
        if (!s.arena_mgr || !visitor) return;
        for (Arena *a : s.arena_mgr->all_arenas()) {
            const std::size_t total = a->total_blocks();
            const std::size_t bs = a->block_size();
            std::byte        *base = a->begin();
            for (std::size_t i = 0; i < total; ++i) {
                if (!a->is_allocated(i)) continue;
                GCHeader *h = reinterpret_cast<GCHeader *>(base + i * bs);
                if (h->flags & GC_FLAG_DEAD) continue;
                TypeDescriptor *td = type_registry().find_by_index(h->type_index);
                sid64           sid = td ? static_cast<sid64>(td->name) : 0;
                std::size_t     sz = td ? td->size : 0;
                visitor(sid, payload_of(h), sz, user);
            }
        }
    }

} // namespace carbon::gc