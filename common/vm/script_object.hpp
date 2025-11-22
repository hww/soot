/**
 * @file ScriptObject.hpp
 * @brief Reference counting with inline data and magic validation
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <new>

 // =============================================================================
 // Script Object Base Structure
 // =============================================================================

struct ScriptObjectBase {
    int32_t ref_count;
    uint32_t magic;

    ScriptObjectBase() : ref_count(1), magic(0xDDEF) {}

    bool is_valid() const { return magic == 0xDDEF; }
};

// =============================================================================
// Template Script Object
// =============================================================================

template<typename T>
struct ScriptObject : ScriptObjectBase {
    T data;

    template<typename... Args>
    ScriptObject(Args&&... args)
        : ScriptObjectBase(), data(std::forward<Args>(args)...) {
    }
};

// =============================================================================
// Universal Management Functions (const-correct)
// =============================================================================

/**
 * @brief Get ScriptObjectBase from data pointer (const version)
 */
inline const ScriptObjectBase* to_script_base(const void* data_ptr) {
    if (!data_ptr) return nullptr;

    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(data_ptr);
    const ScriptObjectBase* base = reinterpret_cast<const ScriptObjectBase*>(
        byte_ptr - sizeof(ScriptObjectBase)
        );

    return base->is_valid() ? base : nullptr;
}

/**
 * @brief Get ScriptObjectBase from data pointer (non-const version)
 */
inline ScriptObjectBase* to_script_base(void* data_ptr) {
    if (!data_ptr) return nullptr;

    uint8_t* byte_ptr = reinterpret_cast<uint8_t*>(data_ptr);
    ScriptObjectBase* base = reinterpret_cast<ScriptObjectBase*>(
        byte_ptr - sizeof(ScriptObjectBase)
        );

    return base->is_valid() ? base : nullptr;
}

/**
 * @brief Get typed ScriptObject wrapper from data pointer
 */
template<typename T>
ScriptObject<T>* to_script_object(const T* data_ptr) {
    if (!data_ptr) return nullptr;

    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(data_ptr);
    ScriptObject<T>* wrapper = reinterpret_cast<ScriptObject<T>*>(
        byte_ptr - sizeof(ScriptObjectBase)
        );

    return wrapper->is_valid() ? wrapper : nullptr;
}

template<typename T>
ScriptObject<T>* to_script_object(T* data_ptr) {
    if (!data_ptr) return nullptr;

    uint8_t* byte_ptr = reinterpret_cast<uint8_t*>(data_ptr);
    ScriptObject<T>* wrapper = reinterpret_cast<ScriptObject<T>*>(
        byte_ptr - sizeof(ScriptObjectBase)
        );

    return wrapper->is_valid() ? wrapper : nullptr;
}

/**
 * @brief Increase reference count (const version)
 */
inline void script_ref(const void* data_ptr) {
    if (const ScriptObjectBase* base = to_script_base(data_ptr)) {
        const_cast<ScriptObjectBase*>(base)->ref_count++;  // Safe - мы владеем объектом
    }
}

/**
 * @brief Increase reference count (non-const version)
 */
inline void script_ref(void* data_ptr) {
    if (ScriptObjectBase* base = to_script_base(data_ptr)) {
        base->ref_count++;
    }
}

/**
 * @brief Decrease reference count (const version)
 */
inline void script_unref(const void* data_ptr) {
    if (const ScriptObjectBase* base = to_script_base(data_ptr)) {
        ScriptObjectBase* non_const_base = const_cast<ScriptObjectBase*>(base);
        if (--non_const_base->ref_count == 0) {
            // Удаляем как сырую память - деструкторы не вызываются!
            // Это ОСОЗНАННОЕ решение для производительности
            ::operator delete(non_const_base);
        }
    }
}

/**
 * @brief Decrease reference count (non-const version)
 */
inline void script_unref(void* data_ptr) {
    if (ScriptObjectBase* base = to_script_base(data_ptr)) {
        if (--base->ref_count == 0) {
            ::operator delete(base);
        }
    }
}

/**
 * @brief Get reference count
 */
inline int32_t script_ref_count(const void* data_ptr) {
    if (const ScriptObjectBase* base = to_script_base(data_ptr)) {
        return base->ref_count;
    }
    return -1;
}

// =============================================================================
// Type-Safe Creation Function
// =============================================================================

template<typename T, typename... Args>
T* script_create(Args&&... args) {
    auto* obj = static_cast<ScriptObject<T>*>(::operator new(sizeof(ScriptObject<T>)));
    new (obj) ScriptObject<T>(std::forward<Args>(args)...);
    return &obj->data;
}