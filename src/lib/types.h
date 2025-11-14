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

// Forward declarations
class Object;
class HeapObject;
class EnvironmentObject;
class LambdaObject;
class MacroObject;
class SymbolObject;

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