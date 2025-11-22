#pragma once

/**
 * @file ptr.h
 * Safe GOAL pointer representation with offset-based arithmetic
 */

#include <cassert>
#include <type_traits>
#include "types.hpp"

namespace vm {

    extern u8* g_ee_main_mem;

    /**
     * GOAL pointer to T. Represented as 32-bit offset from g_ee_main_mem.
     * NULL pointer has offset 0.
     */
    template <typename T>
    struct Ptr {
        u32 offset;

        // Constructors
        constexpr Ptr() noexcept : offset(0) {}
        explicit constexpr Ptr(u32 off) noexcept : offset(off) {}

        // Single conversion constructor - let compiler handle conversions
        template <typename U>
        Ptr(Ptr<U> other) noexcept : offset(other.offset) {
            // Let compiler errors handle invalid conversions naturally
        }

        // Const-correct dereference
        T* operator->() noexcept {
            assert(offset != 0 && "Dereferencing null pointer");
            return reinterpret_cast<T*>(g_ee_main_mem + offset);
        }

        const T* operator->() const noexcept {
            assert(offset != 0 && "Dereferencing null pointer");
            return reinterpret_cast<const T*>(g_ee_main_mem + offset);
        }

        T& operator*() noexcept {
            assert(offset != 0 && "Dereferencing null pointer");
            return *reinterpret_cast<T*>(g_ee_main_mem + offset);
        }

        const T& operator*() const noexcept {
            assert(offset != 0 && "Dereferencing null pointer");
            return *reinterpret_cast<const T*>(g_ee_main_mem + offset);
        }

        // Array subscript
        T& operator[](size_t index) noexcept {
            assert(offset != 0 && "Accessing null pointer");
            return *(reinterpret_cast<T*>(g_ee_main_mem + offset) + index);
        }

        const T& operator[](size_t index) const noexcept {
            assert(offset != 0 && "Accessing null pointer");
            return *(reinterpret_cast<const T*>(g_ee_main_mem + offset) + index);
        }

        // Comparison operators
        bool operator==(const Ptr& other) const noexcept { return offset == other.offset; }
        bool operator!=(const Ptr& other) const noexcept { return offset != other.offset; }
        bool operator<(const Ptr& other) const noexcept { return offset < other.offset; }
        bool operator>(const Ptr& other) const noexcept { return offset > other.offset; }
        bool operator<=(const Ptr& other) const noexcept { return offset <= other.offset; }
        bool operator>=(const Ptr& other) const noexcept { return offset >= other.offset; }

        // Pointer arithmetic with type safety
        Ptr operator+(std::ptrdiff_t diff) const noexcept {
            return Ptr(offset + diff * sizeof(T));
        }

        Ptr operator-(std::ptrdiff_t diff) const noexcept {
            return Ptr(offset - diff * sizeof(T));
        }

        std::ptrdiff_t operator-(Ptr other) const noexcept {
            assert((offset - other.offset) % sizeof(T) == 0 && "Pointer misalignment");
            return (offset - other.offset) / sizeof(T);
        }

        // Increment/Decrement
        Ptr& operator++() noexcept { offset += sizeof(T); return *this; }
        Ptr& operator--() noexcept { offset -= sizeof(T); return *this; }

        Ptr operator++(int) noexcept { Ptr temp = *this; ++*this; return temp; }
        Ptr operator--(int) noexcept { Ptr temp = *this; --*this; return temp; }

        // Explicit conversions
        explicit operator bool() const noexcept { return offset != 0; }

        // C-style pointer conversion
        T* c() noexcept {
            return offset ? reinterpret_cast<T*>(g_ee_main_mem + offset) : nullptr;
        }

        const T* c() const noexcept {
            return offset ? reinterpret_cast<const T*>(g_ee_main_mem + offset) : nullptr;
        }

        // Safe type casting
        template <typename U>
        Ptr<U> cast() const noexcept {
            return Ptr<U>(offset);
        }

        // Raw offset access
        u32 get_offset() const noexcept { return offset; }

        // Validity check
        bool is_null() const noexcept { return offset == 0; }
        bool valid() const noexcept { return offset != 0; }
    };

    // Specialization for void
    template <>
    struct Ptr<void> {
        u32 offset;

        constexpr Ptr() noexcept : offset(0) {}
        explicit constexpr Ptr(u32 off) noexcept : offset(off) {}

        // Single conversion constructor for void
        template <typename U>
        Ptr(Ptr<U> other) noexcept : offset(other.offset) {}

        // Only safe operations for void
        bool operator==(const Ptr& other) const noexcept { return offset == other.offset; }
        bool operator!=(const Ptr& other) const noexcept { return offset != other.offset; }
        bool operator<(const Ptr& other) const noexcept { return offset < other.offset; }
        bool operator>(const Ptr& other) const noexcept { return offset > other.offset; }
        bool operator<=(const Ptr& other) const noexcept { return offset <= other.offset; }
        bool operator>=(const Ptr& other) const noexcept { return offset >= other.offset; }

        explicit operator bool() const noexcept { return offset != 0; }

        void* c() noexcept {
            return offset ? reinterpret_cast<void*>(g_ee_main_mem + offset) : nullptr;
        }

        const void* c() const noexcept {
            return offset ? reinterpret_cast<const void*>(g_ee_main_mem + offset) : nullptr;
        }

        template <typename T>
        Ptr<T> cast() const noexcept {
            return Ptr<T>(offset);
        }

        u32 get_offset() const noexcept { return offset; }
        bool is_null() const noexcept { return offset == 0; }
        bool valid() const noexcept { return offset != 0; }

        // Arithmetic for void* - treat as byte pointers
        Ptr<void> operator+(u32 diff) const noexcept {
            return Ptr<void>(offset + diff);
        }

        Ptr<void> operator-(u32 diff) const noexcept {
            return Ptr<void>(offset - diff);
        }

        u32 operator-(Ptr<void> other) const noexcept {
            return offset - other.offset;
        }
    };

    // Specialization for const void
    template <>
    struct Ptr<const void> {
        u32 offset;

        constexpr Ptr() noexcept : offset(0) {}
        explicit constexpr Ptr(u32 off) noexcept : offset(off) {}

        // Conversion from any pointer type
        template <typename U>
        Ptr(Ptr<U> other) noexcept : offset(other.offset) {}

        bool operator==(const Ptr& other) const noexcept { return offset == other.offset; }
        bool operator!=(const Ptr& other) const noexcept { return offset != other.offset; }
        bool operator<(const Ptr& other) const noexcept { return offset < other.offset; }
        bool operator>(const Ptr& other) const noexcept { return offset > other.offset; }
        bool operator<=(const Ptr& other) const noexcept { return offset <= other.offset; }
        bool operator>=(const Ptr& other) const noexcept { return offset >= other.offset; }

        explicit operator bool() const noexcept { return offset != 0; }

        const void* c() const noexcept {
            return offset ? reinterpret_cast<const void*>(g_ee_main_mem + offset) : nullptr;
        }

        template <typename T>
        Ptr<T> cast() const noexcept {
            return Ptr<T>(offset);
        }

        u32 get_offset() const noexcept { return offset; }
        bool is_null() const noexcept { return offset == 0; }
        bool valid() const noexcept { return offset != 0; }

        Ptr<const void> operator+(u32 diff) const noexcept {
            return Ptr<const void>(offset + diff);
        }

        Ptr<const void> operator-(u32 diff) const noexcept {
            return Ptr<const void>(offset - diff);
        }

        u32 operator-(Ptr<const void> other) const noexcept {
            return offset - other.offset;
        }
    };

    // Factory functions
    template <typename T>
    Ptr<T> make_ptr(T* ptr) noexcept {
        if (!ptr) return Ptr<T>();
        return Ptr<T>(static_cast<u32>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(g_ee_main_mem)));
    }

    template <typename T>
    Ptr<const T> make_ptr(const T* ptr) noexcept {
        if (!ptr) return Ptr<const T>();
        return Ptr<const T>(static_cast<u32>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(g_ee_main_mem)));
    }

    template <typename T>
    Ptr<u8> make_uint8_ptr(T* ptr) noexcept {
        if (!ptr) return Ptr<u8>();
        return Ptr<u8>(static_cast<u32>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(g_ee_main_mem)));
    }

    template <typename T>
    Ptr<const u8> make_uint8_ptr(const T* ptr) noexcept {
        if (!ptr) return Ptr<const u8>();
        return Ptr<const u8>(static_cast<u32>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(g_ee_main_mem)));
    }

} // namespace vm