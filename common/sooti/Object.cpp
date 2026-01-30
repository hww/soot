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

    
    // ============================================================================
    // SymbolTable
    // ============================================================================
    
    SymbolTable* Object::s_table = nullptr;
    
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
        core.sym_undefined  = make_symbol(":undefined");
        core.sym_true       = make_symbol("#t");
        core.sym_false      = make_symbol("#f");
        core.optional       = make_symbol(":optional");
        core.key            = make_symbol(":key");
        core.rest           = make_symbol(":rest");
        core.empty_list     = make_symbol("empty-list");
        core.integer        = make_symbol("integer");
        core.float_pt       = make_symbol("float");
        core.character      = make_symbol("char");
        core.symbol         = make_symbol("symbol");
        core.string         = make_symbol("string");
        core.pair           = make_symbol("pair");
        core.array          = make_symbol("array");
        core.hash_table     = make_symbol("hash-tabe");
        core.lambda         = make_symbol("lambda");
        core.macro          = make_symbol("macro");
        core.environment    = make_symbol("environment");
        core.reader         = make_symbol("reader");
        core.lextoken       = make_symbol("lextoken");
        core.unknown        = make_symbol("unknown");
        core.cell           = make_symbol("cell");
        core.native_ref     = make_symbol("native-ref");
        core.static_buffer  = make_symbol("static-buffer");
        core.static_writer  = make_symbol("static-writer");
    }
    Object SymbolTable::object_type_to_symbol(ObjectType type) {
        switch (type) {
            case ObjectType::UNDEFINED:         return core.sym_undefined;
            case ObjectType::EMPTY_LIST:        return core.empty_list; // было EmptyList
            case ObjectType::INTEGER:           return core.integer;    // было Integer
            case ObjectType::FLOAT:             return core.float_pt;   // было Float
            case ObjectType::CHAR:              return core.character;  // было Char
            case ObjectType::SYMBOL:            return core.symbol;
            case ObjectType::STRING:            return core.string;
            case ObjectType::PAIR:              return core.pair;
            case ObjectType::ARRAY:             return core.array;
            case ObjectType::STRING_HASH_TABLE: return core.hash_table;
            case ObjectType::LAMBDA:            return core.lambda;
            case ObjectType::MACRO:             return core.macro;
            case ObjectType::ENVIRONMENT:       return core.environment;
            case ObjectType::READER:            return core.reader;
            case ObjectType::CELL:              return core.cell;
            case ObjectType::NATIVE_REF:        return core.native_ref;
            case ObjectType::STATIC_BUFFER:     return core.static_buffer;
            case ObjectType::STATIC_WRITER:     return core.static_writer;
            default:                            return core.unknown;
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

    Object SymbolTable::make_symbol(const char* name) {
        Object obj;
        obj.type = ObjectType::SYMBOL;
        obj.symbol_obj.value = intern(name);  // ← преобразуем в string
        return obj;
    }

    Object SymbolTable::make_keyword(const char* name) {
        Object obj;
        obj.type = ObjectType::SYMBOL;

        if (name[0] == ':') {
            // Если уже начинается с ':', интернируем как есть
            obj.symbol_obj.value = intern(name);
        } else {
            // Если нет, добавляем префикс
            std::string name_with_colon = ":" + std::string(name);
            obj.symbol_obj.value = intern(name_with_colon.c_str());
        }

        return obj;
    }
    
    Object SymbolTable::make_symbol(std::string name) { return make_symbol(name.c_str()); }
    Object SymbolTable::make_keyword(std::string name) { return make_keyword(name.c_str()); }
    
    // ============================================================================
    // SymbolTable
    // ============================================================================

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

    // ============================================================================
    // Type Strings
    // ============================================================================

    std::string object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::UNDEFINED:
        return "[undefined]";
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
        case ObjectType::CELL:
        return "[cell]";
        case ObjectType::NATIVE_REF:
        return "[native-ref]";
        case ObjectType::STATIC_BUFFER:
        return "[static-buffer]";
        case ObjectType::STATIC_WRITER:
        return "[static-writer]";
        default:
            throw std::runtime_error(fmt::format("unknown object type {} in object_type_to_string", (int)type));
        }
    }

    void Object::throw_type_error(const std::string& expected) const {
        throw std::runtime_error("Type error: expected " + expected +
            ", got " + object_type_to_string(type));
    }

    Object Object::step(const Object& key) const {
        // Если тип объекта предполагает наличие HeapObject (CELL, NATIVE_REF, TYPE и т.д.)
        if (this->is_heap_object()) { 
            return this->as_heap_object()->make_step_alias(key);
        }
        
        throw std::runtime_error(fmt::format("Type {} does not support '->' operator", this->type_name()));
    }
        
    // ============================================================================
    // Object factory
    // ============================================================================

    // 1. Для оператора (-> base key)
    // По умолчанию объект не дает в себя "зайти".
    Object HeapObject::make_step_alias(const Object& key) {
        (void)key;
        throw std::runtime_error(fmt::format("Object {} is not navigable", this->print()));
    }

    // 2. Для автоматического eval и явного (deref obj)
    // По умолчанию объект разыменовывается в самого себя.
    Object HeapObject::deref() {
        return Object::make_native_ref(shared_from_this());
    }

    // 3. Для (set! obj val)
    // По умолчанию объекты в куче неизменяемы (кроме ячеек).
    void HeapObject::assign(const Object& value) {
        (void)value;
        throw std::runtime_error(fmt::format("Object {} is not assignable", this->print()));
    }

    // ============================================================================
    // Object factory
    // ============================================================================

    Object Object::make_undefined() { 
        Object obj;
        obj.type = ObjectType::UNDEFINED;
        return obj;        
    }

    Object Object::make_heap_object(std::shared_ptr<HeapObject> heap_object, ObjectType type)
    {
        Object obj;
        obj.type = type;
        obj.heap_obj = std::move(heap_object);
        return obj;     
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

    Object Object::make_null() {
        Object obj;
        obj.type = ObjectType::EMPTY_LIST;
        return obj;
    }

    Object Object::make_list(const std::vector<Object>& elements) {
        return build_list(elements);
    }
    
    InternedSymbolPtr Object::intern(const char* name) { 
        if (s_table) 
            return s_table->intern(name);
        throw std::runtime_error("call set_symbol_table(...) before");
    }

    Object Object::make_symbol(const char* name) {
        if (s_table) 
            return s_table->make_symbol(name);
        throw std::runtime_error("call set_symbol_table(...) before");
    }

    Object Object::make_keyword(const char* name) {
        if (s_table) 
            return s_table->make_keyword(name);
        throw std::runtime_error("call set_symbol_table(...) before");
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
        
    Object Object::make_native_ref(std::shared_ptr<HeapObject> heap_object)
    {
        Object obj;
        obj.type = ObjectType::NATIVE_REF;
        obj.heap_obj = std::move(heap_object);
        return obj;    
    }

    Object Object::make_cell(std::shared_ptr<MemoryCell> cell, MemoryAccessKind type)
    {
        Object obj;
        obj.type = ObjectType::CELL;
        
        // Сначала настраиваем данные внутри MemoryCell
        if (cell) {
            cell->m_kind = type;
        }
        
        // И только в самом конце отдаем владение объекту Object
        obj.heap_obj = std::move(cell); 
        return obj;
    }

    Object Object::make_cell(void* raw_ptr, MemoryAccessKind type) {
        // Создаем НОВЫЙ объект ячейки в куче, который будет смотреть на raw_ptr
        auto cell = std::make_shared<MemoryCell>(raw_ptr);
        return make_cell(std::move(cell), type);
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
        case ObjectType::UNDEFINED:
            return "udefined";
        case ObjectType::EMPTY_LIST:
            return "null";
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

std::string Object::to_std_string() const {
    switch (type) {
        case ObjectType::STRING:
            // Используем ссылку, чтобы избежать лишнего копирования до возврата
            return static_cast<StringObject*>(heap_obj.get())->data;
            
        case ObjectType::SYMBOL:
            // Предполагаем, что у символа есть поле value (std::string)
            return symbol_obj.value; 

        default:
            throw std::runtime_error("Cannot convert " + object_type_to_string(type) + " to std::string");
    }
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
  
    std::vector<std::string> Object::as_c_vector_of_strings() const {
        if (!is_list())
            throw std::runtime_error("as_c_vector_of_strings called on a " + object_type_to_string(type) + " " + print());
        std::vector<std::string> result;
        Object current = *this;
        while (current.is_pair()) {
            auto item = current.as_pair()->car;
            if (item.is_string())
                result.push_back(item.to_std_string());
            else if (item.is_symbol())
                result.push_back(item.as_symbol().c_str());
            else
                // fall back 
                result.push_back(item.print());
            current = current.as_pair()->cdr;
        }
        return result;
    }

    const IntegerObject& Object::as_integer_obj() const {
        if (!is_integer()) throw_type_error("integer");
        return integer_obj;
    }

    MemoryCell* Object::as_cell() const {
        if (type != ObjectType::CELL) {
            throw std::runtime_error("as_cell called on a " + object_type_to_string(type) +
                " " + print());
        }
        return dynamic_cast<MemoryCell*>(heap_obj.get());
    }
  
    HeapObject* Object::as_heap_object() const {
        if (type != ObjectType::NATIVE_REF) {
            throw std::runtime_error("as_reference called on a " + object_type_to_string(type) +
                " " + print());
        }
        return dynamic_cast<HeapObject*>(heap_obj.get());
    }

    // Comparison
    bool Object::operator==(const Object& other) const {
        if (type != other.type) return false;

        switch (type) {
        case ObjectType::UNDEFINED:
            return false;
            
        case ObjectType::STRING:
            return as_string()->data == other.as_string()->data;
        case ObjectType::INTEGER:
            return integer_obj.value == other.integer_obj.value;
        case ObjectType::FLOAT:
            return float_obj.value == other.float_obj.value;
        case ObjectType::CHAR:
            return char_obj.value == other.char_obj.value;
        case ObjectType::SYMBOL:
            return symbol_obj.value.name_ptr == other.symbol_obj.value.name_ptr;
        
        case ObjectType::ENVIRONMENT:
        case ObjectType::LAMBDA:
        case ObjectType::MACRO:
        case ObjectType::READER:
            return heap_obj == other.heap_obj;
        
        case ObjectType::EMPTY_LIST:
            return true;

        case ObjectType::PAIR:
            return as_pair()->car == other.as_pair()->car && as_pair()->cdr == other.as_pair()->cdr;
        case ObjectType::ARRAY: {
            auto this_arr = dynamic_cast<ArrayObject*>(heap_obj.get());
            auto other_arr = dynamic_cast<ArrayObject*>(other.heap_obj.get());
            if (!this_arr || !other_arr) return false;
            return this_arr->data == other_arr->data;
        }

        case ObjectType::STRING_HASH_TABLE:
            return as_hash_table()->data == other.as_hash_table()->data;

        default:
            throw std::runtime_error("equality not implemented for " + print());

        }
    }

    /*!
     * Build a list of objects from a vector of objects.
     */
    Object build_list(const std::vector<Object>& objects) {
        if (objects.empty()) {
            return Object::make_null();
        }

        // this is by far the most expensive part of parsing, so this is done a bit carefully.
        // we maintain a std::shared_ptr<PairObject> that represents the list, built from back to front.
        std::shared_ptr<PairObject> head =
            std::make_shared<PairObject>(objects.back(), Object::make_null());

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
            return Object::make_null();
        }

        // this is by far the most expensive part of parsing, so this is done a bit carefully.
        // we maintain a std::shared_ptr<PairObject> that represents the list, built from back to front.
        std::shared_ptr<PairObject> head =
            std::make_shared<PairObject>(objects.back(), Object::make_null());

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
            return Object::make_null(); // Или специальный EOF символ
        }
        return Object::make_char(ts->peek());
    }

    // read-char: извлекаем символ через твой ts->read()
    Object ReaderObject::read_char() {
        if (!ts || !ts->text_remains()) {
            return Object::make_null();
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

    Object ArgumentSpec::to_object() const {
        auto& symbols = Object::symbol_table();
        ListBuilder lb{symbols};

        // 1. Позиционные аргументы
        for (const auto& arg : unnamed) {
            lb.push_back(Object::make_symbol(arg.name.c_str()));
        }

        // 2. Именованные аргументы (Keyword arguments)
        if (!named.empty()) {
            lb.push_back(Object::make_keyword("key")); // Маркер &key
            for (const auto& [name, spec] : named) {
                if (spec.has_default) {
                    // Если есть дефолт: (name default)
                    ListBuilder entry{};
                    entry.push_back(Object::make_symbol(name.c_str()));
                    entry.push_back(spec.default_value);
                    lb.push_back(entry.finalize());
                } else {
                    lb.push_back(Object::make_symbol(name.c_str()));
                }
            }
        }

        // 3. Rest аргумент (вариативность)
        if (!rest.empty()) {
            lb.push_back(Object::make_symbol("rest")); // Маркер &rest
            lb.push_back(Object::make_symbol(rest.c_str()));
        }

        return lb.finalize();
    }

    ArgumentSpec ArgumentSpec::create(
            const std::vector<std::string>& required,
            const std::map<std::string, Object>& optional,
            const std::map<std::string, Object>& keys,
            const std::string& rest_name 
        ) {
            ArgumentSpec spec;
            spec.keys = !keys.empty();
            spec.rest = rest_name;
            spec.varargs = false; // Мы явно задаем структуру

            // 1. Обязательные позиционные аргументы
            for (const auto& name : required) {
                PositionalArg arg;
                arg.name = name;
                arg.is_optional = false;
                spec.unnamed.push_back(arg);
            }

            // 2. Опциональные позиционные аргументы (с дефолтами)
            for (const auto& [name, default_val] : optional) {
                PositionalArg arg;
                arg.name = name;
                arg.is_optional = !default_val.is_undefined();
                arg.default_value = default_val;
                spec.unnamed.push_back(arg);
            }

            // 3. Ключевые аргументы (&key с дефолтами)
            for (const auto& [name, default_val] : keys) {
                NamedArg arg;
                arg.has_default = !default_val.is_undefined();
                arg.default_value = default_val;
                spec.named[name] = arg;
            }

            return spec;
        }
        
    // ============================================================================
    // Memory Cell
    // ============================================================================
        
    Object MemoryCell::inspect() const {
        auto& symbols = Object::symbol_table();
        ListBuilder lb{symbols};

        lb.push_back(symbols.core.cell); // Символ 'cell
        
        // Вместо "base" и "key" мы показываем физику:
        lb.push_kv(symbols, "address", Object::make_integer((uintptr_t)m_ptr)); 
        
        // Показываем текущее значение, раз мы "арестовали" этот участок памяти
        try {
            lb.push_kv(symbols, "value", const_cast<MemoryCell*>(this)->get());
        } catch (...) {
            lb.push_kv(symbols, "value", symbols.core.unknown);
        }

        return lb.finalize();
    }

    Object MemoryCell::make_step_alias(const Object& key) {
        (void) key;
        // Пытаемся привести базовый Type* к StructType*
        throw std::runtime_error("Can't make step alias on the MemoryCell");
        return Object::make_undefined();
    }
    
    Object MemoryCell::get() {
        if (!m_ptr) return Object::make_undefined();

        switch (m_kind) {
            case MemoryAccessKind::SINT8:   return Object::make_integer(*(int8_t*)m_ptr);
            case MemoryAccessKind::UINT8:   return Object::make_integer(*(uint8_t*)m_ptr);
            
            case MemoryAccessKind::SINT16:  return Object::make_integer(*(int16_t*)m_ptr);
            case MemoryAccessKind::UINT16:  return Object::make_integer(*(uint16_t*)m_ptr);
            
            case MemoryAccessKind::SINT32:  return Object::make_integer(*(int32_t*)m_ptr);
            case MemoryAccessKind::UINT32:  return Object::make_integer(*(uint32_t*)m_ptr);
            
            case MemoryAccessKind::SINT64:  return Object::make_integer(*(int64_t*)m_ptr);
            case MemoryAccessKind::UINT64:  return Object::make_integer((int64_t)*(uint64_t*)m_ptr); // Каст к знаковому для Лиспа

            case MemoryAccessKind::FLOAT:   return Object::make_float(*(float*)m_ptr);
            case MemoryAccessKind::DOUBLE:  return Object::make_float((float)*(double*)m_ptr);

            case MemoryAccessKind::POINTER: 
                return Object::make_integer((uintptr_t)*(void**)m_ptr);

            case MemoryAccessKind::STRING: {
                // В OpenGOAL строка — это часто указатель на начало char данных
                char* str_ptr = *(char**)m_ptr;
                return str_ptr ? Object::make_string(str_ptr) : Object::make_null();
            }

            default:
                return Object::make_undefined();
        }
    }

    void MemoryCell::set(const Object& val) {
        if (!m_ptr) return;

        switch (m_kind) {
            case MemoryAccessKind::SINT8:   *(int8_t*)m_ptr  = (int8_t)val.as_integer(); break;
            case MemoryAccessKind::UINT8:   *(uint8_t*)m_ptr  = (uint8_t)val.as_integer(); break;
            
            case MemoryAccessKind::SINT16:  *(int16_t*)m_ptr = (int16_t)val.as_integer(); break;
            case MemoryAccessKind::UINT16:  *(uint16_t*)m_ptr = (uint16_t)val.as_integer(); break;
            
            case MemoryAccessKind::SINT32:  *(int32_t*)m_ptr = (int32_t)val.as_integer(); break;
            case MemoryAccessKind::UINT32:  *(uint32_t*)m_ptr = (uint32_t)val.as_integer(); break;
            
            case MemoryAccessKind::SINT64:  *(int64_t*)m_ptr = (int64_t)val.as_integer(); break;
            case MemoryAccessKind::UINT64:  *(uint64_t*)m_ptr = (uint64_t)val.as_integer(); break;

            case MemoryAccessKind::FLOAT:   *(float*)m_ptr   = val.as_float(); break;
            case MemoryAccessKind::DOUBLE:  *(double*)m_ptr  = (double)val.as_float(); break;

            case MemoryAccessKind::POINTER: 
                *(uintptr_t*)m_ptr = (uintptr_t)val.as_integer(); 
                break;

            case MemoryAccessKind::STRING:
                // Внимание: запись в строки обычно требует аллокации, 
                // здесь мы просто меняем указатель, если это допустимо.
                *(char**)m_ptr = const_cast<char*>(val.to_std_string().c_str()); 
                break;

            default:
                throw std::runtime_error("Unsupported memory write operation");
        }
    }

    // -- PRINTS --------------------------------------------------------------
    /**
     * Вспомогательная функция для безопасного строкового представления объекта
     */
    std::string truncate_obj(const Object& obj, size_t max_arg_len) {
        std::string s = obj.print(); // Используем существующий метод print объекта
        if (s.length() <= max_arg_len) return s;
        return s.substr(0, max_arg_len - 3) + "...";
    }
    std::string truncate_obj(const std::string& s, size_t max_arg_len) {
        if (s.length() <= max_arg_len) return s;
        return s.substr(0, max_arg_len - 3) + "...";
    }

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

    std::string ArgumentSpec::print_full(size_t max_len, size_t max_arg_len) const {

        std::stringstream ss;

        ss << "(";

        // 1. Позиционные аргументы (unnamed)
        if (!unnamed.empty()) {
            for (size_t i = 0; i < unnamed.size(); ++i) {
                ss << truncate_obj(unnamed[i].name, max_arg_len);
                if (i < unnamed.size() - 1) ss << " ";
            }
        }

        // 2. Ключевые аргументы (named)
        if (!named.empty()) {
            if (!unnamed.empty()) ss << " ";
            ss << "&key ";
            bool first = true;
            for (const auto& [name, val] : named) {
                if (!first) ss << " ";
                if (val.default_value.is_undefined())
                    ss << name;
                else
                    ss << "(" << name << " " << truncate_obj(val.default_value, max_arg_len) << ")";
                first = false;
            }
        }

        // 3. Остаток (rest)
        if (!rest.empty()) {
            if (!unnamed.empty() || !named.empty()) ss << " ";
            ss << "&rest: ";
            ss << truncate_obj(rest, max_arg_len);
        }

        ss << ")";

        std::string result = ss.str();
        if (result.length() > max_len) {
            return result.substr(0, max_len - 4) + "...}";
        }

        return result;
    }

    std::string Arguments::print() const {
        // Инстанция аргументов в момент вызова
        return fmt::format("#<args-invoked u:{} n:{} r:{}>", 
            unnamed.size(), 
            named.size(), 
            rest.size());
    }

    std::string Arguments::print_full(size_t max_len, size_t max_arg_len) const {

        std::stringstream ss;

        ss << "(";

        // 1. Позиционные аргументы (unnamed)
        if (!unnamed.empty()) {
            for (size_t i = 0; i < unnamed.size(); ++i) {
                ss << truncate_obj(unnamed[i], max_arg_len);
                if (i < unnamed.size() - 1) ss << " ";
            }
        }

        // 2. Ключевые аргументы (named)
        if (!named.empty()) {
            if (!unnamed.empty()) ss << " ";
            ss << "&key ";
            bool first = true;
            for (const auto& [name, val] : named) {
                if (!first) ss << " ";
                ss << ":" << name << " " << truncate_obj(val, max_arg_len);
                first = false;
            }
        }

        // 3. Остаток (rest)
        if (has_rest) {
            if (!unnamed.empty() || !named.empty()) ss << " ";
            ss << "&rest: ";
            for (size_t i = 0; i < rest.size(); ++i) {
                ss << truncate_obj(rest[i], max_arg_len);
                if (i < rest.size() - 1) ss << " ";
            }
        }

        ss << ")";

        std::string result = ss.str();
        if (result.length() > max_len) {
            return result.substr(0, max_len - 4) + "...}";
        }

        return result;
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
        if (!current.is_null()) {
            ss << " . " << current.print();
        }

        ss << ")";
        return ss.str();
    }

    std::string MemoryCell::print() const  { 
        return fmt::format("#<cell addr={:p}>", m_ptr); 
    }

    // -- INSPECTORS --------------------------------------------------------------

    std::string Object::inspect_short() const { 
        auto& symbols = Object::symbol_table();

        const int max_len = 64;
        // 1. Получаем S-expression инспекта
        Object info = this->inspect(); 
        
        // 2. Превращаем структуру в строку для отображения
        std::string str = info.print(); 
        
        if (str.size() <= max_len) return str;
        return str.substr(0, max_len - 3) + "...";
    }

    Object Object::inspect() const {
        auto& symbols = Object::symbol_table();

        switch (type) {
            case ObjectType::EMPTY_LIST:
                return Object::make_symbol("nil");

            case ObjectType::INTEGER: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol("integer"));
                lb.push_kv(symbols, "value", *this);
                return lb.finalize();
            }

            case ObjectType::FLOAT: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol("float"));
                lb.push_kv(symbols, "value", *this);
                return lb.finalize();
            }

            case ObjectType::SYMBOL: {
                ListBuilder lb{symbols};
                lb.push_back(Object::make_symbol("symbol"));
                lb.push_kv(symbols, "name", *this);
                return lb.finalize();
            }

            default:
                if (is_heap_object() && heap_obj) {
                    return heap_obj->inspect(); 
                }
                return Object::make_symbol("error-unknown");
        }
    }

    Object PairObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("pair"));
        lb.push_kv(symbols, "car", this->car);
        lb.push_kv(symbols, "cdr", this->cdr);
        return lb.finalize();
    }

    Object StringObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("string"));
        lb.push_kv(symbols, "value", Object::make_string(print().c_str()));
        lb.push_kv(symbols, "length", Object::make_integer(data.length()));
        return lb.finalize();
    }

    template <typename T>
    Object FixedObject<T>::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol(type_as_string().c_str()));
        lb.push_kv(symbols, "value", Object(value)); // Убрали & перед symbols
        return lb.finalize();
    }

    Object ArrayObject::inspect() const {
        auto& symbols = Object::symbol_table();
        ListBuilder lb{symbols};
        
        // 1. Имя типа
        lb.push_back(Object::make_symbol("array"));
        
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
            elements_lb.push_back(Object::make_symbol("..."));
        }

        lb.push_kv(symbols, "data", elements_lb.finalize());
        
        return lb.finalize();
    }

    // Удалено дублирующееся определение HashTableObject::inspect. Оставили одно:
    Object HashTableObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("hash-table"));
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

    Object EnvironmentObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("environment"));
        lb.push_kv(symbols, "name", name.empty() ? Object::make_symbol("anonymous") : Object::make_string(name.c_str()));
        
        if (parent_env) {
            lb.push_kv(symbols, "parent", Object::make_string(parent_env->print().c_str()));
        }

        // Если InternedPtrMap не поддерживает итераторы, выводим только количество
        lb.push_kv(symbols, "bindings-count", Object::make_integer(size()));
        lb.push_kv(symbols, "address", Object::make_integer((int64_t)this));
        return lb.finalize();
    }

    Object LambdaObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("lambda"));
        lb.push_kv(symbols, "name", name.empty() ? Object::make_symbol("anonymous") : Object::make_string(name.c_str()));
        lb.push_kv(symbols, "args", args.to_object()); 
        lb.push_kv(symbols, "body", body);
        return lb.finalize();
    }

    Object MacroObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        
        // 1. Заголовок типа
        lb.push_back(Object::make_symbol("macro"));
        
        // 2. Имя макроса (если есть)
        lb.push_kv(symbols, "name", name.empty() ? 
            Object::make_symbol("anonymous") : Object::make_string(name.c_str()));
        
        // 3. Спецификация аргументов
        // Вызываем to_object, который возвращает структуру аргументов (списки имён и т.д.)
        lb.push_kv(symbols, "args", args.to_object());
        
        // 4. Тело макроса (исходный код)
        lb.push_kv(symbols, "body", body);

        // 5. Адрес в памяти для отладки
        lb.push_kv(symbols, "address", Object::make_integer((int64_t)this));

        return lb.finalize();
    }

    Object ReaderObject::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("reader"));
        lb.push_kv(symbols, "line", Object::make_integer(ts ? ts->line_count : 0));
        return lb.finalize();
    }

    Object ArgumentSpec::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("argument-spec"));
        lb.push_kv(symbols, "has-rest", rest.empty() ? Object::make_null() : Object::make_symbol("#t"));
        lb.push_kv(symbols, "structure", this->to_object());
        return lb.finalize();
    }

    Object Arguments::inspect() const {
        auto& symbols = Object::symbol_table();

        ListBuilder lb{symbols};
        lb.push_back(Object::make_symbol("arguments-instance"));

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
