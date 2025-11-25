#pragma once

#include <cassert>
#include "types.hpp"

namespace vm
{
    // Внешняя переменная - базовый адрес пула модулей
    extern u8* g_module_pool_base;

    /**
     * @class Ptr
     * @brief Умный указатель, работающий со смещениями в пуле памяти модулей
     *
     * @tparam T Тип данных, на который указывает указатель
     *
     * Класс предоставляет безопасный интерфейс для работы с указателями в пуле памяти,
     * используя 32-битные смещения вместо 64-битных указателей для экономии памяти.
     * Все операции автоматически учитывают базовый адрес пула памяти.
     */
    template <typename T>
    struct Ptr
    {

        u32 offset;  ///< Смещение от базового адреса пула памяти

        /**
         * @brief Конструктор
         * @param off Смещение в байтах от g_module_pool_base (0 = nullptr)
         */
        explicit Ptr(u32 off = 0) : offset(off) {}

        /**
         * @brief Оператор доступа к членам класса
         * @return Указатель на объект типа T
         * @throws assert при попытке разыменования нулевого указателя
         */
        T* operator->() const {
            assert(offset && "Dereferencing null pointer");
            return reinterpret_cast<T*>(g_module_pool_base + offset);
        }

        /**
         * @brief Оператор разыменования
         * @return Ссылка на объект типа T
         * @throws assert при попытке разыменования нулевого указателя
         */
        T& operator*() const {
            assert(offset && "Dereferencing null pointer");
            return *reinterpret_cast<T*>(g_module_pool_base + offset);
        }

        // === АРИФМЕТИКА УКАЗАТЕЛЕЙ ===

        /**
         * @brief Сложение указателя с целым числом
         * @param diff Количество элементов для смещения
         * @return Новый указатель, смещенный на diff элементов
         */
        Ptr operator+(s32 diff) const { return Ptr(offset + diff * sizeof(T)); }

        /**
         * @brief Вычитание целого числа из указателя
         * @param diff Количество элементов для обратного смещения
         * @return Новый указатель, смещенный на -diff элементов
         */
        Ptr operator-(s32 diff) const { return Ptr(offset - diff * sizeof(T)); }

        /**
         * @brief Разность между двумя указателями
         * @param other Другой указатель того же типа
         * @return Количество элементов между указателями
         */
        s32 operator-(const Ptr<T>& other) const {
            return (offset - other.offset) / sizeof(T);
        }

        // === ОПЕРАТОРЫ СРАВНЕНИЯ ===

        bool operator==(const Ptr<T>& other) const { return offset == other.offset; }
        bool operator!=(const Ptr<T>& other) const { return offset != other.offset; }
        bool operator==(std::nullptr_t) const { return offset == 0; }
        bool operator!=(std::nullptr_t) const { return offset != 0; }

        bool operator<(const Ptr<T>& other) const { return offset < other.offset; }
        bool operator>(const Ptr<T>& other) const { return offset > other.offset; }
        bool operator<=(const Ptr<T>& other) const { return offset <= other.offset; }
        bool operator>=(const Ptr<T>& other) const { return offset >= other.offset; }

        // === КОНВЕРТАЦИЯ И ПРОВЕРКИ ===

        /**
         * @brief Получение сырого указателя C++
         * @return Указатель типа T* или nullptr если смещение равно 0
         */
        T* c() const {
            return offset ? reinterpret_cast<T*>(g_module_pool_base + offset) : nullptr;
        }

        /**
         * @brief Проверка на нулевой указатель
         * @return true если указатель нулевой, false иначе
         */
        bool is_null() const { return offset == 0; }

        /**
         * @brief Явное преобразование в bool
         * @return true если указатель ненулевой, false иначе
         */
        explicit operator bool() const { return offset != 0; }

        /**
         * @brief Безопасное преобразование между типами указателей
         * @tparam T2 Целевой тип указателя
         * @return Указатель Ptr<T2> с тем же смещением
         */
        template <typename T2>
        Ptr<T2> cast() const {
            return Ptr<T2>(offset);
        }
    };

    /**
     * @class Ptr<void>
     * @brief Специализация для void-указателей
     *
     * Отличается от базовой версии отсутствием операторов -> и *,
     * а также другой арифметикой указателей (без умножения на sizeof).
     */
    template <>
    struct Ptr<void>
    {
        u32 offset;  ///< Смещение от базового адреса пула памяти

        /**
         * @brief Конструктор
         * @param off Смещение в байтах от g_module_pool_base (0 = nullptr)
         */
        explicit Ptr(u32 off = 0) : offset(off) {}

        // НЕТ operator-> и operator* для void!

        // === АРИФМЕТИКА УКАЗАТЕЛЕЙ (в байтах) ===

        /**
         * @brief Сложение указателя с целым числом (в байтах)
         * @param diff Количество байт для смещения
         * @return Новый указатель, смещенный на diff байт
         */
        Ptr<void> operator+(s32 diff) const { return Ptr<void>(offset + diff); }

        /**
         * @brief Вычитание целого числа из указателя (в байтах)
         * @param diff Количество байт для обратного смещения
         * @return Новый указатель, смещенный на -diff байт
         */
        Ptr<void> operator-(s32 diff) const { return Ptr<void>(offset - diff); }

        /**
         * @brief Разность между двумя указателями (в байтах)
         * @param other Другой указатель того же типа
         * @return Количество байт между указателями
         */
        s32 operator-(const Ptr<void>& other) const {
            return offset - other.offset;
        }

        // === ОПЕРАТОРЫ СРАВНЕНИЯ ===

        bool operator==(const Ptr<void>& other) const { return offset == other.offset; }
        bool operator!=(const Ptr<void>& other) const { return offset != other.offset; }
        bool operator==(std::nullptr_t) const { return offset == 0; }
        bool operator!=(std::nullptr_t) const { return offset != 0; }

        bool operator<(const Ptr<void>& other) const { return offset < other.offset; }
        bool operator>(const Ptr<void>& other) const { return offset > other.offset; }
        bool operator<=(const Ptr<void>& other) const { return offset <= other.offset; }
        bool operator>=(const Ptr<void>& other) const { return offset >= other.offset; }

        // === КОНВЕРТАЦИЯ И ПРОВЕРКИ ===

        /**
         * @brief Получение сырого указателя C++
         * @return Указатель типа void* или nullptr если смещение равно 0
         */
        void* c() const {
            return offset ? reinterpret_cast<void*>(g_module_pool_base + offset) : nullptr;
        }

        /**
         * @brief Проверка на нулевой указатель
         * @return true если указатель нулевой, false иначе
         */
        bool is_null() const { return offset == 0; }

        /**
         * @brief Явное преобразование в bool
         * @return true если указатель ненулевой, false иначе
         */
        explicit operator bool() const { return offset != 0; }

        /**
         * @brief Безопасное преобразование между типами указателей
         * @tparam T2 Целевой тип указателя
         * @return Указатель Ptr<T2> с тем же смещением
         */
        template <typename T2>
        Ptr<T2> cast() const {
            return Ptr<T2>(offset);
        }
    };

    // === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===

    /**
     * @brief Создание Ptr из сырого указателя
     * @tparam T Тип указателя
     * @param ptr Сырой указатель C++
     * @return Ptr<T> с соответствующим смещением или нулевой Ptr если ptr == nullptr
     */
    template <typename T>
    Ptr<T> make_ptr(T* ptr) {
        if (!ptr || !g_module_pool_base) return Ptr<T>(0);

        u8* ptr_u8 = reinterpret_cast<u8*>(ptr);
        return Ptr<T>(static_cast<u32>(ptr_u8 - g_module_pool_base));
    }

    // Специализация для void закомментирована, так как базовая версия
    // template<typename T> уже корректно обрабатывает void* через механизмы C++
    /*
    template <>
    Ptr<void> make_ptr(void* ptr) {
        if (!ptr || !g_module_pool_base) return Ptr<void>(0);

        u8* ptr_u8 = reinterpret_cast<u8*>(ptr);
        return Ptr<void>(static_cast<u32>(ptr_u8 - g_module_pool_base));
    }
    */

} // namespace vm