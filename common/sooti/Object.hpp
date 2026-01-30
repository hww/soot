#pragma once

#include "common/util/Crc32.hpp"
#include "common/util/Assert.hpp"
#include <string>
#include <fmt/format.h>
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
#include <type_traits>
#include <cstring>
#include <unordered_set>
#include "SourceInfo.hpp"

namespace script
{
    // Портируемые типы
    using FloatType = double;
    using IntType = int64_t;

    enum class ObjectType : uint8_t {
        UNDEFINED, 
        EMPTY_LIST, PAIR, 
        ARRAY, STRING_HASH_TABLE, 
        INTEGER, FLOAT, CHAR,
        SYMBOL, STRING, 
        LAMBDA, MACRO, 
        ENVIRONMENT, 
        READER,
        NATIVE_REF,
        CELL
    };

    enum class MemoryAccessKind {
        SINT8, UINT8, 
        SINT16, UINT16, 
        SINT32, UINT32, 
        SINT64, UINT64, 
        FLOAT, DOUBLE,
        POINTER,      // Просто адрес
        STRING        // Специфично для OpenGOAL (адрес на символы)
    };
    
    std::string object_type_to_string(ObjectType type);

    // Forward declarations
    class EnvironmentObject;
    class MacroObject;
    class LambdaObject;
    class PairObject;
    class HashTableObject;
    class FilePortObject;
    class SymbolTable;
    class StringObject;
    class ArrayObject;
    class TextStream;
    class ReaderObject;
    class Reader;
    class PlaceObject;
    class Object;
    class MemoryCell;

    struct ArgumentSpec;

    // InternedSymbolPtr как в OpenGOAL
    struct InternedSymbolPtr {
        const char* name_ptr;

        bool starts_with_colon() const {
            return name_ptr && name_ptr[0] != '\0' && name_ptr[0] == ':';
        }


        struct hash {
            auto operator()(const InternedSymbolPtr& x) const {
                return std::hash<const void*>()((const void*)x.name_ptr);
            }
        };

        const char* c_str() const { return name_ptr; }
        std::string as_string() { return std::string(name_ptr); }

        // Добавляем этот оператор
        operator std::string() const {
            return name_ptr ? std::string(name_ptr) : std::string();
        }
        
        // И, возможно, этот, чтобы можно было передавать в функции типа printf или fmod
        operator const char*() const {
            return name_ptr;
        }

        bool operator==(const char* msg) const { return strcmp(msg, name_ptr) == 0; }
        bool operator!=(const char* msg) const { return strcmp(msg, name_ptr) != 0; }
        bool operator==(const std::string& str) const { return str == name_ptr; }
        bool operator!=(const std::string& str) const { return str != name_ptr; }
        bool operator==(const InternedSymbolPtr& other) const { return other.name_ptr == name_ptr; }
        bool operator!=(const InternedSymbolPtr& other) const { return other.name_ptr != name_ptr; }
    };

    // FixedObject шаблон как в OpenGOAL
    template <typename T>
    std::string fixed_to_string(T x);

    template <>
    std::string fixed_to_string<FloatType>(FloatType x);

    template <>
    std::string fixed_to_string<char>(char x);

    template <>
    std::string fixed_to_string<IntType>(IntType x);

    template <>
    std::string fixed_to_string<InternedSymbolPtr>(InternedSymbolPtr x);

    template <typename T>
    class FixedObject {
    public:
        T value;

        explicit FixedObject(T v) : value(v) {}
        FixedObject() = default;

        std::string print() const {
             return fixed_to_string(value); 
        }

        Object inspect() const;

        bool operator==(const FixedObject<T>& other) const {
            return value == other.value;
        }

    private:
        std::string type_as_string() const {
            if constexpr (std::is_same_v<T, FloatType>)
                return object_type_to_string(ObjectType::FLOAT);
            if constexpr (std::is_same_v<T, IntType>)
                return object_type_to_string(ObjectType::INTEGER);
            if constexpr (std::is_same_v<T, char>)
                return object_type_to_string(ObjectType::CHAR);
            if constexpr (std::is_same_v<T, InternedSymbolPtr>)
                return object_type_to_string(ObjectType::SYMBOL);
            throw std::runtime_error("Unsupported FixedObject type");
        }
    };

    // Fixed object types
    using IntegerObject = FixedObject<IntType>;
    using FloatObject = FixedObject<FloatType>;
    using CharObject = FixedObject<char>;
    using SymbolObject = FixedObject<InternedSymbolPtr>;

    // Базовый класс для heap-allocated объектов
    class HeapObject : public std::enable_shared_from_this<HeapObject> {
    public:
        virtual ~HeapObject() = default;
        virtual std::string print() const = 0;
        virtual std::string printc() const { return print(); }
        virtual Object inspect() const = 0;

        // 1. Для оператора (-> base key)
        // По умолчанию объект не дает в себя "зайти".
        virtual Object make_step_alias(const Object& key);

        // 2. Для автоматического eval и явного (deref obj)
        // По умолчанию объект разыменовывается в самого себя.
        virtual Object deref();

        // 3. Для (set! obj val)
        // По умолчанию объекты в куче неизменяемы (кроме ячеек).
        virtual void assign(const Object& value);
    };

    // Main Object class
    class Object {
        friend class EnvironmentPrettyPrinter;
    public:
        ObjectType type = ObjectType::UNDEFINED;

        // For fixed types (value semantics) - как в OpenGOAL
        union {
            IntegerObject integer_obj;
            FloatObject float_obj;
            CharObject char_obj;
            SymbolObject symbol_obj;
        };

        // For heap types (reference semantics)
        std::shared_ptr<HeapObject> heap_obj;

        // Тот самый делегат который сообщает о текущей таблицк
        static void set_symbol_table(SymbolTable* table) { s_table = table; }
        static SymbolTable& symbol_table() { return *s_table; }
        static InternedSymbolPtr intern(const char* name);
        // адресация к объекту -> key
        Object step(const Object& key) const;
        
        // Constructors for fixed types
        static Object make_undefined();
        static Object make_integer(IntType value);
        static Object make_float(FloatType value);
        static Object make_char(char value);
        static Object make_null();
        static Object make_list(const std::vector<Object>& elements);
        static Object make_array(const std::vector<Object>& elements);
        static Object make_vector(const std::vector<Object>& elements);
        static Object make_symbol(const char* name);
        static Object make_keyword(const char* name);
        static Object make_symbol(std::string name) { return make_symbol(name.c_str()); }
        static Object make_keyword(std::string name) { return make_keyword(name.c_str()); }
        static Object make_boolean(bool v) { return v ? make_symbol("#t") : make_symbol("#f"); };
        static Object make_string(const std::string& text);
        static Object make_pair(const Object& car, const Object& cdr);
        static Object make_lambda(const ArgumentSpec& args, const Object& body, const std::shared_ptr<EnvironmentObject>& env);
        static Object make_macro(const ArgumentSpec& args, const Object& body, const std::shared_ptr<EnvironmentObject>& env);
        static Object make_hash_table(int size = 16);
        static Object make_reader(TextStream* textStream);
        static Object make_cell(std::shared_ptr<MemoryCell> cell, MemoryAccessKind type);
        static Object make_cell(void* raw_ptr, MemoryAccessKind type);
        static Object make_native_ref(std::shared_ptr<HeapObject> heap_object);

        // String representation
        std::string print() const;
        std::string printc() const { return is_heap_object() && heap_obj ? heap_obj->printc() : print(); } // сырой формат например без "" для строки
        std::string inspect_short() const;
        Object inspect() const;

        std::string type_name() const { return object_type_to_string(type); }

        // Type checking
        bool is_undefined() const { return type == ObjectType::UNDEFINED; }
        bool is_heap_object() const { return heap_obj != nullptr; }
        bool is_integer() const { return type == ObjectType::INTEGER; }
        bool is_float() const { return type == ObjectType::FLOAT; }
        bool is_char() const { return type == ObjectType::CHAR; }
        bool is_symbol() const { return type == ObjectType::SYMBOL; }
        bool is_keyword() const { return type == ObjectType::SYMBOL && symbol_obj.value.starts_with_colon(); }
        bool is_string() const { return type == ObjectType::STRING; }
        bool is_pair() const { return type == ObjectType::PAIR; }
        bool is_cell() const { return type == ObjectType::CELL; }
        bool is_native_ref() const { return type == ObjectType::NATIVE_REF; }
        bool is_array() const { return type == ObjectType::ARRAY; }
        bool is_null() const { return type == ObjectType::EMPTY_LIST; }
        bool is_list() const { return is_null() || is_pair(); }
        bool is_lambda() const { return type == ObjectType::LAMBDA; }
        bool is_macro() const { return type == ObjectType::MACRO; }
        bool is_vector() const { return type == ObjectType::ARRAY; }
        bool is_hash_table() const { return type == ObjectType::STRING_HASH_TABLE; }
        bool is_env() const { return type == ObjectType::ENVIRONMENT; }
        bool is_reader() const { return type == ObjectType::READER; }
        bool is_boolean() const { return is_symbol() && (as_symbol() == "#t" || as_symbol() == "#f"); }

        // Evaluates the truthiness of an object. Since the Object class lacks access 
        // to the Symbol Table, it must perform string comparisons, which is inefficient. 
        // For better performance, the Interpreter uses its own 'truthy()' method, 
        // which compares pre-interned symbols directly.
        bool as_boolean() const { 
            if (is_null()) return false;
            return !(is_symbol() && as_symbol() == "#f"); 
        }
        /**
         * @brief Evaluates the truthiness of an object in accordance with Common Lisp semantics.
         * * This method implements the core logical branching rule: an object is considered 
         * "false" (NIL) if it is either an empty list or the specific '#f' symbol. 
         * All other objects (including zero, empty strings, etc.) evaluate to "true".
         * * Optimization: Uses direct pointer comparison for the false symbol, 
         * leveraging the fact that symbols are interned.
         * * @param false_symbol A reference to the pre-interned symbol used for 'false' (e.g., "#f").
         * @return true if the object is truthy, false if it is an empty list or matches false_symbol.
         */
        bool truthy(InternedSymbolPtr false_symbol) const 
        { 
            // Ложь — это если объект является пустым списком ИЛИ символом #f
            if (is_null()) return false;
            return !(is_symbol() && as_symbol().name_ptr == false_symbol.name_ptr); 
        }
        
        // Value access with type checking
        char                        as_char() const;
        IntType                     as_integer() const;
        FloatType                   as_float() const;
        StringObject*               as_string() const;
        ArrayObject*                as_array() const;
        HashTableObject*            as_hash_table() const;
        MacroObject*                as_macro() const;
        LambdaObject*               as_lambda() const;
        EnvironmentObject*          as_env() const;
        ReaderObject*               as_reader() const;
        MemoryCell*                 as_cell() const;
        HeapObject*                 as_heap_object() const;
        const IntegerObject&        as_integer_obj() const;
        const InternedSymbolPtr&    as_symbol() const;
        std::shared_ptr<EnvironmentObject> as_env_ptr() const;
        std::string                 to_std_string() const;
    
        template <typename T>
        std::shared_ptr<T> as_native_ref() const {
            // 1. Проверяем, что объект вообще является нативной ссылкой (инкапсулированным указателем)
            if (!is_native_ref()) {
                return nullptr;
            }

            // 2. Извлекаем базовый указатель на HeapObject (или твой базовый класс для нативов)
            // Предполагаем, что m_data.heap_obj хранит shared_ptr
            auto base_ptr = heap_obj; 

            // 3. Пытаемся безопасно привести к целевому типу T
            auto casted_ptr = std::dynamic_pointer_cast<T>(base_ptr);
            
            return casted_ptr; 
        }

        // C++ идеоматичные методы
        std::vector<Object> as_c_vector() const;
        std::vector<std::string> as_c_vector_of_strings() const;

        // For pair access
        PairObject* as_pair() const;

        bool operator==(const Object& other) const;
        bool operator!=(const Object& other) const { return !(*this == other); }

    private:
        void throw_type_error(const std::string& expected) const;
        static SymbolTable* s_table; // Просто статический указатель
    };

    // Now define PairObject AFTER Object
    class PairObject : public HeapObject {
    public:
        Object car;
        Object cdr;
        PairObject() = default;
        PairObject(const Object& car, const Object& cdr) : car(car), cdr(cdr) {}
        ~PairObject() override = default;

        std::string print() const override;
        Object inspect() const override;

        int lenght() {
            int count = 1;
            auto lst = cdr;
            while (lst.is_pair()) {
                count++;
                lst = lst.as_pair()->cdr;
            }
            return count;
        }
    };

    class StringObject : public HeapObject {
    public:
        std::string data;
        explicit StringObject(std::string text) : data(std::move(text)) {}
        ~StringObject() override = default;

        int length() const { return data.length(); }
        bool empty() const { return data.empty(); }

        char at(const int index) {
            return data[index];
        }

        std::string print() const override {
            return "\"" + data + "\"";
        }

        std::string printc() const override {
            return data;
        }

        Object inspect() const override;

        // Неявное преобразование в std::string
        operator std::string() const {
            return data; 
        }

        operator const char*() const {
            return data.c_str();
        }
    };

    class ArrayObject : public HeapObject {
    public:
        std::vector<Object> data;
        
        explicit ArrayObject(std::vector<Object> elements) : data(std::move(elements)) {}
        ~ArrayObject() override = default;

        int size() { return data.size(); }

        Object& get(int index) { return data[index]; }
        void set(int index, Object value) { data[index] = value; }

        const Object& operator[](size_t idx) const { return data.at(idx); }
        Object& operator[](size_t idx) { return data.at(idx); }

        std::string print() const override {
            std::string result = "#(";
            if (data.empty()) {
                return result + ")";
            }
            for (const auto& obj : data) {
                result += obj.print() + " ";
            }
            result.pop_back();  // remove last space
            return result + ")";
        }

        Object inspect() const override;

    };

    class HashTableObject : public HeapObject {
    public:
        std::unordered_map<std::string, Object> data;

        HashTableObject() = default;
        HashTableObject(int size = 16) : data(size) {};
        ~HashTableObject() override = default;

        std::string print() const override {
            // Короткий системный принт: #<hash-table size:5>
            return fmt::format("#<hash-table size:{}>", data.size());
        }

        std::string print_long() const  {
            std::string result = "{";
            for (const auto& kv : data) {
            result += '(';
            result += kv.first;
            result += ' ';
            result += kv.second.print();
            result += ')';
            result += ' ';
            }
            if (!data.empty()) {
            result.pop_back();
            }
            result += '}';
            return result;
        }

        Object inspect() const override;

        // Метод получения: возвращает ссылку на объект. 
        // Если ключа нет, unordered_map создаст объект по умолчанию.
        Object& get(const std::string& key) {
            return data[key];
        }

        // Метод установки: записывает значение и возвращает ссылку на обновленное место.
        Object& set(const std::string& key, Object value) {
            data[key] = std::move(value); // используем move для эффективности
            return data[key];
        }

        // Доступ по строковому ключу (неконстантный): 
        // стандартное поведение для ассоциативных контейнеров.
        Object& operator[](const std::string& key) {
            return data[key];
        }

        // Доступ по индексу (size_t): 
        // В unordered_map нет прямого доступа по индексу, как в векторе.
        // Если это необходимо, используем итераторы (но помни, что порядок не гарантирован).
        const Object& operator[](size_t idx) const {
            if (idx >= data.size()) {
                throw std::out_of_range("HashTable index out of bounds");
            }
            auto it = data.begin();
            std::advance(it, idx);
            return it->second;
        }
    };

    template <typename T>
    class InternedPtrMap {       
        friend class EnvironmentPrettyPrinter;
    private:
        struct Entry {
            const char* key = nullptr;
            T value;
        };

        std::vector<Entry> m_entries;        
    public:

        InternedPtrMap(const InternedPtrMap&) = delete;
        InternedPtrMap& operator=(const InternedPtrMap&) = delete;
        InternedPtrMap() { clear(); }
        
        int size() const { return m_entries.size(); }
        const std::vector<Entry>& get_all_entries() const { return m_entries; }

        T* lookup(InternedSymbolPtr str) {
            if (m_entries.size() < 10) {
                for (auto& e : m_entries) {
                    if (e.key == str.name_ptr) {  // ← Сравниваем указатели!
                        return &e.value;
                    }
                }
                return nullptr;
            }
            uint32_t hash = util::compute_crc32(str.name_ptr, sizeof(const char*));  // ← Используем name_ptr

            // probe
            for (uint32_t i = 0; i < m_entries.size(); i++) {
                uint32_t slot_addr = (hash + i) & m_mask;
                auto& slot = m_entries[slot_addr];
                if (!slot.key) {
                    return nullptr;
                }
                else {
                    if (slot.key != str.name_ptr) {  // ← Сравниваем указатели!
                        continue;
                    }
                    return &slot.value;
                }
            }
            ASSERT_NOT_REACHED();
        }

        void set(InternedSymbolPtr ptr, const T& obj) {
            uint32_t hash = util::compute_crc32(ptr.name_ptr, sizeof(const char*));  // ← Используем name_ptr

            // probe
            for (uint32_t i = 0; i < m_entries.size(); i++) {
                uint32_t slot_addr = (hash + i) & m_mask;
                auto& slot = m_entries[slot_addr];
                if (!slot.key) {
                    // not found, insert!
                    slot.key = ptr.name_ptr;  // ← Сохраняем указатель!
                    slot.value = obj;
                    m_used_entries++;
                    if (m_used_entries >= m_next_resize) {
                        resize();
                    }
                    return;
                }
                else {
                    if (slot.key == ptr.name_ptr) {  // ← Сравниваем указатели!
                        slot.value = obj;
                        return;
                    }
                }
            }
            ASSERT_NOT_REACHED();
        }

        void clear() {
            m_entries.clear();
            m_power_of_two_size = 3;  // 2 ^ 3 = 8
            m_entries.resize(8);
            m_used_entries = 0;
            m_next_resize = (m_entries.size() * kMaxUsed);
            m_mask = 0b111;
        }

    private:

        void resize() {
            m_power_of_two_size++;
            m_mask = (1U << m_power_of_two_size) - 1;

            std::vector<Entry> new_entries(m_entries.size() * 2);
            for (const auto& old_entry : m_entries) {
                if (old_entry.key) {
                    bool done = false;
                    uint32_t hash = util::compute_crc32(old_entry.key, sizeof(const char*));
                    for (uint32_t i = 0; i < new_entries.size(); i++) {
                        uint32_t slot_addr = (hash + i) & m_mask;
                        auto& slot = new_entries[slot_addr];
                        if (!slot.key) {
                            slot.key = old_entry.key;
                            slot.value = std::move(old_entry.value);
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
        int m_power_of_two_size = 0;
        int m_used_entries = 0;
        int m_next_resize = 0;
        uint32_t m_mask = 0;
        static constexpr float kMaxUsed = 0.7;

    };

    class SymbolTable {
    public:
        struct TypeSymbols {
            Object empty_list;
            Object integer;
            Object float_pt;
            Object character;
            Object symbol;
            Object string;
            Object pair;
            Object array;
            Object hash_table;
            Object lambda;
            Object macro;
            Object environment;
            Object reader;
            Object lextoken;
            Object place;
            Object unknown;
            Object cell;
            Object sym_undefined;
            Object sym_true;
            Object sym_false;
            Object optional;
            Object key;
            Object rest;
            Object native_ref;

            Object true_or_false(bool val) { return val ? sym_true : sym_false; }
        } core;
        void init_core_symbols();
        Object object_type_to_symbol(ObjectType type);
    public:
        SymbolTable(const SymbolTable&) = delete;
        SymbolTable& operator=(const SymbolTable&) = delete;
        SymbolTable();
        ~SymbolTable();

        InternedSymbolPtr intern(const char* str);
        Object make_symbol(const char* name);
        Object make_keyword(const char* name);
        Object make_symbol(std::string name);
        Object make_keyword(std::string name);

        // Метод для итерации по символам
        template<typename F>
        void for_each_symbol(F func) const {
            for (const auto& entry : m_entries) {
                if (entry.name) {
                    func(InternedSymbolPtr{ entry.name });
                }
            }
        }

        size_t get_symbol_count() const { return m_used_entries; }
    private:
        void resize();
        int m_power_of_two_size = 0;
        struct Entry {
            uint32_t hash = 0;
            const char* name = nullptr;
        };
        std::vector<Entry> m_entries;
        int m_used_entries = 0;
        int m_next_resize = 0;
        uint32_t m_mask = 0;
        static constexpr float kMaxUsed = 0.7;
    };

    using EnvironmentMap = InternedPtrMap<Object>;

    class EnvironmentObject : public HeapObject
    {
    public:

        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        EnvironmentMap vars;

        EnvironmentObject() = default;
        EnvironmentObject(std::shared_ptr<EnvironmentObject> parent)
            : parent_env(std::move(parent)) {
        }
        ~EnvironmentObject() override = default;

        int size() const { return vars.size(); }

        Object* find(const char* n, SymbolTable* st) {
            return vars.lookup(st->intern(n));
        }

        Object* find(InternedSymbolPtr ptr) {
            return vars.lookup(ptr);
        }

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::ENVIRONMENT;
            obj.heap_obj = std::make_shared<EnvironmentObject>();
            return obj;
        }

        static Object make_new(std::string name,
            std::shared_ptr<EnvironmentObject> parent_env = nullptr) {
            Object obj;
            obj.type = ObjectType::ENVIRONMENT;
            auto env = std::make_shared<EnvironmentObject>();
            env->name = std::move(name);
            env->parent_env = std::move(parent_env);
            obj.heap_obj = std::move(env);
            return obj;
        }

        std::string print() const override {
            return fmt::format("#<env {} parent:{} @{:p}>", 
                name.empty() ? "anonymous" : name,
                parent_env ? parent_env->name : "none",
                (void*)this);
        }

        Object inspect() const override;
    };

    // Аргументы функций
    struct Arguments {
        std::vector<Object> unnamed;
        std::map<std::string, Object> named;
        std::vector<Object> rest;
        bool has_rest = false;

        Object inspect() const;

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

        std::string print() const;
        std::string print_full(size_t max_len = 512, size_t max_arg_len = 64) const;
    };

    /**
     * @brief Представляет аргумент с опциональным значением по умолчанию.
     * * Используется для обработки аргументов в секциях &key.
     * Позволяет различать ситуации, когда аргумент просто не передан 
     * (и должен использовать дефолт), и когда он отсутствует в спецификации.
     */
    struct NamedArg {
        /** * @brief Флаг наличия значения по умолчанию.
         * Если false, и аргумент не передан пользователем — это ошибка (для обязательных ключей).
         */        
        bool has_default = false;
        /** * @brief Значение, которое будет использовано, если аргумент не указан при вызове.
         * Может содержать любой Lisp-объект (число, символ, список и т.д.).
         */        
        Object default_value;
    };
    /**
     * @brief Представляет аргумент с опциональным значением по умолчанию.
     * * Используется для обработки аргументов в секциях &optional.
     * Позволяет различать ситуации, когда аргумент просто не передан 
     * (и должен использовать дефолт), и когда он отсутствует в спецификации.
     */
    struct PositionalArg {
        std::string name;
        bool is_optional;     // или просто проверка has_default
        Object default_value; // NIL по умолчанию для опциональных
    };
    /**
     * @brief Спецификация аргументов функции (lambda-list).
     * * Описывает структуру ожидаемых входных данных, разделяя их на категории
     * согласно стандартам Lisp (позиционные, опциональные, ключевые и rest-аргументы).
     */
    struct ArgumentSpec {
        /** @brief Разрешить использование ключевых слов (&key), например: :mode 'fast. */
        bool keys = true;
        /** @brief Разрешить неограниченное количество неименованных аргументов (для встроенных функций). */
        bool varargs = false;
        /** * @brief Список имен обязательных позиционных аргументов.
         * Должны быть переданы в строгом порядке в начале вызова.
         */        
        std::vector<PositionalArg> unnamed;   
        /** * @brief Именованные (ключевые) аргументы (&key).
         * Передаются в формате :имя значение. Порядок в вызове не имеет значения.
         */        
        std::unordered_map<std::string, NamedArg> named;    
        /** * @brief Имя переменной для захвата всех оставшихся аргументов (&rest).
         * Если не пустая, все лишние аргументы будут собраны в список с этим именем.
         */
        std::string rest;

        ArgumentSpec() : keys(false), varargs(false) {
        }
        ArgumentSpec(bool keys, bool varargs) : keys(keys), varargs(varargs) {
        }

        size_t size() const { return unnamed.size() + named.size(); }
        size_t unnamed_size() const { return unnamed.size(); }
        size_t named_size() const { return named.size(); }
        
        bool empty() const { return unnamed.empty() && named.empty(); }

        Object to_object() const;
        Object inspect() const;

        const PositionalArg& operator[](size_t index) const {
            if (index >= unnamed.size()) throw std::out_of_range("ArgumentSpec index out of range");
            return unnamed[index];
        }

        const PositionalArg* get_unnamed_spec(const std::string& name) const {
            for (const auto& arg : unnamed) {
                if (arg.name == name) return &arg;
            }
            return nullptr;
        }
        /**
         * @brief Проверяет, определен ли позиционный аргумент (обязательный или опциональный) с данным именем.
         * @param name Имя аргумента для поиска.
         * @return true, если аргумент найден в списке позиционных параметров.
         */
        bool has_unnamed(const std::string& name) const {
            // Используем std::find_if для поиска в векторе структур
            return std::any_of(unnamed.begin(), unnamed.end(), 
                [&name](const PositionalArg& arg) {
                    return arg.name == name;
                });
        }

        bool has_named(const std::string& name) const {
            return named.find(name) != named.end();
        }

        const NamedArg& get_named(const std::string& name) const {
            auto it = named.find(name);
            if (it == named.end()) throw std::out_of_range("Named argument not found: " + name);
            return it->second;
        }

        std::string print() const;
        std::string print_full(size_t max_len = 512, size_t max_arg_len = 64) const;

        static ArgumentSpec create(
            const std::vector<std::string>& required,
            const std::map<std::string, Object>& optional = {},
            const std::map<std::string, Object>& keys = {},
            const std::string& rest_name = ""
        );
    };
 
    class LambdaObject : public HeapObject {
    public:
        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        Object body;
        ArgumentSpec args;

        LambdaObject() = default;
        ~LambdaObject() override = default;

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::LAMBDA;
            obj.heap_obj = std::make_shared<LambdaObject>();
            return obj;
        }

        std::string print() const override {
            return name.empty() ? "#<unnamed lambda>" : "<lambda " + name + ">";
        }
        
        Object inspect() const override;
    };

    class MacroObject : public HeapObject {
    public:
        std::string name;
        std::shared_ptr<EnvironmentObject> parent_env;
        Object body;
        ArgumentSpec args;

        MacroObject() = default;
        ~MacroObject() override = default;

        static Object make_new() {
            Object obj;
            obj.type = ObjectType::MACRO;
            obj.heap_obj = std::make_shared<MacroObject>();
            return obj;
        }

        std::string print() const override {
            return name.empty() ? "#<unnamed macro>" : "#<macro " + name + ">";
        }

        Object inspect() const override;
    };

    class ReaderObject : public HeapObject {
        public:
        // Передаем указатель на активный поток разбора
        TextStream* ts = nullptr;

        explicit ReaderObject(TextStream* stream) : ts(stream) {}
        ~ReaderObject() override = default;

        // peek-char: смотрим символ через твой ts->peek()
        Object peek_char() const;

        // read-char: извлекаем символ через твой ts->read()
        Object read_char();

        // skip-whitespace: используем твой метод
        void skip_whitespace();

        // Проверка на конец файла
        bool is_eof() const;
        std::string print() const override;
        Object inspect() const override;
    };

    class MemoryCell : public HeapObject {
    public:
        void*               m_ptr;       // Прямой адрес в памяти (base_addr + offset)
        MemoryAccessKind    m_kind;      // Метаданные (как именно читать этот кусок памяти)
    
        MemoryCell(void* ptr) : m_ptr(ptr), m_kind(MemoryAccessKind::UINT32) {}

        Object get();

        void set(const Object& val);

        virtual Object make_step_alias(const Object& key);

        std::string print() const override;
        Object inspect() const override;

    };



    Object build_list(std::vector<Object>&& objects);
    Object build_list(const std::vector<Object>& objects);

} // namespace script
