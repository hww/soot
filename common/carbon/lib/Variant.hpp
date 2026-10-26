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
        Int,     // 64-bit целое + bool + char
        Float,   // 64-bit float
        Pointer, // указатель на что угодно
    };

    inline const char *value_type_to_string(RuntimeType type) {
        switch (type) {
        case RuntimeType::Null: return "null";
        case RuntimeType::Int: return "int";
        case RuntimeType::Float: return "float";
        case RuntimeType::Pointer: return "pointer";
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

        Variant(bool value) : type_(RuntimeType::Int), int_value(value ? 1 : 0) {}

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
            if (other.is_int()) {
                int_value = other.int_value;           
            } else if (other.is_float()) {
                float_value = other.float_value;
            } else if (other.type_ == RuntimeType::Pointer) {
                ptr_value = other.ptr_value;
            } else {
                int_value = 0; /* null */
            }
        }

        Variant &operator=(const Variant &other) {
            if (this != &other) {

                type_ = other.type_;

                if (other.is_int()) {
                    int_value = other.int_value;
                } else if (other.is_float()) {
                    float_value = other.float_value;
                } else if (other.type_ == RuntimeType::Pointer) {
                    ptr_value = other.ptr_value;
                } else {
                    int_value = 0; /* null */
                }
            }
            return *this;
        }

        Variant(Variant &&other) noexcept : type_(other.type_) {
            if (other.is_int()) {
                int_value = other.int_value;
            } else if (other.is_float()) {
                float_value = other.float_value;
            } else if (other.type_ == RuntimeType::Pointer) {
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

                if (other.is_int()) {
                    int_value = other.int_value;
                } else if (other.is_float()) {
                    float_value = other.float_value;
                } else if (other.type_ == RuntimeType::Pointer) {
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
            type_ = RuntimeType::Int;
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
                type_ = RuntimeType::Pointer;
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
            if (type_ != RuntimeType::Int && type_ != RuntimeType::Int) {
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

        const void *get_ptr() const {
            if (!is_ptr()) { throw TypeError("get_ptr() expected 'pointer' but found", type_); }
            return ptr_value;
        }

        void *get_ptr() {
            if (!is_ptr()) { throw TypeError("get_ptr() expected 'pointer' but found", type_); }
            return ptr_value;
        }

        std::string get_string() const {
            if (type_ != RuntimeType::Pointer) { throw TypeError("Expected string", type_); }
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
            throw TypeError("to_int() cannot convert from", type_);
        }

        f32 to_float() const {
            if (type_ == RuntimeType::Int) return static_cast<float>(int_value);
            if (type_ == RuntimeType::Float) return float_value;
            throw TypeError("to_float() cannot convert from", type_);
        }

        bool to_bool() const {
            if (type_ == RuntimeType::Int) return int_value != 0;
            if (type_ == RuntimeType::Float) return float_value != 0;
            if (type_ == RuntimeType::Pointer) return ptr_value != nullptr;
            return false;
        }

        // =========================================================================
        // TYPE CHECKING
        // =========================================================================

        RuntimeType get_type() const { return type_; }

        inline bool is_null()   const { return type_ == RuntimeType::Null; }
        inline bool is_int() const { return type_ == RuntimeType::Int; }
        inline bool is_float()  const { return type_ == RuntimeType::Float; }
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
            case RuntimeType::Int:   return std::to_string(get_i64());
            case RuntimeType::Float: return fmt::format("{}", get_f64());
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

            return ptr_value == other.ptr_value;
        }

        bool operator!=(const Variant &other) const { return !(*this == other); }

        bool operator<(const Variant &other) const {
            if (type_ != other.type_) return type_ < other.type_;
            if (is_null()) return true;
   
            if (is_int()) return int_value < other.int_value;
            if (is_float()) return float_value < other.float_value;

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
// Helper functions used by the formatter.
// All live in namespace carbon so that the formatter (which lives in
// namespace fmt) can call them as carbon::foo.
// ============================================================================

namespace carbon {

    // Resolve a SID to a human-readable name, or fall back to hex.
    // sid == 0 means "null".
    inline std::string fmt_sid(sid64 sid) {
        if (sid == 0) { return "null"; }
        try {
            const char *name = StringIdManager::instance().get_cstring(sid);
            if (name && name[0] != '\0') { return std::string(name); }
        } catch (const std::exception &) {
            // fall through to hex
        }
        return fmt::format("sid:0x{:016X}", static_cast<u64>(sid));
    }

    // Escape a C string for one-line display: quotes, backslashes,
    // control characters. Stops at '\0' or after max_len bytes,
    // whichever comes first, so a stray pointer cannot dump
    // megabytes of random memory into the log.
    inline std::string escape_cstr(const char *s, size_t max_len = 256) {
        if (!s) { return ""; }
        std::string out;
        out.reserve(32);
        for (size_t i = 0; i < max_len && s[i] != '\0'; ++i) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += fmt::format("\\x{:02X}", c);
                } else {
                    out += static_cast<char>(c);
                }
                break;
            }
        }
        return out;
    }

    // Format an f64 as if it were a float: integral values get a
    // trailing ".0", non-integral values use the default fmt format
    // (shortest round-trip representation).
    //
    // Used by both the {:f} flag and the {:t=float} type name so that
    // the two paths agree.
    inline std::string fmt_f64_as_float(f64 value) {
        if (value == static_cast<f64>(static_cast<i64>(value))) {
            return fmt::format("{:.1f}", value);
        }
        return fmt::format("{}", value);
    }

    // Decode a Variant according to a declared type name.
    // Used by the formatter for the {:t=<name>} spec.
    inline std::string fmt_variant_typed(const Variant &v, std::string_view type_name) {
        // String-like types: read the pointer as a C string.
        if (type_name == "string" || type_name == "cstr") {
            if (v.get_type() == RuntimeType::Pointer) {
                const char *s = static_cast<const char *>(v.get_ptr());
                return s ? ("\"" + escape_cstr(s) + "\"") : "null";
            }
            // Fall through: non-pointer values print normally.
        }

        // Bool-like types: Int 0/1 -> true/false.
        if (type_name == "bool" || type_name == "boolean") {
            if (v.get_type() == RuntimeType::Int) { return v.get_i64() ? "true" : "false"; }
        }

        // SID-like types: Int -> name or sid:0x.
        if (type_name == "symbol" || type_name == "sid64") {
            if (v.get_type() == RuntimeType::Int) {
                return fmt_sid(static_cast<sid64>(v.get_i64()));
            }
        }

        // Float-like types: Int -> float with ".0" for integral values.
        if (type_name == "float" || type_name == "f32" || type_name == "f64") {
            if (v.get_type() == RuntimeType::Int) {
                return fmt_f64_as_float(static_cast<f64>(v.get_i64()));
            }
        }

        // Everything else: default rendering.
        // A Pointer whose type_name is not "string" is NOT read as a C
        // string; it is printed as an address.
        if (v.get_type() == RuntimeType::Pointer) {
            const void *raw = v.get_ptr();
            if (!raw) { return "null"; }
            return fmt::format("ptr:0x{:X}", reinterpret_cast<uintptr_t>(raw));
        }
        if (v.get_type() == RuntimeType::Int) { return std::to_string(v.get_i64()); }
        if (v.get_type() == RuntimeType::Float) { return fmt::format("{}", v.get_f64()); }
        return "null";
    }

} // namespace carbon

// ============================================================================
// fmt::formatter<carbon::Variant>
// ============================================================================
//
// Formats a carbon::Variant for logging, REPL output, and debugging.
//
// A Variant holds one of four runtime types:
//
//     Null      — no value
//     Int       — a signed 64-bit integer. Also used to carry bools,
//                 SIDs, and small numeric types; the semantic meaning
//                 is supplied by the format spec, not by the Variant.
//     Float     — a 64-bit floating-point value
//     Pointer   — a raw void*. May point to a C string, a structure,
//                 a function, or an array; without type information
//                 the formatter cannot tell which.
//
// Because Int and Pointer carry no semantic tag, the formatter
// defaults are deliberately conservative:
//
//     Int     -> decimal number
//     Float   -> decimal number
//     Pointer -> C string, printed WITHOUT quotes (see below)
//     Null    -> "null"
//
// The caller can override the default with a format spec after ':'.
// The spec is a small, custom mini-language (NOT standard fmt
// precision/width — Variant is not a number):
//
//     {}              default rendering (see table below)
//     {:s}            string mode: Pointer is read as a C string and
//                     printed WITH quotes (e.g. `"hello"`).
//                     Int is printed as a number (an Int cannot be a
//                     string without type information).
//     {:p}            pointer mode: Pointer is printed as an address
//                     (`ptr:0x...`) and is never read as a string.
//                     nullptr prints as `null`.
//     {:b}            bool mode: Int 0/1 is printed as `false`/`true`.
//                     Any non-zero Int prints as `true`.
//     {:sid}          SID mode: Int is resolved through
//                     StringIdManager and printed as the registered
//                     name, or as `sid:0x...` if the name is unknown.
//                     Zero prints as `null`.
//     {:f}            float mode: Int is printed as a floating-point
//                     number with a trailing `.0` for integral values
//                     (`42` -> `42.0`). Float is printed as-is.
//     {:x}            hex mode: Int is printed as `0x...`.
//     {:t=<name>}     type-name mode: decode the Variant according to
//                     <name>. Supported names:
//
//                         "string" / "cstr"    Pointer -> "..." (quoted)
//                         "bool" / "boolean"   Int     -> true/false
//                         "symbol" / "sid64"   Int     -> SID name
//                         "float" / "f32" / "f64"
//                                              Int     -> 42.0
//                         anything else        default rendering, but
//                                              a Pointer is printed
//                                              as `ptr:0x...`, NEVER
//                                              as a C string.
//
// The `{:t=<name>}` form is the correct way to print a value whose
// declared type is known at the call site (a StructFieldInfo::type_name,
// a function's return type, a REPL command's expected type, ...).
// It is what BinaryFileInspector uses to render data-struct fields.
//
// DEFAULT MODE AND QUOTES
// -----------------------
// The default `{}` prints a Pointer as a C string WITHOUT quotes.
// This matches the historical behaviour of `(print "{}" x)` and
// keeps existing scripts working:
//
//     (print "{}" "hello")      ->  hello
//
// The explicit string mode `{:s}` prints the same value WITH quotes,
// which is appropriate for debug output where the string boundary
// matters:
//
//     (print "{:s}" "hello")    ->  "hello"
//
// EXAMPLES
// --------
//
//     Variant n;                          fmt::format("{}",     n) -> "null"
//     Variant i(42);                      fmt::format("{}",     i) -> "42"
//     Variant i(42);                      fmt::format("{:x}",   i) -> "0x2A"
//     Variant i(1);                       fmt::format("{:b}",   i) -> "true"
//     Variant i(42);                      fmt::format("{:f}",   i) -> "42.0"
//     Variant i(42);                      fmt::format("{:t=float}", i) -> "42.0"
//     Variant sid(SID("foo"));            fmt::format("{:sid}", sid) -> "foo"
//     Variant sid(0);                     fmt::format("{:sid}", sid) -> "null"
//     Variant p(std::string("hello"));    fmt::format("{}",     p) -> "hello"
//     Variant p(std::string("hello"));    fmt::format("{:s}",   p) -> "\"hello\""
//     Variant p(std::string("hello"));    fmt::format("{:p}",   p) -> "ptr:0x..."
//     Variant p(std::string("hello"));    fmt::format("{:t=string}", p) -> "\"hello\""
//     Variant p(std::string("hello"));    fmt::format("{:t=vector}", p) -> "ptr:0x..."
//     Variant null_ptr((void*)nullptr, RuntimeType::Pointer);
//     fmt::format("{}", null_ptr) -> "null"
//
// SAFETY
// ------
// `Pointer` is read as a C string ONLY in default mode, `{:s}`,
// and `{:t=string}` / `{:t=cstr}`. In every other mode (`{:p}`,
// `{:t=vector}`, `{:t=<unknown>}`, ...) a Pointer is printed as an
// address and is NEVER dereferenced. This is deliberate: without a
// declared type name, a Pointer may point to a structure or a
// function, and reading it as a string would be undefined behaviour.
//
// In default mode the string is read with a 256-byte cap
// (escape_cstr) so that a stray non-string Pointer cannot dump
// megabytes of random memory into the log.
//
// LIMITATIONS
// -----------
// - Standard fmt precision, width, and alignment ({:.2f}, {:>10})
//   are NOT supported: Variant is not a number, and the spec grammar
//   is reserved for the modes above.
// - Int cannot be read as a C string; use `{:t=symbol}` or `{:sid}`
//   to resolve an Int as a SID.
// - Float is always printed with the default fmt format (shortest
//   round-trip representation). Use `{:.2f}` on a `double` value
//   directly if you need fixed-point output.
// - A SID that is not registered in StringIdManager prints as
//   `sid:0x...`, not as a name. Register the string via
//   StringIdManager::register_string if you need it to resolve.
//
// THREAD SAFETY
// -------------
// The formatter itself is stateless between calls (each format()
// call uses only its own parsed mode and type name). StringIdManager
// is shared, but fmt does not add locking; if the application interns
// strings concurrently with formatting, that is the application's
// concern.
//
// ============================================================================
namespace fmt {

    template <> struct formatter<carbon::Variant> {
        // One of: Default, String, Pointer, Bool, Sid, Float, Hex.
        // Parsed from the format spec after ':'.
        enum class Mode { Default, String, Pointer, Bool, Sid, Float, Hex };
        Mode m_mode = Mode::Default;

        // {:t=<name>} — explicit type name. When non-empty, overrides
        // m_mode: the formatter decodes the Variant according to <name>
        // ("string", "bool", "symbol", "sid64", "float", "pointer").
        std::string m_type_name;

        constexpr auto parse(format_parse_context &ctx) {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it == end || *it == '}') { return it; }

            // Three-letter flag must be checked before the one-letter 's'.
            if (*it == 's' && it + 2 < end && *(it + 1) == 'i' && *(it + 2) == 'd') {
                m_mode = Mode::Sid;
                it += 3;
                return it;
            }

            // {:t=<name>} — explicit type name. Consume everything up to '}'.
            if (*it == 't' && it + 1 != end && *(it + 1) == '=') {
                it += 2;
                auto name_begin = it;
                while (it != end && *it != '}') { ++it; }
                m_type_name.assign(name_begin, it);
                m_mode = Mode::Default; // resolved by m_type_name
                return it;
            }

            // Single-letter flags.
            if (*it == 's') {
                m_mode = Mode::String;
                ++it;
            } else if (*it == 'p') {
                m_mode = Mode::Pointer;
                ++it;
            } else if (*it == 'b') {
                m_mode = Mode::Bool;
                ++it;
            } else if (*it == 'f') {
                m_mode = Mode::Float;
                ++it;
            } else if (*it == 'x') {
                m_mode = Mode::Hex;
                ++it;
            }

            return it;
        }

        template <typename FormatContext>
        auto format(const carbon::Variant &v, FormatContext &ctx) const {
            // Explicit type name wins over the mode flag.
            if (!m_type_name.empty()) {
                return fmt::format_to(ctx.out(), "{}", carbon::fmt_variant_typed(v, m_type_name));
            }

            // RuntimeType is a free enum in namespace carbon.
            using carbon::RuntimeType;
            switch (v.get_type()) {
            case RuntimeType::Null: return fmt::format_to(ctx.out(), "null");

            case RuntimeType::Int: return format_int(v.get_i64(), ctx);

            case RuntimeType::Float: return fmt::format_to(ctx.out(), "{}", v.get_f64());

            case RuntimeType::Pointer: return format_ptr(v.get_ptr(), ctx);
            }
            return fmt::format_to(ctx.out(), "<unknown>");
        }

    private:
        template <typename FormatContext> auto format_int(i64 value, FormatContext &ctx) const {
            switch (m_mode) {
            case Mode::Bool: return fmt::format_to(ctx.out(), "{}", value ? "true" : "false");
            case Mode::Sid:
                return fmt::format_to(ctx.out(), "{}", carbon::fmt_sid(static_cast<sid64>(value)));
            case Mode::Float:
                return fmt::format_to(ctx.out(), "{}",
                                      carbon::fmt_f64_as_float(static_cast<f64>(value)));
            case Mode::Hex: return fmt::format_to(ctx.out(), "0x{:X}", value);
            case Mode::Default:
            case Mode::String:
            case Mode::Pointer:
            default: return fmt::format_to(ctx.out(), "{}", value);
            }
        }

        template <typename FormatContext>
        auto format_ptr(const void *raw, FormatContext &ctx) const {
            if (raw == nullptr) { return fmt::format_to(ctx.out(), "null"); }
            switch (m_mode) {
            case Mode::Pointer:
                // Explicit {:p}: print as an address, never read as a string.
                return fmt::format_to(ctx.out(), "ptr:0x{:X}", reinterpret_cast<uintptr_t>(raw));
            case Mode::String:
                // Explicit {:s}: quote the string, since the caller asked
                // for a string rendering.
                return fmt::format_to(ctx.out(), "\"{}\"",
                                      carbon::escape_cstr(static_cast<const char *>(raw)));
            case Mode::Default:
            default: {
                // Default {}: print the pointer as a C string, without
                // quotes. This matches the historical behaviour of print
                // and keeps old scripts working.
                const char *s = static_cast<const char *>(raw);
                return fmt::format_to(ctx.out(), "{}", carbon::escape_cstr(s));
            }
            }
        }
    };

} // namespace fmt