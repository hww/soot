#pragma once
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/ScriptObject.hpp"

namespace runtime::lib {
    // =========================================================================
    // EXCEPTION CLASSES
    // =========================================================================

#ifdef VM_INT_64_BITS
    typedef s64 vm_int;
    typedef f64 vm_float;
#else
    typedef s32 vm_int;
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
                    lib::to_string(actual_type))
            )
        {
        }
        TypeError(const std::string& message, StringId expected_type, StringId actual_type)
            : std::runtime_error(
                fmt::format("TypeError: {} (expected type: {} actual type: {})",
                    message,
                    lib::to_string(expected_type),
                    lib::to_string(actual_type))
            )
        {
        }

        TypeError(const std::string& message)
            : std::runtime_error(fmt::format("TypeError: {}", message))
        {
        }
    };

    namespace type {
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
        const StringId null         = SID("null");
        const StringId number       = SID("number");    // Базовый
        const StringId integer      = SID("integer");   // number::integer
        const StringId sinteger     = SID("sinteger");  // integer::sinteger
        const StringId i64          = SID("i64");       // integer::sinteger::s64
        const StringId i32          = SID("s32");       // integer::sinteger::s32
        const StringId i16          = SID("i16");       // integer::sinteger::s16
        const StringId i8           = SID("i8");        // integer::sinteger::s8
        const StringId uinteger     = SID("uinteger");  // integer::uinteger
        const StringId u64          = SID("u64");       // integer::uinteger::i64
        const StringId u32          = SID("u32");       // integer::uinteger::s32
        const StringId u16          = SID("u16");       // integer::uinteger::i16
        const StringId u8           = SID("u8");        // integer::uinteger::i8
        const StringId f32          = SID("float");     // number::float
        const StringId boolean      = SID("bool");
        const StringId string_id    = SID("string_id"); // integer::sinteger::string_id
        const StringId native       = SID("native");
        const StringId string       = SID("string");
        const StringId function     = SID("function");
#ifdef VM_INT_64_BITS
        const StringId _int_     = i64;
        const StringId _float_   = f32;
#else
        const StringId _int_     = u32;
        const StringId _float_   = f32;
#endif
    }

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
            : type_(type::null)
            , int_value(0)
        {
        }

        Variant(vm_int value)
            : type_(type::_int_)
            , int_value(value)
        {
        }

        Variant(vm_float value)
            : type_(type::_float_)
            , float_value(value)
        {
        }

        Variant(bool value)
            : type_(type::_int_)
            , int_value(value)
        {
        }

        Variant(void* ptr, StringId type)
            : type_(type)
            , ptr_value(ptr)
        {
        }

        Variant(StringId sid)
            : type_(type::string_id)
            , int_value(static_cast<s32>(sid))
        {
        }

        Variant(std::string string)
        {
            create_script_object<std::string>(type::string, string);
        }

        Variant(const char* string)
        {
            std::string str(string);
            create_script_object<std::string>(type::string, str);
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
                other.type_ = type::null;
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
                    other.type_ = type::null;
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
            type_ = type::u64;
            int_value = value;
        }

        void set_int32(s32 value) {
            type_ = type::u32;
            int_value = value;
        }

        void set_float(float value) {
            type_ = type::f32;
            float_value = value;
        }

        void set_bool(bool value) {
            type_ = type::boolean;
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
            type_ = type::string_id;
            int_value = static_cast<s32>(sid);
        }

        void set_string(const std::string& str) {
            create_script_object<std::string>(type::string, str);
        }

        void set_null() {
            type_ = type::null;
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
            if (type_ != type::_int_) {
                throw TypeError("get_int()", type::_int_, type_);
            }
            return int_value;
        }

        s32 get_int32() const {
            if (type_ != type::_int_) {
                throw TypeError("get_int()", type::_int_, type_);
            }
            return int_value;
        }

        vm_float get_float() const {
            if (type_ != type::_float_) {
                throw TypeError("get_float()", type::_float_, type_);
            }
            return float_value;
        }

        bool get_bool() const {
            if (type_ != type::_int_) {
                throw TypeError("get_bool()", type::_int_, type_);
            }
            return int_value;
        }

        StringId get_sid() const {
            if (type_ != type::string_id && type_ != type::_int_) {
                throw TypeError("get_sid()", type::string_id, type_);
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
            if (type_ != type::string) {
                throw TypeError("Expected string", type_);
            }
            return *static_cast<std::string*>(ptr_value);
        }

        // =========================================================================
        // TYPE CONVERTERS
        // =========================================================================

        vm_int to_int() const {
            if (type_ == type::_int_) return int_value;
            if (type_ == type::_float_) return static_cast<s32>(float_value);
            if (type_ == type::boolean) return int_value;
            if (type_ == type::string_id) return static_cast<s32>(int_value);
            throw TypeError("to_int() cannot convert from", type_);
        }

        vm_float to_float() const {
            if (type_ == type::_float_) return float_value;
            if (type_ == type::_int_) return static_cast<float>(int_value);
            if (type_ == type::boolean) return int_value;
            throw TypeError("to_float() cannot convert from", type_);
        }

        bool to_bool() const {
            if (type_ == type::boolean) return int_value;
            if (type_ == type::_int_) return int_value != 0;
            if (type_ == type::_float_) return float_value != 0.0f;
            if (type_ == type::string_id) return int_value != 0;
            else return ptr_value != nullptr;
            return false; // null is false
        }

        // =========================================================================
        // TYPE CHECKING
        // =========================================================================

        StringId get_type() const { return type_; }

        bool is_null() const { return type_ == type::null; }

        // Проверка семейства или нативного типа используемого VM
        inline bool is_number() const { return type_ == type::_int_ || type_ == type::_float_ || type_ == type::string_id; }
        inline bool is_ptr() const { return !is_number(); }

        // Проверка конкретного типа VM
        bool is_int() const { return type_ == type::_int_; }
        bool is_float() const { return type_ == type::_float_; }
        bool is_bool() const { return type_ == type::_int_; }
        bool is_sid() const { return type_ == type::string_id; }

 
        // there are possible thousands of pointer
        bool is_function() const { return type_ == type::function; }
        bool is_native() const   { return type_ == type::native; }
        bool is_string() const   { return type_ == type::string; }

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
            type_ = type::null;
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