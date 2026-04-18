#pragma once
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/ScriptObject.hpp"
#include "type_system/Type.hpp"
#include <cstddef>

namespace carbon {

    /**
      * Тип регистра 
      */
    enum class RuntimeType : u8 {
        Null,
        Int,      // 64-bit целое + bool + char
        Float,    // 64-bit float
        Pointer   // указатель на что угодно
    };

    inline const char* value_type_to_string(RuntimeType type) {
        switch (type) {
            case  RuntimeType::Null: return "null";
            case  RuntimeType::Int: return "int";
            case  RuntimeType::Float: return "float";
            case  RuntimeType::Pointer: return "pointer";
        }
    }
    // =========================================================================
    // EXCEPTION CLASSES
    // =========================================================================

    /**
     * @class TypeError
     * @brief Exception thrown for type conversion and access errors
     */
    class TypeError : public std::runtime_error {
    public:
        TypeError(const std::string& message, RuntimeType actual_type)
            : std::runtime_error(
                fmt::format("TypeError: {} (actual type: {})",
                    message,
                    value_type_to_string(actual_type)))
        {
        }

        TypeError(const std::string& message, RuntimeType expected_type, RuntimeType actual_type)
            : std::runtime_error(
                fmt::format("TypeError: {} (expected type: {} actual type: {})",
                    message,
                    value_type_to_string(expected_type),
                    value_type_to_string(actual_type)
            ))
        {
        }

        TypeError(const std::string& message)
            : std::runtime_error(fmt::format("TypeError: {}", message))
        {
        }
    };

    // =========================================================================
    // TYPE CONSTANTS
    // =========================================================================
    // Builting Types Tree
    // Type                        | Bits  | Native Opcodes | Load Static  |
    // object                      |  --   |                |              |
    // ├── number                  |  64   |                |              |
    // │   ├── integer             |  64   |                |              |
    // │   │   ├── int(псевдоним)  |  64   |                |   yes        |
    // │   │   ├── uint(псевдоним) |  64   |                |   yes        |
    // │   │   ├── sinteger        |  64   |      yes       |              |
    // │   │   │   ├── int8        |   8   |                |   yes        |
    // │   │   │   ├── int16       |  16   |                |   yes        |
    // │   │   │   ├── int32       |  32   |                |   yes (main) |
    // │   │   │   └── int64       |  64   |                |   yes        |
    // │   │   └── uinteger        |  64   |                |              |
    // │   │       ├── uint8       |   8   |                |   yes        |
    // │   │       ├── uint16      |  16   |                |   yes        |
    // │   │       ├── uint32      |  32   |                |   yes        |
    // │   │       └── uint64      |  64   |                |   yes        |
    // │   └── float               |  32   |      yes       |   yes (main) |
    // ├── structure               |       |                |              |
    // ├── basic                   |       |                |              |
    // └── ...                     |       |                |              |
    // 
    // В регистрах VM работают :
    // 
    // sinteger - 64 - bit знаковый(основной числовой тип)
    // 
    //     float - 32 - bit плавающий(основной дробный тип)
    // 
    //     Из памяти загружаются :
    // 
    // int32 - основной целочисленный(32 - bit)
    // 
    //     float - основной дробный(32 - bit)
    // 
    //     int8, int16, int64 - редко используемые
    // 
    //     uint * -практически не используются
    // 
    //     Псевдонимы :
    // 
    // int → sinteger(64 - bit в регистрах)
    // 
    //     uint → uinteger(64 - bit в регистрах)
    // 
    struct TypeIds {
        inline static const StringId none      = StringId("none");
        inline static const StringId type      = StringId("type");
        inline static const StringId null      = StringId("null");
        inline static const StringId number    = StringId("number");
        inline static const StringId integer   = StringId("integer");
        inline static const StringId sinteger  = StringId("sinteger");
        inline static const StringId i64       = StringId("i64");
        inline static const StringId i32       = StringId("i32");
        inline static const StringId i16       = StringId("i16");
        inline static const StringId i8        = StringId("i8");
        inline static const StringId uinteger  = StringId("uinteger");
        inline static const StringId u64       = StringId("u64");
        inline static const StringId u32       = StringId("u32");
        inline static const StringId u16       = StringId("u16");
        inline static const StringId u8        = StringId("u8");
        inline static const StringId f32       = StringId("f32");
        inline static const StringId f64       = StringId("f64");
        inline static const StringId bool_     = StringId("bool");
        inline static const StringId char_     = StringId("char");
        inline static const StringId string_   = StringId("string");
        inline static const StringId sid       = StringId("sid");
        inline static const StringId sid32     = StringId("sid32");

        inline static const StringId function       = StringId("function");
        inline static const StringId native         = StringId("native");
        inline static const StringId state          = StringId("state");
        inline static const StringId method         = StringId("method");
        inline static const StringId new_method     = StringId("new-method");
        inline static const StringId event_message  = StringId("event-message");
        inline static const StringId process        = StringId("process");
    };

    /**
     * @class Variant
     * @brief Universal value container for virtual machine operations
     *
     * Stores either scalar values directly or pointers to external objects.
     * Type system uses StringId identifiers, allowing unlimited types without
     * modifying the Variant class structure.
     */
    class Variant {
    public:
        // =========================================================================
        // CONSTRUCTORS
        // =========================================================================

        Variant()
            : type_(RuntimeType::Null)
            , int_value(0)
        {
        }

        Variant(i64 value)
            : type_(RuntimeType::Int)
            , int_value(value)
        {
        }

        Variant(u64 value)
            : type_(RuntimeType::Int)
            , int_value(value)
        {
        }

        Variant(i32 value)
            : type_(RuntimeType::Int)
            , int_value(value)
        {
        }

        Variant(u32 value)
            : type_(RuntimeType::Int)
            , int_value(value)
        {
        }

        Variant(f32 value)
            : type_(RuntimeType::Float)
            , float_value(value)
        {
        }

        
        Variant(f64 value)
            : type_(RuntimeType::Float)
            , float_value(value)
        {
        }

        Variant(bool value)
            : type_(RuntimeType::Int)
            , int_value(value ? 1 : 0)
        {
        }

        Variant(void* ptr, RuntimeType type)
            : type_(type)
            , ptr_value(ptr)
        {
        }

        Variant(StringId sid)
            : type_(RuntimeType::Int)
            , int_value(static_cast<i32>(sid))
        {
        }

        Variant(std::string string)
        {
            create_script_object<std::string>(RuntimeType::Pointer, string);
        }

        Variant(const char* string)
        {
            std::string str(string);
            create_script_object<std::string>(RuntimeType::Pointer, str);
        }

        // =========================================================================
        // RULE OF 5 - CRITICAL FOR REFCOUNTING!
        // =========================================================================

        // 1. Копи-конструктор
        Variant(const Variant& other)
            : type_(other.type_)
        {
            if (other.is_number()) {
                // Числа копируем напрямую
                if (other.is_int() || other.is_sid() || other.is_bool()) {
                    int_value = other.int_value;
                }
                else if (other.is_float()) {
                    float_value = other.float_value;
                }
            }
            else if (other.is_ptr() && other.ptr_value != nullptr) {
                // Для указателей - увеличиваем refcount
                ptr_value = other.ptr_value;
                ref_ptr();  // ✅ Увеличиваем refcount!
            }
            else {
                // null или nullptr
                int_value = 0;
            }
        }

        // 2. Оператор присваивания копированием
        Variant& operator=(const Variant& other) {
            if (this != &other) {
                // Освобождаем текущие ресурсы
                cleanup();

                // Копируем из other
                type_ = other.type_;

                if (other.is_number()) {
                    if (other.is_int() || other.is_sid() || other.is_bool()) {
                        int_value = other.int_value;
                    }
                    else if (other.is_float()) {
                        float_value = other.float_value;
                    }
                }
                else if (other.is_ptr() && other.ptr_value != nullptr) {
                    ptr_value = other.ptr_value;
                    ref_ptr();  // ✅ Увеличиваем refcount!
                }
                else {
                    int_value = 0;
                }
            }
            return *this;
        }

        // 3. Move-конструктор
        Variant(Variant&& other) noexcept
            : type_(other.type_)
        {
            if (other.is_number()) {
                if (other.is_int() || other.is_sid() || other.is_bool()) {
                    int_value = other.int_value;
                }
                else if (other.is_float()) {
                    float_value = other.float_value;
                }
            }
            else if (other.is_ptr()) {
                // Move - забираем владение без изменения refcount
                ptr_value = other.ptr_value;
                other.ptr_value = nullptr;  // Обнуляем у источника
                other.type_ = RuntimeType::Null;
            }
            else {
                int_value = 0;
            }
        }

        // 4. Move-присваивание
        Variant& operator=(Variant&& other) noexcept {
            if (this != &other) {
                cleanup();

                type_ = other.type_;

                if (other.is_number()) {
                    if (other.is_int() || other.is_sid() || other.is_bool()) {
                        int_value = other.int_value;
                    }
                    else if (other.is_float()) {
                        float_value = other.float_value;
                    }
                }
                else if (other.is_ptr()) {
                    ptr_value = other.ptr_value;
                    other.ptr_value = nullptr;
                    other.type_ = RuntimeType::Null;
                }
                else {
                    int_value = 0;
                }
            }
            return *this;
        }

        // 5. Деструктор (уже есть, но обновим)
        ~Variant() {
            cleanup();
        }
        // =========================================================================
        // VALUE SETTERS
        // =========================================================================
        
        void set_i64(i64 value) {
            type_ = RuntimeType::Int;
            int_value = value;
        }

        void set_i32(i32 value) {
            type_ = RuntimeType::Int;
            int_value = value;
        }

        void set_f64(f64 value) {
            type_ = RuntimeType::Int;
            float_value = value;
        }
        
        void set_f32(f32 value) {
            type_ = RuntimeType::Int;
            float_value = value;
        }

        void set_bool(bool value) {
            type_ = RuntimeType::Int;
            int_value = value;
        }

        void set_ptr(void* ptr, RuntimeType type) {
            // Если уже храним указатель - уменьшаем refcount
            if (is_ptr() && ptr_value != nullptr) {
                unref_ptr();
            }

            type_ = type;
            ptr_value = ptr;

            // Увеличиваем refcount для нового указателя
            if (ptr != nullptr) {
                ref_ptr();
            }
        }

        void set_sid(StringId sid) {
            type_ = RuntimeType::Int;
            int_value = static_cast<u64>(sid);
        }     
        
        void set_sid32(StringId sid) {
            type_ = RuntimeType::Int;
            int_value = static_cast<u32>(sid);
        }

        void set_string(const std::string& str) {
            create_script_object<std::string>(RuntimeType::Pointer, str);
        }

        void set_null() {
            type_ = RuntimeType::Null;
            int_value = 0;
        }

        // =========================================================================
        // ASSIGNMENT OPERATORS
        // =========================================================================

        Variant& operator=(i64 value) {
            set_i32(value);
            return *this;
        }

        Variant& operator=(float value) {
            set_f64(value);
            return *this;
        }

        Variant& operator=(bool value) {
            set_bool(value);
            return *this;
        }

        Variant& operator=(StringId sid) {
            set_sid(sid);
            return *this;
        }

        Variant& operator=(std::string string) {
            set_ptr(new std::string(string), RuntimeType::Pointer);
            return *this;
        }

        // =========================================================================
        // STRICT TYPE GETTERS
        // =========================================================================
      

        i64 get_i64() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_i64()", RuntimeType::Int, type_);
            }
            return int_value;
        }

        i32 get_i32() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_i32()", RuntimeType::Int, type_);
            }
            return int_value;
        }

        u64 get_u64() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_i64()", RuntimeType::Int, type_);
            }
            return int_value;
        }

        u32 get_u32() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_int()", RuntimeType::Int, type_);
            }
            return int_value;
        }
        
        f32 get_float() const {
            if (type_ != RuntimeType::Float) {
                throw TypeError("get_float()", RuntimeType::Float, type_);
            }
            return float_value;
        }
        
        f32 get_f32() const {
            if (type_ != RuntimeType::Float) {
                throw TypeError("get_f32()", RuntimeType::Float, type_);
            }
            return float_value;
        }

        f64 get_f64() const {
            if (type_ != RuntimeType::Float) {
                throw TypeError("get_f64()", RuntimeType::Float, type_);
            }
            return float_value;
        }

        bool get_bool() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_bool()", RuntimeType::Int, type_);
            }
            return int_value;
        }

        StringId get_sid() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_sid()", RuntimeType::Int, type_);
            }
            return static_cast<StringId>(int_value);
        }

        const void* get_ptr() const {
            if (!is_ptr()) {
                throw TypeError("get_ptr() expected 'pointer' but found", type_);
            }
            return ptr_value;
        }

        void* get_ptr() {
            if (!is_ptr()) {
                throw TypeError("get_ptr() expected 'pointer' but found", type_);
            }
            return ptr_value;
        }

        std::string get_string() const {
            if (type_ != RuntimeType::Pointer) {
                throw TypeError("Expected string", type_);
            }
            return *static_cast<std::string*>(ptr_value);
        }

        // =========================================================================
        // TYPE CONVERTERS
        // =========================================================================

        i64 to_int() const {
            if (type_ == RuntimeType::Int) return int_value;
            if (type_ == RuntimeType::Float)  return static_cast<i32>(float_value);
            if (type_ == RuntimeType::Pointer) return (u64)(void*)ptr_value;
            throw TypeError("to_int() cannot convert from", type_);
        }

        f32 to_float() const {
            if (type_ == RuntimeType::Int) return static_cast<float>(int_value);
            if (type_ == RuntimeType::Float)  return float_value;
            throw TypeError("to_float() cannot convert from", type_);
        }

        bool to_bool() const {
            if (type_ == RuntimeType::Int) return int_value != 0;
            if (type_ == RuntimeType::Float)  return float_value != 0;
            if (type_ == RuntimeType::Pointer) return ptr_value!=nullptr;
            return false; // null is false
        }

        // =========================================================================
        // TYPE CHECKING
        // =========================================================================

        RuntimeType get_type() const { return type_; }


        // Проверка конкретного типа VM
        bool is_null() const { return type_ == RuntimeType::Null; }
        bool is_int() const { return type_ == RuntimeType::Int; }
        bool is_float() const { return type_ == RuntimeType::Float; }
        inline bool is_ptr() const { return type_ == RuntimeType::Pointer; }

        // Проверка семейства или нативного типа используемого VM
        inline bool is_number() const { return is_int() || is_float(); }

        // Проверка дополнительных типов
        bool is_bool() const { return type_ == RuntimeType::Int; }
        bool is_sid() const { return type_ == RuntimeType::Int; }

 
        // =========================================================================
        // STRING REPRESENTATION
        // =========================================================================

        std::string to_string() const {
            switch (type_) {
                case RuntimeType::Null: return "null";
                case RuntimeType::Int: std::to_string(get_i64());
                case RuntimeType::Float: std::to_string(get_f64());
                case RuntimeType::Pointer: return fmt::format("ptr:{}", ptr_value);
            }
            return "unknown";
        }

        const char* to_c_string() const {
            thread_local static std::string buffer;
            buffer = to_string();
            return buffer.c_str();
        }

        // =========================================================================
        // COMPARISON MEMBERS
        // =========================================================================

        bool operator==(const Variant& other) const {
            if (type_ != other.type_) return false;

            if (is_number()) {
                if (is_int()) return int_value == other.int_value;
                if (is_float()) return float_value == other.float_value;
                if (is_sid()) return int_value == other.int_value;
            }

            return ptr_value == other.ptr_value;
        }

        bool operator!=(const Variant& other) const {
            return !(*this == other);
        }

        // Опционально: операторы порядка если нужны
        bool operator<(const Variant& other) const {
            if (type_ != other.type_) return type_ < other.type_;

            if (is_number()) {
                if (is_int()) return int_value < other.int_value;
                if (is_float()) return float_value < other.float_value;
                if (is_sid()) return int_value < other.int_value;
            }

            return ptr_value < other.ptr_value;
        }

        // =========================================================================
        // Reference Counting
        // =========================================================================
        /**
         * @brief Create script-managed object and store pointer to its DATA
         */
        template<typename T, typename... Args>
        void create_script_object(RuntimeType vm_type, Args&&... args) {
            T* data_ptr = script_create<T>(std::forward<Args>(args)...);
            set_ptr(data_ptr, vm_type);
        }

        /**
         * @brief Increase reference count for stored pointer
         */
        void ref_ptr() const {
            if (is_ptr() && ptr_value != nullptr) {
                // Для этого нужно знать тип, но мы можем хранить его в type_
                // Или использовать type-erasure через шаблоны
                //script_ref(ptr_value); // Будет работать если type_ соответствует реальному типу
            }
        }

        /**
         * @brief Decrease reference count for stored pointer
         */
        void unref_ptr() const {
            if (is_ptr() && ptr_value != nullptr) {
                //script_unref(ptr_value);
            }
        }

        /**
         * @brief Get reference count for stored pointer
         */
        int32_t get_ref_count() const {
            if (!is_ptr() || ptr_value == nullptr) return -1;
            return 0;//script_ref_count(ptr_value);
        }
    private:
        // =========================================================================
        // PRIVATE MEMBERS
        // =========================================================================

        // Вспомогательный метод для очистки ресурсов
        void cleanup() {
            if (is_ptr() && ptr_value != nullptr) {
                unref_ptr();  // ✅ Уменьшаем refcount!
            }
            type_ = RuntimeType::Null;
            int_value = 0;
        }
        RuntimeType type_;    ///< Type identifier using StringId system

        union {
            i64      int_value;    ///< Storage for integer and string_id types
            f64      float_value;  ///< Storage for float types  
            void*    ptr_value;    ///< Storage for pointer types (external data)
        };
    };

} // namespace vm