#pragma once

#include "CommonTypes.hpp"
#include "gc/GC.hpp"
#include "lib/StringId.hpp"

#include <cstring>

namespace carbon::gc {

    // ========================================================================
    // DArray<T>
    // ========================================================================
    //
    // NOTE: this template calls gc::alloc(/*sid=*/0, ...) for its element
    // buffer. That means the buffer is registered with the GC as an
    // untyped block. The GC will not scan it for pointers.
    //
    // Consequences:
    //
    //   DArray<i32>  -- fine, no pointers inside.
    //   DArray<f32>  -- fine, no pointers inside.
    //   DArray<T*>   -- WRONG: the pointers inside are not marked and will be
    //                   reclaimed on the next sweep. Use RuntimeArray from
    //                   vm/NativeArray.hpp instead, which has a per-kind
    //                   mark delegate.
    //
    // If you need a typed array of pointers, prefer NativeArray's
    // runtime-array-ptr type.
    //
    template <typename T> class DArray {
    public:
        DArray() = default;
        ~DArray() = default;

        DArray(const DArray &) = delete;
        DArray &operator=(const DArray &) = delete;

        DArray(DArray &&other) noexcept
            : m_size(other.m_size), m_capacity(other.m_capacity), m_data(other.m_data) {
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_data = nullptr;
        }

        DArray &operator=(DArray &&other) noexcept {
            if (this != &other) {
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                m_data = other.m_data;
                other.m_size = 0;
                other.m_capacity = 0;
                other.m_data = nullptr;
            }
            return *this;
        }

        // ---- Размеры ----

        [[nodiscard]] u32  size() const noexcept { return m_size; }
        [[nodiscard]] u32  capacity() const noexcept { return m_capacity; }
        [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

        // ---- Доступ ----

        [[nodiscard]] T       &operator[](u32 i) noexcept { return m_data[i]; }
        [[nodiscard]] const T &operator[](u32 i) const noexcept { return m_data[i]; }

        [[nodiscard]] T       *data() noexcept { return m_data; }
        [[nodiscard]] const T *data() const noexcept { return m_data; }

        [[nodiscard]] T       *begin() noexcept { return m_data; }
        [[nodiscard]] T       *end() noexcept { return m_data + m_size; }
        [[nodiscard]] const T *begin() const noexcept { return m_data; }
        [[nodiscard]] const T *end() const noexcept { return m_data + m_size; }

        // ---- Изменения ----

        void push_back(const T &value) {
            if (m_size >= m_capacity) grow(m_capacity == 0 ? 4 : m_capacity * 2);
            m_data[m_size++] = value;
        }

        void push_back(T &&value) {
            if (m_size >= m_capacity) grow(m_capacity == 0 ? 4 : m_capacity * 2);
            m_data[m_size++] = std::move(value);
        }

        void resize(u32 new_size) {
            if (new_size > m_capacity) grow(new_size);
            // Инициализация новых — T() по умолчанию.
            for (u32 i = m_size; i < new_size; ++i) { m_data[i] = T{}; }
            m_size = new_size;
        }

        void clear() noexcept { m_size = 0; }

        // ---- Резервирование ----

        void reserve(u32 new_capacity) {
            if (new_capacity > m_capacity) grow(new_capacity);
        }

    private:
        void grow(u32 new_capacity) {
            if (new_capacity <= m_capacity) return;

            // Выделяем в GC-heap через gc::alloc.
            // sid передаём как 0 (неизвестный тип) — можно задать явно.
            const u64 bytes = static_cast<u64>(new_capacity) * sizeof(T);
            void     *raw = gc::alloc(/*sid=*/0, bytes, alignof(T));
            if (!raw) return; // OOM — оставляем как было

            T *new_data = static_cast<T *>(raw);

            // Копируем существующие элементы.
            for (u32 i = 0; i < m_size; ++i) { new_data[i] = std::move(m_data[i]); }

            m_data = new_data;
            m_capacity = new_capacity;
        }

        u32 m_size = 0;
        u32 m_capacity = 0;
        T  *m_data = nullptr;
    };

} // namespace carbon::gc