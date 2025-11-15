#pragma once

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
struct ArgumentSpec;
class SymbolTable;

// InternedSymbolPtr как в OpenGOAL
struct InternedSymbolPtr {
    const char* name_ptr;

    struct hash {
        auto operator()(const InternedSymbolPtr& x) const {
            return std::hash<const void*>()((const void*)x.name_ptr);
        }
    };

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

    // For pair access
    Object car() const;
    Object cdr() const;
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

    std::string print() const override {
        return "\"" + text + "\"";
    }

    std::string inspect() const override {
        return "[string] \"" + text + "\"";
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

class EnvironmentObject : public HeapObject {
public:
    std::shared_ptr<EnvironmentObject> parent_env;
    std::unordered_map<std::string, Object> vars;

    EnvironmentObject() = default;
    explicit EnvironmentObject(std::shared_ptr<EnvironmentObject> parent)
        : parent_env(std::move(parent)) {
    }

    std::string print() const override { return "[environment]"; }
    std::string inspect() const override { return "[environment]"; }

    Object* find(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            return &it->second;
        }
        if (parent_env) {
            return parent_env->find(name);
        }
        return nullptr;
    }

    void set(const std::string& name, const Object& value) {
        vars[name] = value;
    }

    bool try_get(const std::string& name, Object* dest) const {
        auto it = vars.find(name);
        if (it != vars.end()) {
            *dest = it->second;
            return true;
        }
        if (parent_env) {
            return parent_env->try_get(name, dest);
        }
        return false;
    }

    Object get(const std::string& name) const {
        Object result;
        if (try_get(name, &result)) {
            return result;
        }
        throw std::runtime_error("Undefined symbol: " + name);
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

class SymbolTable {
private:
    static constexpr double kMaxUsed = 0.75;
    uint32_t m_power_of_two_size = 1;
    uint32_t m_used_entries = 0;
    uint32_t m_next_resize = 0;
    uint32_t m_mask = 0b1;

    struct Entry {
        char* name = nullptr;
        uint32_t hash = 0;
    };

    std::vector<Entry> m_entries;

public:
    SymbolTable();
    ~SymbolTable();

    InternedSymbolPtr intern(const std::string& name);

private:
    void resize();
    uint32_t compute_hash(const char* data, size_t length) const;
};