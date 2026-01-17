#include "common/CommonTypes.hpp"
#include "common/util/Crc32.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Reader.hpp"
#include "fmt/format.h"
#include <sstream>
#include <iostream>
#include <cstring>
#include <assert.h>


namespace script
{
    SymbolTable::SymbolTable() {
        m_power_of_two_size = 1;  // 2 ^ 1 = 2
        m_entries.resize(2);
        m_used_entries = 0;
        m_next_resize = (m_entries.size() * kMaxUsed);
        m_mask = 0b1;
        init_core_symbols();
    }

    SymbolTable::~SymbolTable() {
        for (auto& e : m_entries) {
            delete[] e.name;
        }
    }
    void SymbolTable::init_core_symbols() {
        core.empty_list     = Object::make_symbol(this, "empty-list");
        core.integer        = Object::make_symbol(this, "integer");
        core.float_pt       = Object::make_symbol(this, "float");
        core.character      = Object::make_symbol(this, "char");
        core.symbol         = Object::make_symbol(this, "symbol");
        core.string         = Object::make_symbol(this, "string");
        core.pair           = Object::make_symbol(this, "pair");
        core.array          = Object::make_symbol(this, "array");
        core.lambda         = Object::make_symbol(this, "lambda");
        core.macro          = Object::make_symbol(this, "macro");
        core.environment    = Object::make_symbol(this, "environment");
        core.reader         = Object::make_symbol(this, "reader");
        core.lextoken       = Object::make_symbol(this, "lextoken");
        core.unknown        = Object::make_symbol(this, "unknown");
    }
    Object SymbolTable::object_type_to_symbol(ObjectType type) {
        switch (type) {
            case ObjectType::EMPTY_LIST:    return core.empty_list; // было EmptyList
            case ObjectType::INTEGER:       return core.integer;    // было Integer
            case ObjectType::FLOAT:         return core.float_pt;   // было Float
            case ObjectType::CHAR:          return core.character;  // было Char
            case ObjectType::SYMBOL:        return core.symbol;
            case ObjectType::STRING:        return core.string;
            case ObjectType::PAIR:          return core.pair;
            case ObjectType::ARRAY:         return core.array;
            case ObjectType::LAMBDA:        return core.lambda;
            case ObjectType::MACRO:         return core.macro;
            case ObjectType::ENVIRONMENT:   return core.environment;
            case ObjectType::READER:        return core.reader;
            case ObjectType::LEXTOKEN:      return core.lextoken;
            default:                        return core.unknown;
        }
    }

    InternedSymbolPtr SymbolTable::intern(const char* str) {
        size_t string_len = strlen(str);
        uint32_t hash = util::compute_crc32(str, string_len);

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


    // Специализации fixed_to_string
    template <>
    std::string fixed_to_string<FloatType>(FloatType x) {
        std::stringstream ss;
        ss << x;
        return ss.str();
    }

    template <>
    std::string fixed_to_string<char>(char x) {
        switch (x) {
            case '\n': return "#\\newline";
            case ' ':  return "#\\space";
            case '\t': return "#\\tab";
            case '\r': return "#\\return";
            case '\0': return "#\\null";
            case '\b': return "#\\backspace";
            case 27:   return "#\\escape"; // ESC символ
            default:
                // Проверяем, является ли символ печатным (printable)
                if (std::isprint(static_cast<unsigned char>(x))) {
                    return std::string("#\\") + x;
                } else {
                    // Если символ непечатный, выводим его код в hex для отладки
                    char buf[16];
                    snprintf(buf, sizeof(buf), "#\\x%02x", static_cast<unsigned char>(x));
                    return std::string(buf);
                }
        }
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
        case ObjectType::EMPTY_LIST:
        return "[empty list]";
        case ObjectType::INTEGER:
        return "[integer]";
        case ObjectType::FLOAT:
        return "[float]";
        case ObjectType::CHAR:
        return "[char]";
        case ObjectType::SYMBOL:
        return "[symbol]";
        case ObjectType::STRING:
        return "[string]";
        case ObjectType::PAIR:
        return "[pair]";
        case ObjectType::ARRAY:
        return "[array]";
        case ObjectType::LAMBDA:
        return "[lambda]";
        case ObjectType::MACRO:
        return "[macro]";
        case ObjectType::ENVIRONMENT:
        return "[environment]";
        case ObjectType::STRING_HASH_TABLE:
        return "[string-hash-table]";
        case ObjectType::READER:
        return "[reader]";
        case ObjectType::LEXTOKEN:
        return "[lextoken]";
        default:
            throw std::runtime_error("unknown object type in object_type_to_string");
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

    Object Object::make_list(const std::vector<Object>& elements) {
        return build_list(elements);
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
    
    Object Object::make_reader(TextStream* textStream)
    {
        Object obj;
        obj.type = ObjectType::READER;
        obj.heap_obj = std::make_shared<ReaderObject>(textStream);
        return obj;    
    }

    Object Object::make_lextoken(const Object& type, const Object& value, const TextRef& info)
    {
        Object obj;
        obj.type = ObjectType::LEXTOKEN;
        // Создаем shared_ptr для твоего нового класса
        obj.heap_obj = std::make_shared<LextokenObject>(type, value, info);
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
        default:
            if (is_heap_object())
                return heap_obj ? heap_obj->print() : "[invalid-heap-object]";
            else
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
        default:
            if (is_heap_object())
                return heap_obj ? heap_obj->inspect() : "[invalid-heap-object]";
            else
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

    ReaderObject* Object::as_reader() const {
        if (type != ObjectType::READER) {
            throw std::runtime_error("as_reader called on a " + object_type_to_string(type) +
                " " + print());
        }
        return dynamic_cast<ReaderObject*>(heap_obj.get());
    }
    
    LextokenObject* Object::as_lextoken() const {
        if (type != ObjectType::LEXTOKEN) {
            throw std::runtime_error("as_lextoken called on a " + object_type_to_string(type) +
                " " + print());
        }
        return dynamic_cast<LextokenObject*>(heap_obj.get());
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

    const IntegerObject& Object::as_integer_obj() const {
        if (!is_integer()) throw_type_error("integer");
        return integer_obj;
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
        ss << "(";
        
        // Печатаем первый элемент
        ss << car.print();

        Object current = cdr;
        // Пока хвост — это пара, печатаем её car через пробел
        while (current.is_pair()) {
            ss << " " << current.as_pair()->car.print();
            current = current.as_pair()->cdr;
        }

        // Если список завершился не пустым списком, а чем-то другим (dotted pair)
        if (!current.is_empty_list()) {
            ss << " . " << current.print();
        }

        ss << ")";
        return ss.str();
    }

    std::string PairObject::inspect() const {
        return "[pair] " + print() + "\n";;
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

    /*!
     * Build a list of objects from a vector of objects.
     */
    Object build_list(const std::vector<Object>& objects) {
        if (objects.empty()) {
            return Object::make_empty_list();
        }

        // this is by far the most expensive part of parsing, so this is done a bit carefully.
        // we maintain a std::shared_ptr<PairObject> that represents the list, built from back to front.
        std::shared_ptr<PairObject> head =
            std::make_shared<PairObject>(objects.back(), Object::make_empty_list());

        s64 idx = ((s64)objects.size()) - 2;
        while (idx >= 0) {
            Object next;
            next.type = ObjectType::PAIR;
            next.heap_obj = std::move(head);

            head = std::make_shared<PairObject>();
            head->car = objects[idx];
            head->cdr = std::move(next);

            idx--;
        }

        Object result;
        result.type = ObjectType::PAIR;
        result.heap_obj = head;
        return result;
    }

    Object build_list(std::vector<Object>&& objects) {
        if (objects.empty()) {
            return Object::make_empty_list();
        }

        // this is by far the most expensive part of parsing, so this is done a bit carefully.
        // we maintain a std::shared_ptr<PairObject> that represents the list, built from back to front.
        std::shared_ptr<PairObject> head =
            std::make_shared<PairObject>(objects.back(), Object::make_empty_list());

        s64 idx = ((s64)objects.size()) - 2;
        while (idx >= 0) {
            Object next;
            next.type = ObjectType::PAIR;
            next.heap_obj = std::move(head);

            head = std::make_shared<PairObject>();
            head->car = std::move(objects[idx]);
            head->cdr = std::move(next);

            idx--;
        }

        Object result;
        result.type = ObjectType::PAIR;
        result.heap_obj = std::move(head);
        return result;
    }


    // peek-char: смотрим символ через твой ts->peek()
    Object ReaderObject::peek_char() const {
        if (!ts || !ts->text_remains()) {
            return Object::make_empty_list(); // Или специальный EOF символ
        }
        return Object::make_char(ts->peek());
    }

    // read-char: извлекаем символ через твой ts->read()
    Object ReaderObject::read_char() {
        if (!ts || !ts->text_remains()) {
            return Object::make_empty_list();
        }
        // Твой ts->read() сам инкрементирует seek и line_count
        return Object::make_char(ts->read());
    }

    // skip-whitespace: используем твой метод
    void ReaderObject::skip_whitespace() {
        if (ts && ts->text_remains()) {
            ts->seek_past_whitespace_and_comments();
        }
    }

    // Проверка на конец файла
    bool ReaderObject::is_eof() const {
        return !ts || !ts->text_remains();
    }

    std::string ReaderObject::print() const { return "#<reader-stream>"; }
    std::string ReaderObject::inspect() const { 
        return "[reader] seek: " + std::to_string(ts ? ts->seek : 0) + " line: " + std::to_string(ts ? ts->line_count : 0); 
    }

    std::string LextokenObject::print() const { return "#<lextoken>"; }
    std::string LextokenObject::inspect() const { 
        return "[lextoken] type: " + type.inspect() + " value: " + value.inspect(); 
    }
} // namespace script