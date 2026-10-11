#include "gc/GC.hpp"
#include "util/Log.hpp"

#include <cassert>
#include <cstring>

namespace carbon::gc {

    // ========================================================================
    // Internal state
    // ========================================================================
    //
    // Единственный экземпляр на процесс. Создаётся в initialize().
    // Уничтожается в shutdown().
    //
    namespace {

        enum class Phase {
            IDLE,
            MARK,
            SWEEP,
        };

        struct GCState {
            ArenaManager *arena_mgr = nullptr;
            u64           current_pass = 0; // 0 = IDLE
            Phase         phase = Phase::IDLE;

            // Курсор sweep: индекс арены в all_arenas + смещение внутри неё.
            std::size_t sweep_arena_idx = 0;
            std::size_t sweep_block_idx = 0;

            // Статистика.
            std::size_t total_allocated = 0;
            std::size_t total_freed = 0;
            std::size_t total_passes = 0;
        };

        GCState &state() {
            static GCState s;
            return s;
        }

    } // namespace

    // ========================================================================
    // Lifecycle
    // ========================================================================

    void initialize(std::size_t initial_mb, std::size_t max_mb) {
        GCState &s = state();
        if (s.arena_mgr) {
            lg::warn("gc::initialize called twice — ignoring");
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

    // ========================================================================
    // Allocation
    // ========================================================================

    void *alloc(sid64 sid, std::size_t size, std::size_t align) {
        GCState &s = state();
        if (!s.arena_mgr) {
            lg::error("gc::alloc before initialize");
            return nullptr;
        }

        // Размер и выравнивание из реестра типов, если не заданы.
        TypeDescriptor *td = type_registry().find(sid);
        if (size == 0) { size = td ? td->size : 0; }
        if (align == 0) { align = td ? td->align : 16; }
        if (size == 0) {
            lg::error("gc::alloc: unknown type and zero size (sid=0x{:016X})", sid);
            return nullptr;
        }

        // Считаем полный размер блока: header + payload, выровненный
        // до 16.
        const std::size_t total = sizeof(GCHeader) + size;
        const std::size_t aligned_total = (total + 15) & ~std::size_t{15};

        // Запрашиваем блок у ArenaManager.
        void *block = s.arena_mgr->allocate(aligned_total, 16);
        if (!block) {
            lg::error("gc::alloc: out of memory (sid=0x{:016X}, size={})", sid, size);
            return nullptr;
        }

        // Инициализируем заголовок.
        GCHeader *h = static_cast<GCHeader *>(block);
        h->pass_cycle = (s.current_pass != 0) ? s.current_pass : 0;
        h->size_class = size_class_for(aligned_total);
        h->flags = 0;
        h->type_desc = td;
        h->reserved = 0;

        // Зануляем payload — чтобы мусор не притворялся указателями.
        void *payload = payload_of(h);
        std::memset(payload, 0, size);

        ++s.total_allocated;

        return payload;
    }

    void free(void *ptr) {
        GCState &s = state();
        if (!s.arena_mgr) return;
        if (!ptr) return;
        if (!s.arena_mgr->owns(ptr)) {
            lg::warn("gc::free: pointer {} not in heap", ptr);
            return;
        }

        GCHeader *h = header_of(ptr);
        if (h->flags & GC_FLAG_DEAD) {
            return; // уже освобождён
        }
        h->flags |= GC_FLAG_DEAD;

        s.arena_mgr->deallocate(h);
        ++s.total_freed;
    }

    bool is_in_heap(const void *ptr) {
        GCState &s = state();
        if (!s.arena_mgr) return false;
        return s.arena_mgr->owns(ptr);
    }

    // ========================================================================
    // Pass API
    // ========================================================================

    u64 start_pass() {
        GCState &s = state();
        if (!s.arena_mgr) return 0;

        ++s.current_pass;
        if (s.current_pass == 0) {
            // overflow — теоретически невозможно, но подстрахуемся
            s.current_pass = 1;
        }
        s.phase = Phase::MARK;
        s.sweep_arena_idx = 0;
        s.sweep_block_idx = 0;
        ++s.total_passes;

        return s.current_pass;
    }

    u64 current_pass() { return state().current_pass; }

    // ----------------------------------------------------------------
    // mark_object — рекурсивный обход одного heap-объекта.
    // ----------------------------------------------------------------
    //
    // Алгоритм:
    //   1. Проверить, что ptr в heap.
    //   2. Получить header, проверить pass_cycle.
    //   3. Если уже помечен в этом проходе — выход.
    //   4. Пометить: pass_cycle = current_pass.
    //   5. Если type_desc задан — пройти по полям, рекурсивно пометить
    //      все указатели.
    //
    static void scan_object(GCHeader *h, void *payload);

    void mark_object(void *payload) {
        GCState &s = state();
        if (!s.arena_mgr) return;
        if (!payload) return;
        if (!s.arena_mgr->owns(payload)) return;

        GCHeader *h = header_of(payload);

        // Если явно освобождён — не помечаем (мёртв).
        if (h->flags & GC_FLAG_DEAD) return;

        // Уже помечен в этом проходе?
        if (h->pass_cycle == s.current_pass) return;

        // Помечаем.
        h->pass_cycle = s.current_pass;

        // Сканируем поля, если тип известен.
        scan_object(h, payload);
    }

    // ----------------------------------------------------------------
    // scan_object — обход полей объекта.
    // ----------------------------------------------------------------
    //
    // Для каждого поля:
    //   * если скаляр — пропустить;
    //   * если указатель (reference без INLINE) — mark_object;
    //   * если array + inline + reference — mark каждого элемента;
    //   * если array + dynamic — пока не поддерживается (TODO).
    //
    static void scan_object(GCHeader *h, void *payload) {
        TypeDescriptor *td = h->type_desc;
        if (!td || !td->fields || td->num_fields == 0) return;

        for (u32 i = 0; i < td->num_fields; ++i) {
            const FieldDesc &f = td->fields[i];

            TypeDescriptor *ftd = type_registry().find(f.type);
            const bool      field_is_scalar =
                (ftd && (ftd->flags & TYPE_IS_SCALAR)) || (!ftd && (f.flags == 0));

            // Массив?
            if (f.flags & FIELD_IS_ARRAY) {
                if (f.flags & FIELD_IS_INLINE) {
                    // Inline array. Если элементы — reference, mark каждого.
                    if (field_is_scalar) continue;

                    void **elems =
                        reinterpret_cast<void **>(static_cast<std::byte *>(payload) + f.offset);
                    for (u32 j = 0; j < f.count; ++j) {
                        void *e = elems[j];
                        if (e && is_in_heap(e)) mark_object(e);
                    }
                } else if (f.flags & FIELD_IS_DYNAMIC) {
                    // TODO: darray. Пока пропускаем.
                    // Layout: { u32 size; u32 capacity; u8* data; }
                    // Ожидаем f.offset указывает на начало darray.
                    // Пока — не сканируем.
                    continue;
                } else {
                    // Массив без INLINE и DYNAMIC — не поддерживаем.
                    continue;
                }
            } else {
                // Не массив.
                if (f.flags & FIELD_IS_INLINE) {
                    // Inline structure — данные внутри объекта.
                    // Поля внутри inline-структуры не перечислены в
                    // нашем дескрипторе (мы не разворачиваем вложенные
                    // структуры). Поэтому пока считаем, что inline
                    // структура не содержит указателей — либо это
                    // ответственность составителя дескриптора.
                    continue;
                }

                // Простой указатель (или скаляр).
                if (field_is_scalar) continue;

                void *field_ptr =
                    *reinterpret_cast<void **>(static_cast<std::byte *>(payload) + f.offset);
                if (field_ptr && is_in_heap(field_ptr)) { mark_object(field_ptr); }
            }
        }
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
            std::byte        *end = a->end();
            const std::size_t bs = a->block_size();
            const std::size_t total_blocks = a->total_blocks();

            while (s.sweep_block_idx < total_blocks) {
                if (!a->is_allocated(s.sweep_block_idx)) {
                    ++s.sweep_block_idx;
                    continue; // свободный — пропускаем
                }

                std::byte *block = begin + s.sweep_block_idx * bs;
                GCHeader  *h = reinterpret_cast<GCHeader *>(block);

                if (h->flags & GC_FLAG_DEAD) {
                    // уже освобождён через free()
                } else if (h->pass_cycle == s.current_pass) {
                    // помечен в этом проходе — живой
                    h->flags &= ~GC_FLAG_MARKED;
                } else {
                    // не помечен — мёртв
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

    // ========================================================================
    // Full collection (STW)
    // ========================================================================
    //
    // Полный проход без корней. Полезно только для тестов: собирает всё,
    // что не помечено вручную до вызова.
    //
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
        if (!s.arena_mgr) return 0;
        return s.arena_mgr->live_count();
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

} // namespace carbon::gc