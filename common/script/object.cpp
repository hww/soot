#include "object.h"
#include "util/crc32.h"
#include <sstream>
#include <iostream>
#include <cstring>
#include <util/assert.h>

namespace script
{
    // Специализации fixed_to_string
    template <>
    std::string fixed_to_string<FloatType>(FloatType x) {
        std::stringstream ss;
        ss << x;
        return ss.str();
    }

    template <>
    std::string fixed_to_string<char>(char x) {
        return std::string(1, x);
    }

    template <>
    std::string fixed_to_string<IntType>(IntType x) {
        return std::to_string(x);
    }

    template <>
    std::string fixed_to_string<InternedSymbolPtr>(InternedSymbolPtr x) {
        return x.name_ptr ? std::string(x.name_ptr) : "";
    }

    std::string object_type_to_string(ObjectType type) {
        switch (type) {
        case ObjectType::EMPTY_LIST: return "empty-list";
        case ObjectType::INTEGER: return "integer";
        case ObjectType::FLOAT: return "float";
        case ObjectType::CHAR: return "char";
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
        obj.integer_obj.value = value;
        return obj;
    }

    Object Object::make_float(FloatType value) {
        Object obj;
        obj.type = ObjectType::FLOAT;
        obj.float_obj.value = value;
        return obj;
    }

    Object Object::make_char(char value) {
        Object obj;
        obj.type = ObjectType::CHAR;
        obj.char_obj.value = value;
        return obj;
    }

    Object Object::make_empty_list() {
        Object obj;
        obj.type = ObjectType::EMPTY_LIST;
        return obj;
    }

    Object Object::make_symbol(SymbolTable* table, const char* name) {
        Object obj;
        obj.type = ObjectType::SYMBOL;
        obj.symbol_obj.value = table->intern(name);  // ← преобразуем в string
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
        return make_array(elements); // Векторы и массивы - одно и то же
    }

    Object Object::make_hash_table() {
        Object obj;
        obj.type = ObjectType::STRING_HASH_TABLE;
        obj.heap_obj = std::make_shared<HashTableObject>();
        return obj;
    }

    Object Object::make_lambda(const ArgumentSpec& args, const Object& body,
        const std::shared_ptr<EnvironmentObject>& env) {
        Object obj = LambdaObject::make_new();
        auto lambda = obj.as_lambda();
        lambda->args = args;
        lambda->body = body;
        lambda->parent_env = env;
        return obj;
    }

    Object Object::make_macro(const ArgumentSpec& args, const Object& body,
        const std::shared_ptr<EnvironmentObject>& env) {
        Object obj = MacroObject::make_new();
        auto macro = obj.as_macro();
        macro->args = args;
        macro->body = body;
        macro->parent_env = env;
        return obj;
    }

    // String representations
    std::string Object::print() const {
        switch (type) {
        case ObjectType::EMPTY_LIST:
            return "()";
        case ObjectType::INTEGER:
            return integer_obj.print();
        case ObjectType::FLOAT:
            return float_obj.print();
        case ObjectType::CHAR:
            return char_obj.print();
        case ObjectType::SYMBOL:
            return symbol_obj.print();
        case ObjectType::LAMBDA:
        case ObjectType::MACRO:
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
            return integer_obj.inspect();
        case ObjectType::FLOAT:
            return float_obj.inspect();
        case ObjectType::CHAR:
            return char_obj.inspect();
        case ObjectType::SYMBOL:
            return symbol_obj.inspect();
        case ObjectType::LAMBDA:
        case ObjectType::MACRO:
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
    PairObject* Object::as_pair() const {
        if (type != ObjectType::PAIR) {
            throw std::runtime_error("as_pair called on a " + object_type_to_string(type) + " " + print());
        }
        return dynamic_cast<PairObject*>(heap_obj.get());
    }

    EnvironmentObject* Object::as_env() const {
        if (type != ObjectType::ENVIRONMENT) {
            throw std::runtime_error("as_env called on a " + object_type_to_string(type) + " " + print());
        }
        return static_cast<EnvironmentObject*>(heap_obj.get());
    }

    std::shared_ptr<EnvironmentObject> Object::as_env_ptr() const {
        if (type != ObjectType::ENVIRONMENT) {
            throw std::runtime_error("as_env called on a " + object_type_to_string(type) + " " + print());
        }
        return std::dynamic_pointer_cast<EnvironmentObject>(heap_obj);
    }

    StringObject* Object::as_string() const {
        if (type != ObjectType::STRING) {
            throw std::runtime_error("as_string called on a " + object_type_to_string(type) + " " +
                print());
        }
        return static_cast<StringObject*>(heap_obj.get());
    }


    LambdaObject* Object::as_lambda() const {
        if (type != ObjectType::LAMBDA) {
            throw std::runtime_error("as_lambda called on a " + object_type_to_string(type) + " " +
                print());
        }
        return static_cast<LambdaObject*>(heap_obj.get());
    }

    MacroObject* Object::as_macro() const {
        if (type != ObjectType::MACRO) {
            throw std::runtime_error("as_macro called on a " + object_type_to_string(type) + " " + print());
        }
        return static_cast<MacroObject*>(heap_obj.get());
    }

    IntType Object::as_integer() const {
        if (type != ObjectType::INTEGER) {
            throw std::runtime_error("as_integer called on a " + object_type_to_string(type) +
                " " + print());
        }
        return integer_obj.value;
    }

    FloatType Object::as_float() const {
        if (type != ObjectType::FLOAT) {
            throw std::runtime_error("as_float called on a " + object_type_to_string(type) +
                " " + print());
        }
        return float_obj.value;
    }

    char Object::as_char() const {
        if (type != ObjectType::CHAR) {
            throw std::runtime_error("as_char called on a " + object_type_to_string(type) +
                " " + print());
        }
        return char_obj.value;
    }

    const InternedSymbolPtr& Object::as_symbol() const {
        if (type != ObjectType::SYMBOL) {
            throw std::runtime_error("as_symbol called on a " + object_type_to_string(type) +
                " " + print());
        }
        return symbol_obj.value;
    }

    ArrayObject* Object::as_array() const {
        if (type != ObjectType::ARRAY) {
            throw std::runtime_error("as_array called on a " + object_type_to_string(type) + " " + print());
        }
        return static_cast<ArrayObject*>(heap_obj.get());
    }

    HashTableObject* Object::as_hash_table() const {
        if (type != ObjectType::STRING_HASH_TABLE) {
            throw std::runtime_error("as_string_hash_table called on a " + object_type_to_string(type) +
                " " + print());
        }
        return dynamic_cast<HashTableObject*>(heap_obj.get());
    }

    std::vector<Object> Object::as_c_vector() const {
        if (!is_list())
            throw std::runtime_error("as_vector called on a " + object_type_to_string(type) + " " + print());
        std::vector<Object> result;
        Object current = *this;
        while (current.is_pair()) {
            result.push_back(current.as_pair()->car);
            current = current.as_pair()->cdr;
        }
        return result;
    }

    // Comparison
    bool Object::operator==(const Object& other) const {
        if (type != other.type) return false;

        switch (type) {
        case ObjectType::EMPTY_LIST:
            return true;
        case ObjectType::INTEGER:
            return integer_obj.value == other.integer_obj.value;
        case ObjectType::FLOAT:
            return float_obj.value == other.float_obj.value;
        case ObjectType::CHAR:
            return char_obj.value == other.char_obj.value;
        case ObjectType::SYMBOL:
            return symbol_obj.value.name_ptr == other.symbol_obj.value.name_ptr;
        case ObjectType::STRING:
            return as_string() == other.as_string();
        case ObjectType::PAIR:
            return as_pair()->car == other.as_pair()->car && as_pair()->cdr == other.as_pair()->cdr;
        case ObjectType::ARRAY: {
            auto this_arr = dynamic_cast<ArrayObject*>(heap_obj.get());
            auto other_arr = dynamic_cast<ArrayObject*>(other.heap_obj.get());
            if (!this_arr || !other_arr) return false;
            return this_arr->data == other_arr->data;
        }
        default:
            return heap_obj.get() == other.heap_obj.get();
        }
    }

    // PairObject implementations
    std::string PairObject::print() const {
        std::stringstream ss;
        ss << "(" << car.print();

        Object current = cdr;
        while (current.is_pair()) {
            ss << " " << current.as_pair()->car.print();
            current = current.as_pair()->cdr;
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


    SymbolTable::SymbolTable() {
        m_power_of_two_size = 1;  // 2 ^ 1 = 2
        m_entries.resize(2);
        m_used_entries = 0;
        m_next_resize = (m_entries.size() * kMaxUsed);
        m_mask = 0b1;
    }

    SymbolTable::~SymbolTable() {
        for (auto& e : m_entries) {
            delete[] e.name;
        }
    }

    InternedSymbolPtr SymbolTable::intern(const char* str) {
        size_t string_len = strlen(str);
        uint32_t hash = compute_crc32(str, string_len);

        // probe
        for (uint32_t i = 0; i < m_entries.size(); i++) {
            uint32_t slot_addr = (hash + i) & m_mask;
            auto& slot = m_entries[slot_addr];
            if (!slot.name) {
                // not found, insert!
                slot.hash = hash;
                auto* name = new char[string_len + 1];
                memcpy(name, str, string_len + 1);
                slot.name = name;
                m_used_entries++;

                if (m_used_entries >= m_next_resize) {
                    resize();
                    return intern(str);
                }
                return { name };
            }
            else {
                if (slot.hash != hash) {
                    continue;  // bad hash
                }
                if (strcmp(slot.name, str) != 0) {
                    continue;  // bad name
                }
                return { slot.name };
            }
        }

        // should be impossible to reach.
        ASSERT_NOT_REACHED();
    }

    void SymbolTable::resize() {
        m_power_of_two_size++;
        m_mask = (1U << m_power_of_two_size) - 1;

        std::vector<Entry> new_entries(m_entries.size() * 2);
        for (const auto& old_entry : m_entries) {
            if (old_entry.name) {
                bool done = false;
                for (uint32_t i = 0; i < new_entries.size(); i++) {
                    uint32_t slot_addr = (old_entry.hash + i) & m_mask;
                    auto& slot = new_entries[slot_addr];
                    if (!slot.name) {
                        slot.name = old_entry.name;
                        slot.hash = old_entry.hash;
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
} // namespace script