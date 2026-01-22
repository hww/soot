#include "common/CommonTypes.hpp"
#include "common/util/Crc32.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/ListBuilder.hpp"
#include "common/sooti/Reader.hpp"
#include "common/sooti/Errors.hpp"

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
        core.object_true    = Object::make_symbol(this, "#t");
        core.object_false   = Object::make_symbol(this, "#f");
        core.object_nil     = Object::make_empty_list();

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

    Object Object::make_keyword(SymbolTable* table, const char* name) {
        Object obj;
        obj.type = ObjectType::SYMBOL;

        if (name[0] == ':') {
            // Если уже начинается с ':', интернируем как есть
            obj.symbol_obj.value = table->intern(name);
        } else {
            // Если нет, добавляем префикс
            std::string name_with_colon = ":" + std::string(name);
            obj.symbol_obj.value = table->intern(name_with_colon.c_str());
        }

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

    Object Object::make_hash_table(int size) {
        Object obj;
        obj.type = ObjectType::STRING_HASH_TABLE;
        obj.heap_obj = std::make_shared<HashTableObject>(size);
        return obj;
    }
    
    Object Object::make_reader(TextStream* textStream)
    {
        Object obj;
        obj.type = ObjectType::READER;
        obj.heap_obj = std::make_shared<ReaderObject>(textStream);
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
            return "NIL";
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

    Object ArgumentSpec::to_object(SymbolTable& symbols) const {
        ListBuilder lb{};

        // 1. Позиционные аргументы
        for (const auto& arg : unnamed) {
            lb.push_back(Object::make_symbol(&symbols, arg.name.c_str()));
        }

        // 2. Именованные аргументы (Keyword arguments)
        if (!named.empty()) {
            lb.push_back(Object::make_keyword(&symbols, "key")); // Маркер &key
            for (const auto& [name, spec] : named) {
                if (spec.has_default) {
                    // Если есть дефолт: (name default)
                    ListBuilder entry{};
                    entry.push_back(Object::make_symbol(&symbols, name.c_str()));
                    entry.push_back(spec.default_value);
                    lb.push_back(entry.finalize());
                } else {
                    lb.push_back(Object::make_symbol(&symbols, name.c_str()));
                }
            }
        }

        // 3. Rest аргумент (вариативность)
        if (!rest.empty()) {
            lb.push_back(Object::make_symbol(&symbols, "rest")); // Маркер &rest
            lb.push_back(Object::make_symbol(&symbols, rest.c_str()));
        }

        return lb.finalize();
    }

    // -- PRINTS --------------------------------------------------------------

    std::string ArgumentSpec::print() const {
        // Вместо "ArgumentSpec: unnamed=2..." сделаем более сжатый системный вид
        return fmt::format("#<arg-spec u:{} n:{} r:{}{}{}>", 
            unnamed.size(), 
            named.size(), 
            rest.empty() ? "0" : "1",
            keys ? " +rest" : "",
            varargs ? " +vararg" : ""
        );
    }

    std::string Arguments::print() const {
        // Инстанция аргументов в момент вызова
        return fmt::format("#<args-invoked u:{} n:{} r:{}>", 
            unnamed.size(), 
            named.size(), 
            rest.size());
    }

    std::string ReaderObject::print() const { 
        return "#<reader-stream>"; 
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

    // -- INSPECTORS --------------------------------------------------------------

    std::string Object::inspect_short(SymbolTable& symbols) const { 
        const int max_len = 64;
        // 1. Получаем S-expression инспекта
        Object info = this->inspect(symbols); 
        
        // 2. Превращаем структуру в строку для отображения
        std::string str = info.print(); 
        
        if (str.size() <= max_len) return str;
        return str.substr(0, max_len - 3) + "...";
    }

    Object Object::inspect(SymbolTable& symbols) const {
        switch (type) {
            case ObjectType::EMPTY_LIST:
                return Object::make_symbol(&symbols, "nil");

            case ObjectType::INTEGER: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol(&symbols, "integer"));
                lb.push_kv(symbols, "value", *this);
                return lb.finalize();
            }

            case ObjectType::FLOAT: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol(&symbols, "float"));
                lb.push_kv(symbols, "value", *this);
                return lb.finalize();
            }

            case ObjectType::SYMBOL: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol(&symbols, "symbol"));
                lb.push_kv(symbols, "name", *this);
                return lb.finalize();
            }

            default:
                if (is_heap_object() && heap_obj) {
                    return heap_obj->inspect(symbols); 
                }
                return Object::make_symbol(&symbols, "error-unknown");
        }
    }

    Object PairObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "pair"));
        lb.push_kv(symbols, "car", this->car);
        lb.push_kv(symbols, "cdr", this->cdr);
        return lb.finalize();
    }

    Object StringObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "string"));
        lb.push_kv(symbols, "value", Object::make_string(print().c_str()));
        lb.push_kv(symbols, "length", Object::make_integer(data.length()));
        return lb.finalize();
    }

    template <typename T>
    Object FixedObject<T>::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, type_as_string().c_str()));
        lb.push_kv(symbols, "value", Object(value)); // Убрали & перед symbols
        return lb.finalize();
    }

    Object ArrayObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        
        // 1. Имя типа
        lb.push_back(Object::make_symbol(&symbols, "array"));
        
        // 2. Метаданные
        lb.push_kv(symbols, "length", Object::make_integer(data.size()));
        lb.push_kv(symbols, "address", Object::make_integer((int64_t)this));

        // 3. Содержимое (опционально, выводим первые 10 элементов, чтобы не заспамить консоль)
        ListBuilder elements_lb{symbols};
        size_t limit = std::min(data.size(), (size_t)10);
        for (size_t i = 0; i < limit; ++i) {
            elements_lb.push_back(data[i]);
        }
        
        if (data.size() > 10) {
            elements_lb.push_back(Object::make_symbol(&symbols, "..."));
        }

        lb.push_kv(symbols, "data", elements_lb.finalize());
        
        return lb.finalize();
    }

    // Удалено дублирующееся определение HashTableObject::inspect. Оставили одно:
    Object HashTableObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "hash-table"));
        lb.push_kv(symbols, "size", Object::make_integer(data.size()));
        
        ListBuilder entries_lb{symbols};
        for (const auto& [key, val] : data) {
            ListBuilder pair_lb{symbols};
            pair_lb.push_back(Object::make_string(key.c_str()));
            pair_lb.push_back(val);
            entries_lb.push_back(pair_lb.finalize());
        }
        lb.push_kv(symbols, "entries", entries_lb.finalize());
        return lb.finalize();
    }

    Object EnvironmentObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "environment"));
        lb.push_kv(symbols, "name", name.empty() ? Object::make_symbol(&symbols, "anonymous") : Object::make_string(name.c_str()));
        
        if (parent_env) {
            lb.push_kv(symbols, "parent", Object::make_string(parent_env->print().c_str()));
        }

        // Если InternedPtrMap не поддерживает итераторы, выводим только количество
        lb.push_kv(symbols, "bindings-count", Object::make_integer(size()));
        lb.push_kv(symbols, "address", Object::make_integer((int64_t)this));
        return lb.finalize();
    }

    Object LambdaObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "lambda"));
        lb.push_kv(symbols, "name", name.empty() ? Object::make_symbol(&symbols, "anonymous") : Object::make_string(name.c_str()));
        lb.push_kv(symbols, "args", args.to_object(symbols)); 
        lb.push_kv(symbols, "body", body);
        return lb.finalize();
    }

    Object MacroObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        
        // 1. Заголовок типа
        lb.push_back(Object::make_symbol(&symbols, "macro"));
        
        // 2. Имя макроса (если есть)
        lb.push_kv(symbols, "name", name.empty() ? 
            Object::make_symbol(&symbols, "anonymous") : Object::make_string(name.c_str()));
        
        // 3. Спецификация аргументов
        // Вызываем to_object, который возвращает структуру аргументов (списки имён и т.д.)
        lb.push_kv(symbols, "args", args.to_object(symbols));
        
        // 4. Тело макроса (исходный код)
        lb.push_kv(symbols, "body", body);

        // 5. Адрес в памяти для отладки
        lb.push_kv(symbols, "address", Object::make_integer((int64_t)this));

        return lb.finalize();
    }

    Object ReaderObject::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "reader"));
        lb.push_kv(symbols, "line", Object::make_integer(ts ? ts->line_count : 0));
        return lb.finalize();
    }

    Object ArgumentSpec::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "argument-spec"));
        lb.push_kv(symbols, "has-rest", rest.empty() ? Object::make_empty_list() : Object::make_symbol(&symbols, "t"));
        lb.push_kv(symbols, "structure", this->to_object(symbols));
        return lb.finalize();
    }

    Object Arguments::inspect(SymbolTable& symbols) const {
        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(&symbols, "arguments-instance"));

        ListBuilder u_list{symbols};
        for (const auto& obj : unnamed) u_list.push_back(obj);
        lb.push_kv(symbols, "unnamed", u_list.finalize());

        ListBuilder n_list{symbols};
        for (const auto& [name, obj] : named) {
            n_list.push_kv(symbols, name.c_str(), obj); // Убрали & перед symbols
        }
        lb.push_kv(symbols, "named", n_list.finalize());

        return lb.finalize();
    }
} // namespace script
