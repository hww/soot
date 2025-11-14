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
// Портируемые типы
using FloatType = double;
using IntType = int64_t;

enum class ObjectType : uint8_t {
    EMPTY_LIST, INTEGER, FLOAT, CHAR, BOOLEAN,
    SYMBOL, STRING, PAIR, ARRAY, LAMBDA, MACRO, ENVIRONMENT, INVALID, STRING_HASH_TABLE
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
class MacroObject;
class LambdaObject;
class PairObject;
class HashTableObject;
class FilePortObject;
struct ArgumentSpec;

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
    static Object make_lambda(const ArgumentSpec& args, const Object& body, const std::shared_ptr<EnvironmentObject>& env);
    static Object make_macro(const ArgumentSpec& args,const Object& body,const std::shared_ptr<EnvironmentObject>& env);
    static Object make_vector(const std::vector<Object>& elements);
    static Object make_hash_table();


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
    bool is_lambda() const { return type == ObjectType::LAMBDA; }
    bool is_macro() const { return type == ObjectType::MACRO; }
    bool is_vector() const { return type == ObjectType::ARRAY; } 
    bool is_hash_table() const { return type == ObjectType::STRING_HASH_TABLE; }

    // Value access with type checking
    IntType as_integer() const;
    FloatType as_float() const;
    char as_char() const;
    bool as_boolean() const;
    std::string as_symbol() const;
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
    std::shared_ptr<EnvironmentObject> parent_env;
    std::unordered_map<std::string, Object> vars;  // Храним по СТРОКАМ, а не указателям

    EnvironmentObject() = default;
    explicit EnvironmentObject(std::shared_ptr<EnvironmentObject> parent)
        : parent_env(std::move(parent)) {
    }

    std::string print() const override { return "[environment]"; }
    std::string inspect() const override { return "[environment]"; }

    // Метод как в OpenGOAL - ищем переменную по имени
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

    // Простой set - всегда устанавливает в текущее окружение
    void set(const std::string& name, const Object& value) {
        vars[name] = value;
    }

    // try_get для lookup без исключений
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

    // get с исключением для обратной совместимости
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

    std::string Arguments::print() const;
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

    // Основные методы доступа
    size_t size() const {
        return unnamed.size() + named.size();
    }

    size_t unnamed_size() const {
        return unnamed.size();
    }

    size_t named_size() const {
        return named.size();
    }

    bool empty() const {
        return unnamed.empty() && named.empty();
    }

    // Доступ по индексу (только для unnamed параметров)
    const std::string& operator[](size_t index) const {
        if (index >= unnamed.size()) {
            throw std::out_of_range("ArgumentSpec index out of range");
        }
        return unnamed[index];
    }

    // Проверка наличия named параметра
    bool has_named(const std::string& name) const {
        return named.find(name) != named.end();
    }

    // Получение named параметра
    const NamedArg& get_named(const std::string& name) const {
        auto it = named.find(name);
        if (it == named.end()) {
            throw std::out_of_range("Named argument not found: " + name);
        }
        return it->second;
    }

    std::string print() const;
    static ArgumentSpec make_varargs();
};

// Добавь полное определение LambdaObject в object.h
class LambdaObject : public HeapObject {
public:
    std::string name;
    std::shared_ptr<EnvironmentObject> parent_env;
    Object body;
    ArgumentSpec args;  // ← Используй ArgumentSpec вместо std::vector<std::string>

    LambdaObject() = default;

    static Object make_new() {
        Object obj;
        obj.type = ObjectType::LAMBDA;
        obj.heap_obj = std::make_shared<LambdaObject>();
        return obj;
    }

    std::string print() const override {
        if (name.empty()) {
            return "<unnamed lambda>";
        }
        else {
            return "<lambda \"" + name + "\">";
        }
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
    ArgumentSpec args;  // ← Используй ArgumentSpec вместо std::vector<std::string>

    MacroObject() = default;

    static Object make_new() {
        Object obj;
        obj.type = ObjectType::MACRO;
        obj.heap_obj = std::make_shared<MacroObject>();
        return obj;
    }

    std::string print() const override {
        if (name.empty()) {
            return "<unnamed macro>";
        }
        else {
            return "<macro \"" + name + "\">";
        }
    }

    std::string inspect() const override {
        return "[macro]\n  name: " + name + "\n" + args.print();
    }
};

class VectorObject : public HeapObject {
public:
    std::vector<Object> elements;
    VectorObject(const std::vector<Object>& elems) : elements(elems) {}

    std::string print() const override {
        std::stringstream ss;
        ss << "#(";
        for (size_t i = 0; i < elements.size(); ++i) {
            ss << elements[i].print();
            if (i < elements.size() - 1) ss << " ";
        }
        ss << ")";
        return ss.str();
    }

    std::string inspect() const override {
        return "[vector size=" + std::to_string(elements.size()) + "]";
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

    Object intern(const std::string& name);

private:
    void resize();
    uint32_t compute_hash(const char* data, size_t length) const;
};