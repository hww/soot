#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <cstdint>

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

class SymbolObject : public HeapObject {
public:
    std::string name;
    explicit SymbolObject(std::string name) : name(std::move(name)) {}  // ЭТО ДОБАВИТЬ
    std::string print() const override { return name; }
    std::string inspect() const override { return "[symbol] " + name; }
};

// Forward declarations
class EnvironmentObject;
class LambdaObject;
class MacroObject;

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

    // For pair access
    Object car() const;
    Object cdr() const;

    bool operator==(const Object& other) const;
    bool operator!=(const Object& other) const { return !(*this == other); }

private:
    void throw_type_error(const std::string& expected) const;
};

// Symbol table for interning
class SymbolTable {
public:
    Object intern(const std::string& name);

private:
    std::unordered_map<std::string, std::shared_ptr<HeapObject>> table;
};