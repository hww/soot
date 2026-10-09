#pragma once

#include "CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/Variant.hpp"

#include "generated/vector.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace carbon {

    /// @brief A per-frame bump allocator for one POD type T.
    /// @details The pool owns a contiguous block of memory sized for
    ///          `capacity` elements of T. `allocate()` returns the next
    ///          free slot (or nullptr when exhausted). `clear()` resets
    ///          the bump pointer; the memory itself stays alive.
    ///
    ///          The pool does NOT run constructors or destructors. T must
    ///          be trivially copyable (POD). This matches the VM's model:
    ///          script objects are plain memory with no C++ lifetime.
    template <typename T> class Pool {
    public:
        explicit Pool(std::size_t capacity = 1024)
            : m_capacity(capacity), m_used(0),
              m_bytes(static_cast<std::byte *>(
                  ::operator new[](capacity * sizeof(T), std::align_val_t(alignof(T))))) {}

        ~Pool() { ::operator delete[](m_bytes, std::align_val_t(alignof(T))); }

        Pool(const Pool &) = delete;
        Pool &operator=(const Pool &) = delete;

        /// @brief Return a pointer to the next free slot, or nullptr if full.
        [[nodiscard]] T *allocate() noexcept {
            if (m_used >= m_capacity) { return nullptr; }
            T *slot = reinterpret_cast<T *>(m_bytes + m_used * sizeof(T));
            ++m_used;
            return slot;
        }

        /// @brief Reset the bump pointer. Memory is kept; no destructors run.
        void clear() noexcept { m_used = 0; }

        [[nodiscard]] std::size_t used() const noexcept { return m_used; }
        [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }

    private:
        std::size_t m_capacity = 0;
        std::size_t m_used = 0;
        std::byte  *m_bytes = nullptr;
    };

    /// @brief Script-side allocators exposed to the VM as native functions.
    /// @details Each allocator is a `Pool<T>` wrapped in a native function
    ///          that the VM can call as `allocate-<name>`. All pools are
    ///          cleared together by `VirtualMachine::purge()`.
    class NativeAllocators {
    public:
        NativeAllocators() = default;
        ~NativeAllocators() = default;

        NativeAllocators(const NativeAllocators &) = delete;
        NativeAllocators &operator=(const NativeAllocators &) = delete;

        /// @brief Register every allocator as a native function in the VM.
        void initialize_builtins();

        /// @brief Clear every pool. Called from `VirtualMachine::purge()`.
        void clear_all() noexcept;

        // --- Direct C++ access (used by VM when creating objects) ---
        //
        // NOTE: names differ from the free functions in the .cpp on purpose.
        // If both were called `allocate_i64`, taking the address of the
        // free function would be ambiguous at the call site in
        // initialize_builtins().

        [[nodiscard]] i64   *allocate_i64_slot() noexcept { return m_i64_pool.allocate(); }
        [[nodiscard]] f32   *allocate_f32_slot() noexcept { return m_f32_pool.allocate(); }
        [[nodiscard]] sid64 *allocate_sid_slot() noexcept { return m_sid_pool.allocate(); }

        [[nodiscard]] vector *allocate_vector_slot() noexcept { return m_vector_pool.allocate(); }
        [[nodiscard]] cvector *allocate_cvector_slot() noexcept { return m_cvector_pool.allocate(); }

    private:
        Pool<i64>   m_i64_pool{4096};
        Pool<f32>   m_f32_pool{4096};
        Pool<sid64> m_sid_pool{4096};
        Pool<vector> m_vector_pool{4096};
        Pool<cvector> m_cvector_pool{4096};
    };

} // namespace carbon