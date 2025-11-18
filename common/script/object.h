#pragma once

#include "crc32.h"
#include "util/assert.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <type_traits>
#include <cstring>
#include <unordered_set>

namespace script
{
    // Портируемые типы
    using FloatType = double;
    using IntType = int64_t;

    enum class ObjectType : uint8_t {
        EMPTY_LIST, INTEGER, FLOAT, CHAR,
        SYMBOL, STRING, PAIR, ARRAY, LAMBDA, MACRO, ENVIRONMENT, INVALID, STRING_HASH_TABLE
    };

    std::string object_type_to_string(ObjectType type);

    // Forward declarations
    class EnvironmentObject;
    class MacroObject;
    class LambdaObject;
    class PairObject;
    class HashTableObject;
    class FilePortObject;
    class SymbolTable;
    class StringObject;
    struct ArgumentSpec;

    // InternedSymbolPtr как в OpenGOAL
    struct InternedSymbolPtr {
        const char* name_ptr;

        struct hash {
            auto operator()(const InternedSymbolPtr& x) const {
                return std::hash<const void*>()((const void*)x.name_ptr);
            }
        };

        bool starts_with_colon() const {
            return name_ptr && name_ptr[0] != '\0' && name_ptr[0] == ':';
        }

        const char* c_str() const { return name_ptr; }

        bool operator==(const char* msg) const { return strcmp(msg, name_ptr) == 0; }
        bool operator!=(const char* msg) const { return strcmp(msg, name_ptr) != 0; }
        bool operator==(const std::string& str) const { return str == name_ptr; }
        bool operator!=(const std::string& str) const { return str != name_ptr; }
        bool operator==(const InternedSymbolPtr& other) const { return other.name_ptr == name_ptr; }
        bool operator!=(const InternedSymbolPtr& other) const { return other.name_ptr != name_ptr; }
    };

    // FixedObject шаблон как в OpenGOAL
    template <typename T>
    std::string fixed_to_string(T x);

    template <>
    std::string fixed_to_string<FloatType>(FloatType x);

    template <>
    std::string fixed_to_string<char>(char x);

    template <>
    std::string fixed_to_string<IntType>(IntType x);

    template <>
    std::string fixed_to_string<InternedSymbolPtr>(InternedSymbolPtr x);

    template <typename T>
    class FixedObject {
    public:
        T value;

        explicit FixedObject(T v) : value(v) {}
        FixedObject() = default;

        std::string print() const { return fixed_to_string(value); }
        std::string inspect() const {
            return type_as_string() + " " + fixed_to_string(value);
        }

        bool operator==(const FixedObject<T>& other) const {
            return value == other.value;
        }

    private:
        std::string type_as_string() const {
            if constexpr (std::is_same_v<T, FloatType>)
                return object_type_to_string(ObjectType::FLOAT);
            if constexpr (std::is_same_v<T, IntType>)
                return object_type_to_string(ObjectType::INTEGER);
            if constexpr (std::is_same_v<T, char>)
                return object_type_to_string(ObjectType::CHAR);
            if constexpr (std::is_same_v<T, InternedSymbolPtr>)
                return object_type_to_string(ObjectType::SYMBOL);
            throw std::runtime_error("Unsupported FixedObject type");
        }
    };

    // Fixed object types
    using IntegerObject = FixedObject<IntType>;
    using FloatObject = FixedObject<FloatType>;
    using CharObject = FixedObject<char>;
    using SymbolObject = FixedObject<InternedSymbolPtr>;

    // Базовый класс для heap-allocated объектов
    class HeapObject {
    public:
        virtual ~HeapObject() = default;
        virtual std::string print() const = 0;
        virtual std::string inspect() const = 0;
    };

    // Main Object class
    class Object {
    public:
        ObjectType type = ObjectType::INVALID;

        // For fixed types (value semantics) - как в OpenGOAL
        union {
            IntegerObject integer_obj;
            FloatObject float_obj;
            CharObject char_obj;
            SymbolObject symbol_obj;
        };

        // For heap types (reference semantics)
        std::shared_ptr<HeapObject> heap_obj;

        // Constructors for fixed types
        static Object make_integer(IntType value);
        static Object make_float(FloatType value);
        static Object make_char(char value);
        static Object make_empty_list();
        static Object make_symbol(SymbolTable* table, const char* name);
        static Object make_string(const std::string& text);
        static Object make_pair(const Object& car, const Object& cdr);
        static Object make_array(const std::vector<Object>& elements);
        static Object make_lambda(const ArgumentSpec& args, const Object& body, const std::shared_ptr<EnvironmentObject>& env);
        static Object make_macro(const ArgumentSpec& args, const Object& body, const std::shared_ptr<EnvironmentObject>& env);
        static Object make_vector(const std::vector<Object>& elements);
        static Object make_hash_table();


        // String representation
        std::string print() const;
        std::string inspect() const;

        // Type checking
        bool is_integer() const { return type == ObjectType::INTEGER; }
        bool is_float() const { return type == ObjectType::FLOAT; }
        bool is_char() const { return type == ObjectType::CHAR; }
        bool is_symbol() const { return type == ObjectType::SYMBOL; }
        bool is_string() const { return type == ObjectType::STRING; }
        bool is_pair() const { return type == ObjectType::PAIR; }
        bool is_array() const { return type == ObjectType::ARRAY; }
        bool is_empty_list() const { return type == ObjectType::EMPTY_LIST; }
        bool is_list() const { return is_empty_list() || is_pair(); }
        bool is_lambda() const { return type == ObjectType::LAMBDA; }
        bool is_macro() const { return type == ObjectType::MACRO; }
        bool is_vector() const { return type == ObjectType::ARRAY; }
        bool is_hash_table() const { return type == ObjectType::STRING_HASH_TABLE; }
        bool is_symbol(const std::string& name) const { return is_symbol() && as_symbol() == name; }
        bool is_boolean() const { return is_symbol() && (as_symbol() == "#t" || as_symbol() == "#f"); }
        bool is_true() const { return is_symbol() && (as_symbol() != "#f"); }
        bool is_false() const { return is_symbol() && (as_symbol() == "#f"); }

        // Value access with type checking
        IntType as_integer() const;
        FloatType as_float() const;
        char as_char() const;
        const InternedSymbolPtr& as_symbol() const;
        std::string as_string() const;
        std::vector<Object> as_vector() const;
        HashTableObject* as_hash_table() const;
        MacroObject* as_macro() const;
        LambdaObject* as_lambda() const;
        EnvironmentObject* as_env() const;
        std::shared_ptr<EnvironmentObject> as_env_ptr() const;
        bool as_boolean() const { return !is_symbol() || as_symbol() != "#f"; }
        const IntegerObject& as_integer_obj() const {
            if (!is_integer()) throw_type_error("integer");
            return integer_obj;
        }

        // C++ идеоматичные методы
        std::vector<Object> as_c_vector() const;

        // For pair access
        PairObject* as_pair() const;

        bool operator==(const Object& other) const;
        bool operator!=(const Object& other) const { return !(*this == other); }

    private:
        void throw_type_error(const std::string& expected) const;
    };

    // Now define PairObject AFTER Object
    class PairObject : public HeapObject {
    public:
        Object car;
        Object cdr;

        PairObject(const Object& car, const Object& cdr) : car(car), cdr(cdr) {}

        std::string print() const override;
        std::string inspect() const override;
    };
    class StringObject : public HeapObject {
    public:
        std::string text;
        explicit StringObject(std::string text) : text(std::move(text)) {}

        int length() const { return text.length(); }

        char at(const int index) {
            return text[index];
        }

        std::string print() const override {
            return "\"" + text + "\"";
        }

        std::string inspect() const override {
            return "[string] \"" + text + "\"";
        }

        // Неявное преобразование в std::string
        operator std::string() const {
            return text;
        }

        // Дополнительно: можно добавить преобразование в const char*
        const char* c_str() const {
            return text.c_str();
        }
    };

    class ArrayObject : public HeapObject {
    public:
        std::vector<Object> elements;
        explicit ArrayObject(std::vector<Object> elements) : elements(std::move(elements)) {}

        std::string print() const override {
            std::string result = "#(";
            for (size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) result += " ";
                result += elements[i].print();
            }
            result += ")";
            return result;
        }

        std::string inspect() const override {
            return "[array] size=" + std::to_string(elements.size());
        }
    };

    template <typename T>
    class InternedPtrMap {
    public:

        InternedPtrMap(const InternedPtrMap&) = delete;
        InternedPtrMap& operator=(const InternedPtrMap&) = delete;
        InternedPtrMap() { clear(); }

        T* lookup(InternedSymbolPtr str) {
            if (m_entries.size() < 10) {
                for (auto& e : m_entries) {
                    if (e.key == str.name_ptr) {  // ← Сравниваем указатели!
                        return &e.value;
                    }
                }
                return nullptr;
            }
            uint32_t hash = compute_crc32(str.name_ptr, sizeof(const char*));  // ← Используем name_ptr

            // probe
            for (uint32_t i = 0; i < m_entries.size(); i++) {
                uint32_t slot_addr = (hash + i) & m_mask;
                auto& slot = m_entries[slot_addr];
                if (!slot.key) {
                    return nullptr;
                }
                else {
                    if (slot.key != str.name_ptr) {  // ← Сравниваем указатели!
                        continue;
                    }
                    return &slot.value;
                }
            }
            ASSERT_NOT_REACHED();
        }

        void set(InternedSymbolPtr ptr, const T& obj) {
            uint32_t hash = compute_crc32(ptr.name_ptr, sizeof(const char*));  // ← Используем name_ptr

            // probe
            for (uint32_t i = 0; i < m_entries.size(); i++) {
                uint32_t slot_addr = (hash + i) & m_mask;
                auto& slot = m_entries[slot_addr];
                if (!slot.key) {
                    // not found, insert!
                    slot.key = ptr.name_ptr;  // ← Сохраняем указатель!
                    slot.value = obj;
                    m_used_entries++;
                    if (m_used_entries >= m_next_resize) {
                        resize();
                    }
                    return;
                }
                else {
                    if (slot.key == ptr.name_ptr) {  // ← Сравниваем указатели!
                        slot.value = obj;
                        return;
                    }
                }
            }
            ASSERT_NOT_REACHED();
        }

        void clear() {
            m_entries.clear();
            m_power_of_two_size = 3;  // 2 ^ 3 = 8
            m_entries.resize(8);
            m_used_entries = 0;
            m_next_resize = (m_entries.size() * kMaxUsed);
            m_mask = 0b111;
        }

    private:
        struct Entry {
            const char* key = nullptr;
            T value;
        };
        std::vector<Entry> m_entries;

        void resize() {
            m_power_of_two_size++;
            m_mask = (1U << m_power_of_two_size) - 1;

            std::vector<Entry> new_entries(m_entries.size() * 2);
            for (const auto& old_entry : m_entries) {
                if (old_entry.key) {
                    bool done = false;
                    uint32_t hash = compute_crc32(old_entry.key, sizeof(const char*));
                    for (uint32_t i = 0; i < new_entries.size(); i++) {
                        uint32_t slot_addr = (hash + i) & m_mask;
                        auto& slot = new_entries[slot_addr];
                        if (!slot.key) {
                            slot.key = old_entry.key;
                            slot.value = std::move(old_entry.value);
                            done = true;
                            break;
                        }
                    }
                    ASSERT(done);
                }
            }

            m_entries = std::move(new_entries);
            m_next_resize = kMaxUsed * m_entries.size();
        }
        int m_power_of_two_size = 0;
        int m_used_entries = 0;
        int m_next_resize = 0;
        uint32_t m_mask = 0;
        static constexpr float kMaxUsed = 0.7;
    };

    class SymbolTable {
    public:
        SymbolTable(const SymbolTable&) = delete;
        SymbolTable& operator=(const SymbolTable&) = delete;
        SymbolTable();
        ~SymbolTable();

        InternedSymbolPtr intern(const char* str);

    private:
        void resize();
        int m_power_of_two_size = 0;
        struct Entry {
            uint32_t hash = 0;
            const char* name = nullptr;
        };
        std::vector<Entry> m_entries;
        int m_used_entries = 0;
        int m_next_resize = 0;
        uint32_t m_mask = 0;
        static constexpr float kMaxUsed = 0.7;
    };

    using EnvironmentMap = InternedPtrMap<Object>;

    class EnvironmentObject : public HeapObject
    {
    public:

        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        EnvironmentMap vars;

        EnvironmentObject() = default;
        EnvironmentObject(std::shared_ptr<EnvironmentObject> parent)
            : parent_env(std::move(parent)) {
        }

        Object* find(const char* n, SymbolTable* st) {
            return vars.lookup(st->intern(n));
        }

        Object* find(InternedSymbolPtr ptr) {
            return vars.lookup(ptr);
        }

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::ENVIRONMENT;
            obj.heap_obj = std::make_shared<EnvironmentObject>();
            return obj;
        }

        static Object make_new(std::string name,
            std::shared_ptr<EnvironmentObject> parent_env = nullptr) {
            Object obj;
            obj.type = ObjectType::ENVIRONMENT;
            auto env = std::make_shared<EnvironmentObject>();
            env->name = std::move(name);
            env->parent_env = std::move(parent_env);
            obj.heap_obj = std::move(env);
            return obj;
        }

        std::string print() const override {
            if (name.empty()) {
                return "<unnamed environment>";
            }
            else {
                return "<environment \"" + name + "\">";
            }
        }

        std::string inspect() const override {
            std::string result = "[environment]\n  name: " + name +
                "\n  parent: " + (parent_env ? parent_env->print() : "NONE") + "\n";
            return result;
        }
    };

    // Аргументы функций
    struct Arguments {
        std::vector<Object> unnamed;
        std::map<std::string, Object> named;
        std::vector<Object> rest;
        bool has_rest = false;

        Object get_named(const std::string& name, const Object& default_value) {
            auto it = named.find(name);
            return it != named.end() ? it->second : default_value;
        }

        Object get_named(const std::string& name) {
            return named.at(name);
        }

        bool has_named(const std::string& name) {
            return named.find(name) != named.end();
        }

        std::string print() const;
    };

    struct NamedArg {
        bool has_default = false;
        Object default_value;
    };

    struct ArgumentSpec {
        bool varargs = false;
        std::vector<std::string> unnamed;
        std::unordered_map<std::string, NamedArg> named;
        std::string rest;

        size_t size() const { return unnamed.size() + named.size(); }
        size_t unnamed_size() const { return unnamed.size(); }
        size_t named_size() const { return named.size(); }
        bool empty() const { return unnamed.empty() && named.empty(); }

        const std::string& operator[](size_t index) const {
            if (index >= unnamed.size()) throw std::out_of_range("ArgumentSpec index out of range");
            return unnamed[index];
        }

        bool has_named(const std::string& name) const {
            return named.find(name) != named.end();
        }

        const NamedArg& get_named(const std::string& name) const {
            auto it = named.find(name);
            if (it == named.end()) throw std::out_of_range("Named argument not found: " + name);
            return it->second;
        }

        std::string print() const;
        static ArgumentSpec make_varargs();
    };

    class LambdaObject : public HeapObject {
    public:
        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        Object body;
        ArgumentSpec args;

        LambdaObject() = default;

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::LAMBDA;
            obj.heap_obj = std::make_shared<LambdaObject>();
            return obj;
        }

        std::string print() const override {
            return name.empty() ? "<unnamed lambda>" : "<lambda \"" + name + "\">";
        }

        std::string inspect() const override {
            return "[lambda]\n  name: " + name + "\n" + args.print();
        }
    };

    class MacroObject : public HeapObject {
    public:
        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        Object body;
        ArgumentSpec args;

        MacroObject() = default;

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::MACRO;
            obj.heap_obj = std::make_shared<MacroObject>();
            return obj;
        }

        std::string print() const override {
            return name.empty() ? "<unnamed macro>" : "<macro \"" + name + "\">";
        }

        std::string inspect() const override {
            return "[macro]\n  name: " + name + "\n" + args.print();
        }
    };

    class HashTableObject : public HeapObject {
    public:
        std::unordered_map<std::string, Object> data;

        HashTableObject() = default;

        std::string print() const override {
            return "#<hash-table>";
        }

        std::string inspect() const override {
            return "[hash-table size=" + std::to_string(data.size()) + "]";
        }
    };

} // namespace script