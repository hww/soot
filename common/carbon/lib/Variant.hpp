#pragma once
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/ScriptObject.hpp"

namespace carbon::lib {
    // =========================================================================
    // EXCEPTION CLASSES
    // =========================================================================

#ifdef VM_INT_64_BITS
    typedef s64 vm_int;
    typedef u64 vm_uint;
    typedef f64 vm_float;
#else
    typedef s32 vm_int;
    typedef u32 vm_uint;
    typedef f64 vm_float;
#endif
    /**
     * @class TypeError
     * @brief Exception thrown for type conversion and access errors
     */
    class TypeError : public std::runtime_error {
    public:
        TypeError(const std::string& message, StringId actual_type)
            : std::runtime_error(
                fmt::format("TypeError: {} (actual type: {})",
                    message,
                    actual_type.to_cstring()))
        {
        }
        TypeError(const std::string& message, StringId expected_type, StringId actual_type)
            : std::runtime_error(
                fmt::format("TypeError: {} (expected type: {} actual type: {})",
                    message,
                    expected_type.to_cstring(),
                    actual_type.to_cstring())
            )
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
        inline static const StringId unnamed   = StringId("unnamed");
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
        inline static const StringId f32       = StringId("float");
        inline static const StringId boolean   = StringId("bool");
        inline static const StringId string_id = StringId("string_id");
        inline static const StringId native    = StringId("native");
        inline static const StringId string    = StringId("string");
        inline static const StringId function  = StringId("function");
        inline static const StringId event_message  = StringId("event-message");
        inline static const StringId process   = StringId("process");


    #ifdef VM_INT_64_BITS
        inline static const StringId _int_     = i64;
        inline static const StringId _float_   = f32;
    #else
        inline static const StringId _int_     = u32;
        inline static const StringId _float_   = f32;
    #endif
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
            : type_(TypeIds::null)
            , int_value(0)
        {
        }

        Variant(vm_int value)
            : type_(TypeIds::_int_)
            , int_value(value)
        {
        }

        Variant(vm_uint value)
            : type_(TypeIds::_int_)
            , int_value(value)
        {
        }

        Variant(vm_float value)
            : type_(TypeIds::_float_)
            , float_value(value)
        {
        }

        Variant(bool value)
            : type_(TypeIds::_int_)
            , int_value(value)
        {
        }

        Variant(void* ptr, StringId type)
            : type_(type)
            , ptr_value(ptr)
        {
        }

        Variant(StringId sid)
            : type_(TypeIds::string_id)
            , int_value(static_cast<s32>(sid))
        {
        }

        Variant(std::string string)
        {
            create_script_object<std::string>(TypeIds::string, string);
        }

        Variant(const char* string)
        {
            std::string str(string);
            create_script_object<std::string>(TypeIds::string, str);
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
                other.type_ = TypeIds::null;
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
                    other.type_ = TypeIds::null;
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
        
        void set_int(vm_int value) {
            type_ = TypeIds::u64;
            int_value = value;
        }

        void set_int32(s32 value) {
            type_ = TypeIds::u32;
            int_value = value;
        }

        void set_float(float value) {
            type_ = TypeIds::f32;
            float_value = value;
        }

        void set_bool(bool value) {
            type_ = TypeIds::boolean;
            int_value = value;
        }

        void set_ptr(void* ptr, StringId type) {
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
            type_ = TypeIds::string_id;
            int_value = static_cast<s32>(sid);
        }

        void set_string(const std::string& str) {
            create_script_object<std::string>(TypeIds::string, str);
        }

        void set_null() {
            type_ = TypeIds::null;
            int_value = 0;
        }

        // =========================================================================
        // ASSIGNMENT OPERATORS
        // =========================================================================

        Variant& operator=(vm_int value) {
            set_int32(value);
            return *this;
        }

        Variant& operator=(float value) {
            set_float(value);
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
            set_ptr(new std::string(string), SID("string"));
            return *this;
        }

        // =========================================================================
        // STRICT TYPE GETTERS
        // =========================================================================


        vm_int get_int() const {
            if (type_ != TypeIds::_int_) {
                throw TypeError("get_int()", TypeIds::_int_, type_);
            }
            return int_value;
        }

        s32 get_int32() const {
            if (type_ != TypeIds::_int_) {
                throw TypeError("get_int()", TypeIds::_int_, type_);
            }
            return int_value;
        }

        vm_float get_float() const {
            if (type_ != TypeIds::_float_) {
                throw TypeError("get_float()", TypeIds::_float_, type_);
            }
            return float_value;
        }

        bool get_bool() const {
            if (type_ != TypeIds::_int_) {
                throw TypeError("get_bool()", TypeIds::_int_, type_);
            }
            return int_value;
        }

        StringId get_sid() const {
            if (type_ != TypeIds::string_id && type_ != TypeIds::_int_) {
                throw TypeError("get_sid()", TypeIds::string_id, type_);
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
        void* get_ptr(StringId type) const {
            if (type_ == type) {
                throw TypeError("get_ptr(type)", type, type_);
            }
            return ptr_value;
        }

        std::string get_string() const {
            if (type_ != TypeIds::string) {
                throw TypeError("Expected string", type_);
            }
            return *static_cast<std::string*>(ptr_value);
        }

        // =========================================================================
        // TYPE CONVERTERS
        // =========================================================================

        vm_int to_int() const {
            if (type_ == TypeIds::_int_) return int_value;
            if (type_ == TypeIds::_float_) return static_cast<s32>(float_value);
            if (type_ == TypeIds::boolean) return int_value;
            if (type_ == TypeIds::string_id) return static_cast<s32>(int_value);
            throw TypeError("to_int() cannot convert from", type_);
        }

        vm_float to_float() const {
            if (type_ == TypeIds::_float_) return float_value;
            if (type_ == TypeIds::_int_) return static_cast<float>(int_value);
            if (type_ == TypeIds::boolean) return int_value;
            throw TypeError("to_float() cannot convert from", type_);
        }

        bool to_bool() const {
            if (type_ == TypeIds::boolean) return int_value;
            if (type_ == TypeIds::_int_) return int_value != 0;
            if (type_ == TypeIds::_float_) return float_value != 0.0f;
            if (type_ == TypeIds::string_id) return int_value != 0;
            else return ptr_value != nullptr;
            return false; // null is false
        }

        // =========================================================================
        // TYPE CHECKING
        // =========================================================================

        StringId get_type() const { return type_; }

        bool is_null() const { return type_ == TypeIds::null; }

        // Проверка семейства или нативного типа используемого VM
        inline bool is_number() const { return type_ == TypeIds::_int_ || type_ == TypeIds::_float_ || type_ == TypeIds::string_id; }
        inline bool is_ptr() const { return !is_number(); }

        // Проверка конкретного типа VM
        bool is_int() const { return type_ == TypeIds::_int_; }
        bool is_float() const { return type_ == TypeIds::_float_; }
        bool is_bool() const { return type_ == TypeIds::_int_; }
        bool is_sid() const { return type_ == TypeIds::string_id; }

 
        // there are possible thousands of pointer
        bool is_function() const { return type_ == TypeIds::function; }
        bool is_native() const   { return type_ == TypeIds::native; }
        bool is_string() const   { return type_ == TypeIds::string; }

        // =========================================================================
        // STRING REPRESENTATION
        // =========================================================================

        std::string to_string() const {
            if (is_null())    return "null";
            if (is_int())     return std::to_string(get_int());
            if (is_float())   return std::to_string(get_float());
            if (is_bool())    return get_bool() ? "true" : "false";
            if (is_sid())     return fmt::format("sid:{}", get_sid());
            if (is_string())  return get_string();
            if (is_ptr())     return fmt::format("ptr:{}", ptr_value);
            if (is_function())  return "lambda";
            if (is_native())  return "native";
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
        void create_script_object(StringId type, Args&&... args) {
            T* data_ptr = script_create<T>(std::forward<Args>(args)...);
            set_ptr(data_ptr, type);
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
            type_ = TypeIds::null;
            int_value = 0;
        }
        StringId type_;    ///< Type identifier using StringId system

        union {
            vm_int   int_value;    ///< Storage for integer and string_id types
            vm_float float_value;  ///< Storage for float types  
            void*    ptr_value;    ///< Storage for pointer types (external data)
        };
    };

} // namespace vm