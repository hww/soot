#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <map>
#include <unordered_map>

// Портируемые типы
using FloatType = double;
using IntType = int64_t;

enum class ObjectType : uint8_t {
    EMPTY_LIST, INTEGER, FLOAT, CHAR, BOOLEAN,
    SYMBOL, STRING, PAIR, ARRAY, LAMBDA, MACRO, ENVIRONMENT, INVALID
};

std::string object_type_to_string(ObjectType type);

// Базовый класс для heap-allocated объектов
class HeapObject {
public:
    virtual ~HeapObject() = default;
    virtual std::string print() const = 0;
    virtual std::string inspect() const = 0;
};

// Forward declarations
class EnvironmentObject;
class LambdaObject;
class MacroObject;
class PairObject;

// Main Object class
class Object {
public:
    ObjectType type = ObjectType::INVALID;
    
    // For fixed types (value semantics)
    union {
        IntType integer_value;
        FloatType float_value;
        char char_value;
        bool boolean_value;
    };
    
    // For heap types (reference semantics)
    std::shared_ptr<HeapObject> heap_obj;

    // Constructors for fixed types
    static Object make_integer(IntType value);
    static Object make_float(FloatType value);
    static Object make_char(char value);
    static Object make_boolean(bool value);
    static Object make_empty_list();
    static Object make_symbol(const std::string& name);
    static Object make_string(const std::string& text);
    static Object make_pair(const Object& car, const Object& cdr);
    static Object make_array(const std::vector<Object>& elements);

    // String representation
    std::string print() const;
    std::string inspect() const;

    // Type checking
    bool is_integer() const { return type == ObjectType::INTEGER; }
    bool is_float() const { return type == ObjectType::FLOAT; }
    bool is_char() const { return type == ObjectType::CHAR; }
    bool is_boolean() const { return type == ObjectType::BOOLEAN; }
    bool is_symbol() const { return type == ObjectType::SYMBOL; }
    bool is_string() const { return type == ObjectType::STRING; }
    bool is_pair() const { return type == ObjectType::PAIR; }
    bool is_array() const { return type == ObjectType::ARRAY; }
    bool is_empty_list() const { return type == ObjectType::EMPTY_LIST; }
    bool is_list() const { return is_empty_list() || is_pair(); }

    // Value access with type checking
    IntType as_integer() const;
    FloatType as_float() const;
    char as_char() const;
    bool as_boolean() const;
    std::string as_symbol() const;
    std::string as_string() const;

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

class SymbolObject : public HeapObject {
public:
    std::string name;
    explicit SymbolObject(std::string name) : name(std::move(name)) {}
    std::string print() const override { return name; }
    std::string inspect() const override { return "[symbol] " + name; }
};

class StringObject : public HeapObject {
public:
    std::string text;
    explicit StringObject(std::string text) : text(std::move(text)) {}
    std::string print() const override { return "\"" + text + "\""; }
    std::string inspect() const override { return "[string] \"" + text + "\""; }
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

// Add EnvironmentObject definition to fix the incomplete type error
class EnvironmentObject : public HeapObject {
public:
    std::shared_ptr<EnvironmentObject> parent;
    std::unordered_map<std::string, Object> bindings;
    
    EnvironmentObject() = default;
    explicit EnvironmentObject(std::shared_ptr<EnvironmentObject> parent) : parent(std::move(parent)) {}
    
    std::string print() const override { return "[environment]"; }
    std::string inspect() const override { return "[environment]"; }
    
    Object get(const std::string& name) const {
        auto it = bindings.find(name);
        if (it != bindings.end()) {
            return it->second;
        }
        if (parent) {
            return parent->get(name);
        }
        throw std::runtime_error("Undefined symbol: " + name);
    }
    
    void set(const std::string& name, const Object& value) {
        bindings[name] = value;
    }
};

// Symbol table for interning
class SymbolTable {
public:
    Object intern(const std::string& name);

private:
    std::unordered_map<std::string, std::shared_ptr<SymbolObject>> table;
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
};