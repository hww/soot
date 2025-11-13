#include "object.h"
#include <sstream>

// === РЕАЛИЗАЦИИ ПРОСТЫХ ТИПОВ ===

class EmptyListObject : public HeapObject {
public:
    std::string print() const override { return "()"; }
    std::string inspect() const override { return "[empty list]"; }
};

class StringObject : public HeapObject {
public:
    std::string data;
    explicit StringObject(std::string data) : data(std::move(data)) {}
    std::string print() const override { return "\"" + data + "\""; }
    std::string inspect() const override { 
        return "[string] \"" + data + "\" (length: " + std::to_string(data.size()) + ")";
    }
};

class PairObject : public HeapObject {
public:
    Object car;
    Object cdr;
    PairObject(const Object& car, const Object& cdr) : car(car), cdr(cdr) {}
    std::string print() const override;
    std::string inspect() const override { return "[pair] " + print(); }
};

class ArrayObject : public HeapObject {
public:
    std::vector<Object> elements;
    explicit ArrayObject(std::vector<Object> elements) : elements(std::move(elements)) {}
    std::string print() const override;
    std::string inspect() const override { 
        return "[array] size: " + std::to_string(elements.size()) + " " + print();
    }
};

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===

std::string object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::EMPTY_LIST: return "empty-list";
        case ObjectType::INTEGER: return "integer";
        case ObjectType::FLOAT: return "float";
        case ObjectType::CHAR: return "char";
        case ObjectType::BOOLEAN: return "boolean";
        case ObjectType::SYMBOL: return "symbol";
        case ObjectType::STRING: return "string";
        case ObjectType::PAIR: return "pair";
        case ObjectType::ARRAY: return "array";
        case ObjectType::LAMBDA: return "lambda";
        case ObjectType::MACRO: return "macro";
        case ObjectType::ENVIRONMENT: return "environment";
        case ObjectType::INVALID: return "invalid";
        default: return "unknown";
    }
}

// === МЕТОДЫ OBJECT ===

Object Object::make_integer(IntType value) {
    Object obj;
    obj.type = ObjectType::INTEGER;
    obj.integer_value = value;
    return obj;
}

Object Object::make_float(FloatType value) {
    Object obj;
    obj.type = ObjectType::FLOAT;
    obj.float_value = value;
    return obj;
}

Object Object::make_char(char value) {
    Object obj;
    obj.type = ObjectType::CHAR;
    obj.char_value = value;
    return obj;
}

Object Object::make_boolean(bool value) {
    Object obj;
    obj.type = ObjectType::BOOLEAN;
    obj.boolean_value = value;
    return obj;
}

Object Object::make_empty_list() {
    static std::shared_ptr<HeapObject> empty_list = std::make_shared<EmptyListObject>();
    Object obj;
    obj.type = ObjectType::EMPTY_LIST;
    obj.heap_obj = empty_list;
    return obj;
}

Object Object::make_string(const std::string& text) {
    Object obj;
    obj.type = ObjectType::STRING;
    obj.heap_obj = std::make_shared<StringObject>(text);
    return obj;
}

Object Object::make_pair(const Object& car, const Object& cdr) {
    Object obj;
    obj.type = ObjectType::PAIR;
    obj.heap_obj = std::make_shared<PairObject>(car, cdr);
    return obj;
}

Object Object::make_array(const std::vector<Object>& elements) {
    Object obj;
    obj.type = ObjectType::ARRAY;
    obj.heap_obj = std::make_shared<ArrayObject>(elements);
    return obj;
}

std::string Object::print() const {
    switch (type) {
        case ObjectType::INTEGER: return std::to_string(integer_value);
        case ObjectType::FLOAT: return std::to_string(float_value);
        case ObjectType::CHAR: return std::string(1, char_value);
        case ObjectType::BOOLEAN: return boolean_value ? "#t" : "#f";
        case ObjectType::EMPTY_LIST: return "()";
        default: return heap_obj ? heap_obj->print() : "<?>";
    }
}

std::string Object::inspect() const {
    switch (type) {
        case ObjectType::INTEGER: return "[integer] " + std::to_string(integer_value);
        case ObjectType::FLOAT: return "[float] " + std::to_string(float_value);
        case ObjectType::CHAR: return "[char] " + std::string(1, char_value);
        case ObjectType::BOOLEAN: return "[boolean] " + std::string(boolean_value ? "#t" : "#f");
        case ObjectType::EMPTY_LIST: return "[empty list]";
        default: return heap_obj ? heap_obj->inspect() : "[invalid]";
    }
}

IntType Object::as_integer() const {
    if (!is_integer()) throw_type_error("integer");
    return integer_value;
}

FloatType Object::as_float() const {
    if (!is_float()) throw_type_error("float");
    return float_value;
}

char Object::as_char() const {
    if (!is_char()) throw_type_error("char");
    return char_value;
}

bool Object::as_boolean() const {
    if (!is_boolean()) throw_type_error("boolean");
    return boolean_value;
}

Object Object::car() const {
    if (!is_pair()) throw_type_error("pair");
    auto pair = std::static_pointer_cast<PairObject>(heap_obj);
    return pair->car;
}

Object Object::cdr() const {
    if (!is_pair()) throw_type_error("pair");
    auto pair = std::static_pointer_cast<PairObject>(heap_obj);
    return pair->cdr;
}

void Object::throw_type_error(const std::string& expected) const {
    throw std::runtime_error("Expected " + expected + ", got " + object_type_to_string(type));
}

bool Object::operator==(const Object& other) const {
    if (type != other.type) return false;
    
    switch (type) {
        case ObjectType::INTEGER: return integer_value == other.integer_value;
        case ObjectType::FLOAT: return float_value == other.float_value;
        case ObjectType::CHAR: return char_value == other.char_value;
        case ObjectType::BOOLEAN: return boolean_value == other.boolean_value;
        case ObjectType::EMPTY_LIST: return true; // singleton
        default: return heap_obj.get() == other.heap_obj.get(); // pointer comparison
    }
}

// === РЕАЛИЗАЦИИ PRINT ДЛЯ СЛОЖНЫХ ТИПОВ ===

std::string PairObject::print() const {
    std::ostringstream oss;
    oss << "(" << car.print();
    
    Object current = cdr;
    while (current.is_pair()) {
        auto pair_obj = std::static_pointer_cast<PairObject>(current.heap_obj);
        oss << " " << pair_obj->car.print();
        current = pair_obj->cdr;
    }
    
    if (!current.is_empty_list()) {
        oss << " . " << current.print();
    }
    
    oss << ")";
    return oss.str();
}

std::string ArrayObject::print() const {
    std::ostringstream oss;
    oss << "#(";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << " ";
        oss << elements[i].print();
    }
    oss << ")";
    return oss.str();
}

// === SYMBOL TABLE ===

Object SymbolTable::intern(const std::string& name) {
    auto it = table.find(name);
    if (it != table.end()) {
        Object obj;
        obj.type = ObjectType::SYMBOL;
        obj.heap_obj = it->second;
        return obj;
    }
    
    auto symbol = std::make_shared<SymbolObject>(name);
    table[name] = symbol;
    
    Object obj;
    obj.type = ObjectType::SYMBOL;
    obj.heap_obj = symbol;
    return obj;
}