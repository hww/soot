#pragma once
#include "CommonTypes.hpp"
#include "carbon/gc/GC.hpp"
#include "carbon/lib/StringId.hpp"
#include "type_system/Type.hpp"

#include "fmt/format.h"

#include <cstddef>
#include <cstring>

namespace carbon {

    /**
     * Тип регистра
     */
    enum class RuntimeType : u8 {
        Null,
        Bool,
        Int,     // 64-bit целое + bool + char
        Float,   // 64-bit float
        Pointer, // указатель на что угодно
        String
    };

    inline const char *value_type_to_string(RuntimeType type) {
        switch (type) {
        case RuntimeType::Null: return "null";
        case RuntimeType::Bool: return "bool";
        case RuntimeType::Int: return "int";
        case RuntimeType::Float: return "float";
        case RuntimeType::Pointer: return "pointer";
        case RuntimeType::String: return "string";
        }
        return "unknown";
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
        TypeError(const std::string &message, RuntimeType actual_type)
            : std::runtime_error(fmt::format("TypeError: {} (actual type: {})", message,
                                             value_type_to_string(actual_type))) {}

        TypeError(const std::string &message, RuntimeType expected_type, RuntimeType actual_type)
            : std::runtime_error(fmt::format("TypeError: {} (expected type: {} actual type: {})",
                                             message, value_type_to_string(expected_type),
                                             value_type_to_string(actual_type))) {}

        TypeError(const std::string &message)
            : std::runtime_error(fmt::format("TypeError: {}", message)) {}
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
        inline static const StringId none = StringId("none");
        inline static const StringId type = StringId("type");
        inline static const StringId null = StringId("null");
        inline static const StringId number = StringId("number");
        inline static const StringId integer = StringId("integer");
        inline static const StringId sinteger = StringId("sinteger");
        inline static const StringId i64 = StringId("i64");
        inline static const StringId i32 = StringId("i32");
        inline static const StringId i16 = StringId("i16");
        inline static const StringId i8 = StringId("i8");
        inline static const StringId uinteger = StringId("uinteger");
        inline static const StringId u64 = StringId("u64");
        inline static const StringId u32 = StringId("u32");
        inline static const StringId u16 = StringId("u16");
        inline static const StringId u8 = StringId("u8");
        inline static const StringId f32 = StringId("f32");
        inline static const StringId f64 = StringId("f64");
        inline static const StringId bool_ = StringId("bool");
        inline static const StringId char_ = StringId("char");
        inline static const StringId string_ = StringId("string");
        inline static const StringId sid = StringId("sid");
        inline static const StringId sid32 = StringId("sid32");

        inline static const StringId function = StringId("function");
        inline static const StringId native = StringId("native");
        inline static const StringId state = StringId("state");
        inline static const StringId method = StringId("method");
        inline static const StringId new_method = StringId("new-method");
        inline static const StringId event_message = StringId("event-message");
        inline static const StringId process = StringId("process");
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

        Variant() : type_(RuntimeType::Null), int_value(0) {}

        Variant(i64 value) : type_(RuntimeType::Int), int_value(value) {}

        Variant(u64 value) : type_(RuntimeType::Int), int_value(value) {}

        Variant(i32 value) : type_(RuntimeType::Int), int_value(value) {}

        Variant(u32 value) : type_(RuntimeType::Int), int_value(value) {}

        Variant(f32 value) : type_(RuntimeType::Float), float_value(value) {}

        Variant(f64 value) : type_(RuntimeType::Float), float_value(value) {}

        Variant(bool value) : type_(RuntimeType::Bool), int_value(value ? 1 : 0) {}

        Variant(void *ptr, RuntimeType type) : type_(type), ptr_value(ptr) {}

        Variant(StringId sid) : type_(RuntimeType::Int), int_value(static_cast<u64>(sid)) {}

        Variant(std::string string) { set_string(string); }

        Variant(const char *string) {
            if (string) set_string(std::string(string));
            else {
                type_ = RuntimeType::Null;
                int_value = 0;
            }
        }
        
        // Const-pointer constructor. Variant stores a mutable void*, but
        // callers may pass const pointers (e.g. from a const Variant or from
        // a string literal). The const_cast is safe: Variant never mutates
        // through this pointer; it only reads or forwards it.
        Variant(const void *ptr, RuntimeType type)
            : type_(type), ptr_value(const_cast<void *>(ptr)) {}

        // =========================================================================
        // RULE OF 5
        // =========================================================================

        Variant(const Variant &other) : type_(other.type_) {
            if (other.is_int() || other.is_bool()) {
                int_value = other.int_value;           
            } else if (other.is_float()) {
                float_value = other.float_value;
            } else if (other.type_ == RuntimeType::Pointer || other.type_ == RuntimeType::String) {
                ptr_value = other.ptr_value;
            } else {
                int_value = 0; /* null */
            }
        }

        Variant &operator=(const Variant &other) {
            if (this != &other) {

                type_ = other.type_;

                if (other.is_int() || other.is_bool()) {
                    int_value = other.int_value;
                } else if (other.is_float()) {
                    float_value = other.float_value;
                } else if (other.type_ == RuntimeType::Pointer || other.type_ == RuntimeType::String) {
                    ptr_value = other.ptr_value;
                } else {
                    int_value = 0; /* null */
                }
            }
            return *this;
        }

        Variant(Variant &&other) noexcept : type_(other.type_) {
            if (other.is_int() || other.is_bool()) {
                int_value = other.int_value;
            } else if (other.is_float()) {
                float_value = other.float_value;
            } else if (other.type_ == RuntimeType::Pointer || other.type_ == RuntimeType::String) {
                ptr_value = other.ptr_value;
                other.ptr_value = nullptr;
                other.type_ = RuntimeType::Null;
            } else {
                int_value = 0; /* null */
            }
        }

        Variant &operator=(Variant &&other) noexcept {
            if (this != &other) {

                type_ = other.type_;

                if (other.is_int() || other.is_bool()) {
                    int_value = other.int_value;
                } else if (other.is_float()) {
                    float_value = other.float_value;
                } else if (other.type_ == RuntimeType::Pointer || other.type_ == RuntimeType::String) {
                    ptr_value = other.ptr_value;
                    other.ptr_value = nullptr;
                    other.type_ = RuntimeType::Null;
                } else {
                    int_value = 0; /* null */
                }
            }
            return *this;
        }

        ~Variant() {  }

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
            type_ = RuntimeType::Float;
            float_value = value;
        }

        void set_f32(f32 value) {
            type_ = RuntimeType::Float;
            float_value = value;
        }

        void set_bool(bool value) {
            type_ = RuntimeType::Bool;
            int_value = value ? 1 : 0;
        }

        void set_ptr(void *ptr, RuntimeType type) {
            type_ = type;
            ptr_value = ptr;
        }

        void set_sid(StringId sid) {
            type_ = RuntimeType::Int;
            int_value = static_cast<u64>(sid);
        }

        void set_sid32(StringId sid) {
            type_ = RuntimeType::Int;
            int_value = static_cast<u64>(sid);
        }

        void set_string(const std::string &str) {
            const char *cstr = gc_make_cstr(str);
            if (cstr) {
                type_ = RuntimeType::String;
                ptr_value = const_cast<char *>(cstr);
            } else {
                type_ = RuntimeType::Null;
                int_value = 0;
            }
        }

        void set_null() {
            type_ = RuntimeType::Null;
            int_value = 0;
        }

        // =========================================================================
        // ASSIGNMENT OPERATORS
        // =========================================================================

        Variant &operator=(i64 value) {
            set_i64(value);
            return *this;
        }

        Variant &operator=(float value) {
            set_f64(value);
            return *this;
        }

        Variant &operator=(bool value) {
            set_bool(value);
            return *this;
        }

        Variant &operator=(StringId sid) {
            set_sid(sid);
            return *this;
        }

        Variant &operator=(std::string string) {
            set_string(string);
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
                throw TypeError("get_u64()", RuntimeType::Int, type_);
            }
            return int_value;
        }

        u32 get_u32() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_u32()", RuntimeType::Int, type_);
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
            if (type_ != RuntimeType::Int && type_ != RuntimeType::Bool) {
                throw TypeError("get_bool()", RuntimeType::Bool, type_);
            }
            return int_value;
        }

        StringId get_sid() const {
            if (type_ != RuntimeType::Int) {
                throw TypeError("get_sid()", RuntimeType::Int, type_);
            }
            return static_cast<StringId>(int_value);
        }

        const void *get_ptr() const {
            if (!is_ptr()) { throw TypeError("get_ptr() expected 'pointer' but found", type_); }
            return ptr_value;
        }

        void *get_ptr() {
            if (!is_ptr()) { throw TypeError("get_ptr() expected 'pointer' but found", type_); }
            return ptr_value;
        }

        std::string get_string() const {
            if (type_ != RuntimeType::String) { throw TypeError("Expected string", type_); }
            if (ptr_value == nullptr) return "";
            return std::string(static_cast<const char *>(ptr_value));
        }

         // Mutable pointer access for native functions that receive
        // `const Variant*` but need to mutate the pointee. The Variant
        // itself is not mutated; only the object it points to is.
        template <typename T> T *get_mut_ptr() const {
            if (!is_ptr()) { throw TypeError("get_mut_ptr() expected 'pointer' but found", type_); }
            return static_cast<T *>(const_cast<void *>(ptr_value));
        }

        // =========================================================================
        // TYPE CONVERTERS
        // =========================================================================

        i64 to_int() const {
            if (type_ == RuntimeType::Int) return int_value;
            if (type_ == RuntimeType::Float) return static_cast<i32>(float_value);
            if (type_ == RuntimeType::Pointer) return (u64)(void *)ptr_value;
            if (type_ == RuntimeType::Bool) return int_value;
            throw TypeError("to_int() cannot convert from", type_);
        }

        f32 to_float() const {
            if (type_ == RuntimeType::Int) return static_cast<float>(int_value);
            if (type_ == RuntimeType::Float) return float_value;
            if (type_ == RuntimeType::Bool) return static_cast<float>(int_value);
            throw TypeError("to_float() cannot convert from", type_);
        }

        bool to_bool() const {
            if (type_ == RuntimeType::Bool) return int_value != 0;
            if (type_ == RuntimeType::Int) return int_value != 0;

            if (type_ == RuntimeType::Float) return float_value != 0;
            if (type_ == RuntimeType::Pointer) return ptr_value != nullptr;
            if (type_ == RuntimeType::String) return ptr_value != nullptr;
            return false;
        }

        // =========================================================================
        // TYPE CHECKING
        // =========================================================================

        RuntimeType get_type() const { return type_; }

        inline bool is_null()   const { return type_ == RuntimeType::Null; }
        inline bool is_bool()   const { return type_ == RuntimeType::Bool; }
        inline bool is_int() const { return type_ == RuntimeType::Int; }
        inline bool is_float()  const { return type_ == RuntimeType::Float; }
        inline bool is_string() const { return type_ == RuntimeType::String; }
        inline bool is_ptr() const { return type_ == RuntimeType::Pointer; }

        inline bool is_number() const {
            return type_ == RuntimeType::Int || type_ == RuntimeType::Float;
        }

        // =========================================================================
        // STRING REPRESENTATION
        // =========================================================================

        std::string to_string() const {
            switch (type_) {
            case RuntimeType::Null:  return "null";
            case RuntimeType::Bool:  return int_value ? "true" : "false";
            case RuntimeType::Int:   return std::to_string(get_i64());
            case RuntimeType::Float: return std::to_string(get_f64());
            case RuntimeType::String:
                if (ptr_value == nullptr) return "";
                return std::string(static_cast<const char *>(ptr_value));
            case RuntimeType::Pointer:
                if (ptr_value == nullptr) return "null";
                return fmt::format("ptr:0x{:X}", reinterpret_cast<uintptr_t>(ptr_value));
            }
            return "unknown";
        }

        const char *to_c_string() const {
            thread_local static std::string buffer;
            buffer = to_string();
            return buffer.c_str();
        }

        // =========================================================================
        // COMPARISON MEMBERS
        // =========================================================================

        bool operator==(const Variant &other) const {
            if (type_ != other.type_) return false;

            if (is_int())   return int_value == other.int_value;
            if (is_float()) return float_value == other.float_value;
            if (is_string()) {
                if (ptr_value == nullptr && other.ptr_value == nullptr) return true;
                if (ptr_value == nullptr || other.ptr_value == nullptr) return false;
                return std::strcmp(static_cast<const char *>(ptr_value),
                                   static_cast<const char *>(other.ptr_value)) == 0;
            }
            return ptr_value == other.ptr_value;
        }

        bool operator!=(const Variant &other) const { return !(*this == other); }

        bool operator<(const Variant &other) const {
            if (type_ != other.type_) return type_ < other.type_;
            if (is_null()) return true;
   
            if (is_bool()) return int_value < other.int_value;
            if (is_int()) return int_value < other.int_value;
            if (is_float()) return float_value < other.float_value;
            if (is_string()) {
                const char *a = static_cast<const char *>(ptr_value);
                const char *b = static_cast<const char *>(other.ptr_value);
                if (!a) return b != nullptr;
                if (!b) return false;
                return std::strcmp(a, b) < 0;
            }

            return ptr_value < other.ptr_value;
        }

    private:
        static const char *gc_make_cstr(const std::string &value) {
            const std::size_t n = value.size() + 1;
            void             *payload = gc::alloc(SID("cstr"), n, 1);
            if (!payload) return nullptr;
            std::memcpy(payload, value.c_str(), n);
            return static_cast<const char *>(payload);
        }

        RuntimeType type_;

        union {
            i64   int_value;
            f64   float_value;
            void *ptr_value;
        };
    };

} // namespace carbon

// ============================================================================
// fmt::formatter<Variant>
// ============================================================================

template <> struct fmt::formatter<carbon::Variant> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

template <typename FormatContext>
    auto format(const carbon::Variant &v, FormatContext &ctx) const {
        using carbon::RuntimeType;

        switch (v.get_type()) {
        case RuntimeType::Null: return fmt::format_to(ctx.out(), "null");
        case RuntimeType::Int: return fmt::format_to(ctx.out(), "{}", v.get_i64());
        case RuntimeType::Float: return fmt::format_to(ctx.out(), "{}", v.get_f64());
        case RuntimeType::Pointer: {
            const void *raw = v.get_ptr();
            if (raw == nullptr) return fmt::format_to(ctx.out(), "null");
            return fmt::format_to(ctx.out(), "ptr:0x{:X}", reinterpret_cast<uintptr_t>(raw));
        }
        }
        return fmt::format_to(ctx.out(), "<unknown>");
    }
};