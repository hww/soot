#pragma once

#include <cassert>
#include "types.hpp"

namespace vm
{
    extern u8* g_module_pool_base;

    // Базовая версия для обычных типов
    template <typename T>
    struct Ptr
    {
        u32 offset;

        explicit Ptr(u32 off = 0) : offset(off) {}

        T* operator->() const {
            assert(offset && "Dereferencing null pointer");
            return reinterpret_cast<T*>(g_module_pool_base + offset);
        }

        T& operator*() const {
            assert(offset && "Dereferencing null pointer");
            return *reinterpret_cast<T*>(g_module_pool_base + offset);
        }

        // pointer math
        Ptr operator+(s32 diff) const { return Ptr(offset + diff * sizeof(T)); }
        Ptr operator-(s32 diff) const { return Ptr(offset - diff * sizeof(T)); }
        s32 operator-(const Ptr<T>& other) const {
            return (offset - other.offset) / sizeof(T);
        }

        bool operator==(const Ptr<T>& other) const { return offset == other.offset; }
        bool operator!=(const Ptr<T>& other) const { return offset != other.offset; }
        bool operator==(std::nullptr_t) const { return offset == 0; }
        bool operator!=(std::nullptr_t) const { return offset != 0; }

        bool operator<(const Ptr<T>& other) const { return offset < other.offset; }
        bool operator>(const Ptr<T>& other) const { return offset > other.offset; }
        bool operator<=(const Ptr<T>& other) const { return offset <= other.offset; }
        bool operator>=(const Ptr<T>& other) const { return offset >= other.offset; }

        // Конвертация в сырой указатель
        T* c() const {
            return offset ? reinterpret_cast<T*>(g_module_pool_base + offset) : nullptr;
        }

        // Явная проверка на null
        bool is_null() const { return offset == 0; }
        explicit operator bool() const { return offset != 0; }

        // Безопасный кастинг между типами
        template <typename T2>
        Ptr<T2> cast() const {
            return Ptr<T2>(offset);
        }
    };

    // Специализация для void - БЕЗ операторов -> и *
    template <>
    struct Ptr<void>
    {
        u32 offset;

        explicit Ptr(u32 off = 0) : offset(off) {}

        // НЕТ operator-> и operator* для void!

        // pointer math - без sizeof(void)
        Ptr<void> operator+(s32 diff) const { return Ptr<void>(offset + diff); }
        Ptr<void> operator-(s32 diff) const { return Ptr<void>(offset - diff); }
        s32 operator-(const Ptr<void>& other) const {
            return offset - other.offset;
        }

        bool operator==(const Ptr<void>& other) const { return offset == other.offset; }
        bool operator!=(const Ptr<void>& other) const { return offset != other.offset; }
        bool operator==(std::nullptr_t) const { return offset == 0; }
        bool operator!=(std::nullptr_t) const { return offset != 0; }

        bool operator<(const Ptr<void>& other) const { return offset < other.offset; }
        bool operator>(const Ptr<void>& other) const { return offset > other.offset; }
        bool operator<=(const Ptr<void>& other) const { return offset <= other.offset; }
        bool operator>=(const Ptr<void>& other) const { return offset >= other.offset; }

        // Конвертация в сырой указатель
        void* c() const {
            return offset ? reinterpret_cast<void*>(g_module_pool_base + offset) : nullptr;
        }

        // Явная проверка на null
        bool is_null() const { return offset == 0; }
        explicit operator bool() const { return offset != 0; }

        // Безопасный кастинг между типами
        template <typename T2>
        Ptr<T2> cast() const {
            return Ptr<T2>(offset);
        }
    };

    // Функции создания Ptr
    template <typename T>
    Ptr<T> make_ptr(T* ptr) {
        if (!ptr || !g_module_pool_base) return Ptr<T>(0);

        u8* ptr_u8 = reinterpret_cast<u8*>(ptr);
        return Ptr<T>(static_cast<u32>(ptr_u8 - g_module_pool_base));
    }

    // Специализация для void
    //template <>
    //Ptr<void> make_ptr(void* ptr) {
    //    if (!ptr || !g_module_pool_base) return Ptr<void>(0);
    //
    //    u8* ptr_u8 = reinterpret_cast<u8*>(ptr);
    //    return Ptr<void>(static_cast<u32>(ptr_u8 - g_module_pool_base));
    //}
}