#pragma once

#include <cassert>
#include "common/carbon/lib/Types.hpp"

using namespace runtime;

namespace runtime::lib
{
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
        union {
            u64 offset;    ///< Смещение для файла (4 байта)
            T* ptr;      ///< Непосредственный указатель в памяти
        };

        /**
         * @brief Конструктор по умолчанию (нулевой указатель)
         */
        Ptr() : offset(0) {}

        /**
         * @brief Конструктор из смещения
         * @param off Смещение от базового адреса
         */
        explicit Ptr(u64 off) : offset(off) {}

        /**
         * @brief Конструктор из nullptr
         */
        Ptr(std::nullptr_t) : offset(0) {}

        /**
         * @brief Конструктор из указателя
         */
        Ptr(T* pointer) : ptr(pointer) {}

        /**
         * @brief Оператор доступа к членам класса
         * @return Указатель на объект типа T
         * @throws assert при попытке разыменования нулевого указателя
         */
        T* operator->() const {
            assert(ptr && "Dereferencing null pointer");
            return ptr;
        }

        /**
         * @brief Оператор разыменования
         * @return Ссылка на объект типа T
         * @throws assert при попытке разыменования нулевого указателя
         */
        T& operator*() const {
            assert(ptr && "Dereferencing null pointer");
            return *ptr;
        }

        // === АРИФМЕТИКА УКАЗАТЕЛЕЙ ===

        /**
         * @brief Сложение указателя с целым числом
         * @param diff Количество элементов для смещения
         * @return Новый указатель, смещенный на diff элементов
         */
        Ptr operator+(i64 diff) const {
            return Ptr(offset + diff * sizeof(T));
        }

        /**
         * @brief Вычитание целого числа из указателя
         * @param diff Количество элементов для обратного смещения
         * @return Новый указатель, смещенный на -diff элементов
         */
        Ptr operator-(i64 diff) const {
            return Ptr(offset - diff * sizeof(T));
        }

        /**
         * @brief Разность между двумя указателями
         * @param other Другой указатель того же типа
         * @return Количество элементов между указателями
         */
        i64 operator-(const Ptr<T>& other) const {
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
            return ptr;
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
     * @return Ptr<T> с соответствующим указателем
     */
    template <typename T>
    Ptr<T> make_ptr(T* ptr) {
        return Ptr<T>(ptr);  // Просто передаем указатель как есть
    }

    /**
     * @brief Создание Ptr из смещения
     * @tparam T Тип указателя
     * @param offset Смещение от базового адреса
     * @return Ptr<T> с соответствующим смещением
     */
    template <typename T>
    Ptr<T> make_ptr_from_offset(u64 offset) {
        return Ptr<T>(offset);
    }

} // namespace runtime::lib