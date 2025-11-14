#include "object.h"
#include "crc32.h"
#include <sstream>
#include <iostream>

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

void Object::throw_type_error(const std::string& expected) const {
    throw std::runtime_error("Type error: expected " + expected + 
                           ", got " + object_type_to_string(type));
}

// Constructors
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
    Object obj;
    obj.type = ObjectType::EMPTY_LIST;
    return obj;
}

Object Object::make_symbol(const std::string& name) {
    Object obj;
    obj.type = ObjectType::SYMBOL;
    obj.heap_obj = std::make_shared<SymbolObject>(name);
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

Object Object::make_vector(const std::vector<Object>& elements) {
    Object obj;
    obj.type = ObjectType::ARRAY; // или создаем новый ObjectType::VECTOR
    obj.heap_obj = std::make_shared<VectorObject>(elements);
    return obj;
}

Object Object::make_hash_table() {
    Object obj;
    obj.type = ObjectType::STRING_HASH_TABLE; // или создаем новый тип
    obj.heap_obj = std::make_shared<HashTableObject>();
    return obj;
}
Object Object::make_file_port(const std::string& filename) {
    Object obj;
    obj.type = ObjectType::FILE_PORT; // нужно добавить этот тип в enum
    obj.heap_obj = std::make_shared<FilePortObject>(filename);
    return obj;
}


// String representations
std::string Object::print() const {
    switch (type) {
        case ObjectType::EMPTY_LIST:
            return "()";
        case ObjectType::INTEGER:
            return std::to_string(integer_value);
        case ObjectType::FLOAT:
            return std::to_string(float_value);
        case ObjectType::CHAR:
            return std::string(1, char_value);
        case ObjectType::BOOLEAN:
            return boolean_value ? "#t" : "#f";
        case ObjectType::LAMBDA:
            return heap_obj ? heap_obj->inspect() : "[lambda]";            
        case ObjectType::SYMBOL:
        case ObjectType::STRING:
        case ObjectType::PAIR:
        case ObjectType::ARRAY:
        case ObjectType::ENVIRONMENT:
            return heap_obj ? heap_obj->print() : "[invalid]";
        default:
            return "[unknown]";
    }
}

std::string Object::inspect() const {
    switch (type) {
        case ObjectType::EMPTY_LIST:
            return "[empty-list]";
        case ObjectType::INTEGER:
            return "[integer] " + std::to_string(integer_value);
        case ObjectType::FLOAT:
            return "[float] " + std::to_string(float_value);
        case ObjectType::CHAR:
            return "[char] '" + std::string(1, char_value) + "'";
        case ObjectType::BOOLEAN:
            return boolean_value ? "[boolean] #t" : "[boolean] #f";
        case ObjectType::LAMBDA:
        case ObjectType::SYMBOL:
        case ObjectType::STRING:
        case ObjectType::PAIR:
        case ObjectType::ARRAY:
        case ObjectType::ENVIRONMENT:
            return heap_obj ? heap_obj->inspect() : "[invalid]";
        default:
            return "[unknown]";
    }
}

// Value accessors
IntType Object::as_integer() const {
    if (type != ObjectType::INTEGER) {
        throw_type_error("integer");
    }
    return integer_value;
}

FloatType Object::as_float() const {
    if (type != ObjectType::FLOAT) {
        throw_type_error("float");
    }
    return float_value;
}

char Object::as_char() const {
    if (type != ObjectType::CHAR) {
        throw_type_error("char");
    }
    return char_value;
}

bool Object::as_boolean() const {
    if (type != ObjectType::BOOLEAN) {
        throw_type_error("boolean");
    }
    return boolean_value;
}

std::string Object::as_symbol() const {
    if (type != ObjectType::SYMBOL) {
        throw_type_error("symbol");
    }
    auto sym = dynamic_cast<SymbolObject*>(heap_obj.get());
    return sym ? sym->name : "";
}

std::string Object::as_string() const {
    if (type != ObjectType::STRING) {
        throw_type_error("string");
    }
    auto str = dynamic_cast<StringObject*>(heap_obj.get());
    return str ? str->text : "";
}

std::vector<Object> Object::as_vector() const {
    if (type != ObjectType::ARRAY) {
        throw_type_error("vector");
    }
    auto vec = dynamic_cast<VectorObject*>(heap_obj.get());
    return vec ? vec->elements : std::vector<Object>();
}

HashTableObject* Object::as_hash_table() const {
    if (!is_hash_table()) {
        throw_type_error("hash-table");
    }
    return dynamic_cast<HashTableObject*>(heap_obj.get());
}

FilePortObject* Object::as_file_port() const {
    if (!is_file_port()) throw_type_error("file-port");
    return dynamic_cast<FilePortObject*>(heap_obj.get());
}
// Pair accessors
Object Object::car() const {
    if (type != ObjectType::PAIR) {
        throw_type_error("pair");
    }
    auto pair = dynamic_cast<PairObject*>(heap_obj.get());
    return pair ? pair->car : Object::make_empty_list();
}

Object Object::cdr() const {
    if (type != ObjectType::PAIR) {
        throw_type_error("pair");
    }
    auto pair = dynamic_cast<PairObject*>(heap_obj.get());
    return pair ? pair->cdr : Object::make_empty_list();
}

PairObject* Object::as_pair() const {
    if (type != ObjectType::PAIR) {
        throw_type_error("pair");
    }
    return dynamic_cast<PairObject*>(heap_obj.get());
}

// Comparison
bool Object::operator==(const Object& other) const {
    if (type != other.type) return false;
    
    switch (type) {
        case ObjectType::EMPTY_LIST:
            return true;
        case ObjectType::INTEGER:
            return integer_value == other.integer_value;
        case ObjectType::FLOAT:
            return float_value == other.float_value;
        case ObjectType::CHAR:
            return char_value == other.char_value;
        case ObjectType::BOOLEAN:
            return boolean_value == other.boolean_value;
        case ObjectType::SYMBOL:
            return as_symbol() == other.as_symbol();
        case ObjectType::STRING:
            return as_string() == other.as_string();
        case ObjectType::PAIR:
            return car() == other.car() && cdr() == other.cdr();
        case ObjectType::ARRAY: {
            auto this_arr = dynamic_cast<ArrayObject*>(heap_obj.get());
            auto other_arr = dynamic_cast<ArrayObject*>(other.heap_obj.get());
            if (!this_arr || !other_arr) return false;
            return this_arr->elements == other_arr->elements;
        }
        default:
            return false;
    }
}

// PairObject implementations
std::string PairObject::print() const {
    std::stringstream ss;
    ss << "(" << car.print();
    
    Object current = cdr;
    while (current.is_pair()) {
        ss << " " << current.car().print();
        current = current.cdr();
    }
    
    if (!current.is_empty_list()) {
        ss << " . " << current.print();
    }
    
    ss << ")";
    return ss.str();
}

std::string PairObject::inspect() const {
    std::stringstream ss;
    ss << "[pair] car=" << car.inspect() << " cdr=" << cdr.inspect();
    return ss.str();
}


// SymbolTable implementation с CRC32
SymbolTable::SymbolTable() {
    m_power_of_two_size = 1;
    m_entries.resize(2);
    m_used_entries = 0;
    m_next_resize = m_entries.size() * kMaxUsed;
    m_mask = 0b1;
}

SymbolTable::~SymbolTable() {
    for (auto& e : m_entries) {
        delete[] e.name;
    }
}

uint32_t SymbolTable::compute_hash(const char* data, size_t length) const {
    return compute_crc32(data, length);
}

Object SymbolTable::intern(const std::string& name) {
    const char* str = name.c_str();
    size_t string_len = name.length();
    uint32_t hash = compute_hash(str, string_len);

    // Linear probing
    for (uint32_t i = 0; i < m_entries.size(); i++) {
        uint32_t slot_addr = (hash + i) & m_mask;
        auto& slot = m_entries[slot_addr];

        if (!slot.name) {
            // Insert new symbol
            slot.hash = hash;
            char* name_copy = new char[string_len + 1];
            memcpy(name_copy, str, string_len + 1);
            slot.name = name_copy;
            m_used_entries++;

            if (m_used_entries >= m_next_resize) {
                resize();
                return intern(name);
            }

            return Object::make_symbol(name_copy);
        }
        else {
            if (slot.hash != hash) continue;
            if (strcmp(slot.name, str) != 0) continue;

            // Symbol already exists
            return Object::make_symbol(slot.name);
        }
    }

    throw std::runtime_error("SymbolTable: impossible state reached");
}

void SymbolTable::resize() {
    m_power_of_two_size++;
    m_mask = (1U << m_power_of_two_size) - 1;

    std::vector<Entry> new_entries(m_entries.size() * 2);

    for (const auto& old_entry : m_entries) {
        if (old_entry.name) {
            bool inserted = false;
            for (uint32_t i = 0; i < new_entries.size(); i++) {
                uint32_t slot_addr = (old_entry.hash + i) & m_mask;
                auto& slot = new_entries[slot_addr];
                if (!slot.name) {
                    slot.name = old_entry.name;
                    slot.hash = old_entry.hash;
                    inserted = true;
                    break;
                }
            }
            if (!inserted) {
                throw std::runtime_error("SymbolTable: resize failed");
            }
        }
    }

    m_entries = std::move(new_entries);
    m_next_resize = kMaxUsed * m_entries.size();
}

// Вспомогательные функции
ArgumentSpec make_varargs() {
    ArgumentSpec spec;
    spec.varargs = true;
    return spec;
}

std::string ArgumentSpec::print() const {
    std::stringstream ss;
    ss << "ArgumentSpec: unnamed=" << unnamed.size()
        << " named=" << named.size()
        << " rest=" << (rest.empty() ? "none" : rest)
        << " varargs=" << (varargs ? "true" : "false");
    return ss.str();
}

std::string Arguments::print() const {
    std::stringstream ss;
    ss << "Arguments: unnamed=" << unnamed.size()
        << " named=" << named.size()
        << " rest=" << rest.size();
    return ss.str();
}