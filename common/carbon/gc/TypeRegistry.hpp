#pragma once

#include "CommonTypes.hpp"
#include "gc/GCHeader.hpp"
#include "lib/StringId.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace carbon::gc {

    // ========================================================================
    // Field flags
    // ========================================================================
    //
    // Описывают, как интерпретировать поле. Комбинация флагов:
    //
    //   простой скаляр          : type=int32,  flags=0
    //   указатель на структуру  : type=vector, flags=0
    //   inline структура        : type=vector, flags=IS_INLINE
    //   inline массив скаляров  : type=int32,  flags=IS_ARRAY|IS_INLINE, count=N
    //   inline массив структур  : type=vector, flags=IS_ARRAY|IS_INLINE, count=N
    //   динамический массив     : type=vector, flags=IS_ARRAY|IS_DYNAMIC
    //
    enum GCFieldFlags : u32 {
        FIELD_IS_ARRAY = 1u << 0,
        FIELD_IS_INLINE = 1u << 1,
        FIELD_IS_DYNAMIC = 1u << 2,
    };

    // ========================================================================
    // Type flags
    // ========================================================================
    enum GCTypeFlags : u32 {
        TYPE_IS_SCALAR = 1u << 0,    // int32, float, ... — не указатель
        TYPE_IS_REFERENCE = 1u << 1, // structure, basic, ... — указатель
    };

    // ========================================================================
    // FieldDesc — описание одного поля структуры
    // ========================================================================
    struct FieldDesc {
        sid64 type;   // sid типа поля
        u32   offset; // runtime-смещение в объекте
        u32   size;   // размер поля в байтах
        u32   flags;  // FieldFlags
        u32   count;  // число элементов (только для IS_ARRAY|IS_INLINE)
    };

    // ========================================================================
    // TypeDescriptor — описание типа
    // ========================================================================
    //
    // Живёт вечно. Хранится в реестре (владелец — реестр).
    // Поля (fields) — массив, владелец тот же реестр.
    //
    struct TypeDescriptor {
        sid64 name;  // sid типа
        u32   size;  // размер объекта (payload)
        u32   align; // выравнивание объекта
        u32   num_fields;
        u32   flags;       // TypeFlags
        FieldDesc *fields; // массив полей (владелец — реестр) или nullptr
    };

    // ========================================================================
    // TypeRegistry
    // ========================================================================
    //
    // Глобальный реестр. Регистрирует TypeDescriptor по sid.
    // Идемпотентен: повторная регистрация того же sid — no-op.
    //
    // Живёт вечно. Не освобождает зарегистрированные типы.
    // При shutdown() — очищает всё разом.
    //
    class TypeRegistry {
    public:
        TypeRegistry() = default;
        ~TypeRegistry();

        TypeRegistry(const TypeRegistry &) = delete;
        TypeRegistry &operator=(const TypeRegistry &) = delete;

        /// @brief Зарегистрировать тип. Если sid уже зарегистрирован — no-op.
        /// @param td  описание типа (копируется)
        /// @return указатель на хранимый дескриптор
        TypeDescriptor *register_type(const TypeDescriptor &td);

        /// @brief Зарегистрировать тип с явным массивом полей.
        /// @param td          описание типа (без fields)
        /// @param fields      массив полей (копируется)
        /// @param num_fields  число полей
        TypeDescriptor *register_type_with_fields(const TypeDescriptor &td, const FieldDesc *fields,
                                                  u32 num_fields);

        /// @brief Найти тип по sid. nullptr, если не зарегистрирован.
        [[nodiscard]] TypeDescriptor *find(sid64 sid) const noexcept;

        /// @brief Очистить реестр (освобождает всё).
        void clear();

        /// @brief Количество зарегистрированных типов.
        [[nodiscard]] std::size_t size() const noexcept { return m_types.size(); }

    private:
        // Владелец дескрипторов.
        std::unordered_map<sid64, TypeDescriptor *> m_types;

        // Владелец массивов полей. Освобождаем при clear().
        std::vector<FieldDesc *> m_field_arrays;
    };

    /// @brief Глобальный реестр.
    TypeRegistry &type_registry();

} // namespace carbon::gc