#pragma once

#include "common/util/Assert.hpp"
#include "common/util/Crc32.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <functional>

namespace script {
// Портируемые типы
using FloatType = double;
using IntType = int64_t;

enum class ObjectType : uint8_t {
    NONE,
    PRIMITIVE,
    SPECIAL_FORM,
    EMPTY_LIST,
    PAIR,
    ARRAY,
    STRING_HASH_TABLE,
    INTEGER,
    FLOAT,
    CHAR,
    SYMBOL,
    STRING,
    LAMBDA,
    MACRO,
    ENVIRONMENT,
    READER,
    NATIVE_REF,
    POINTER,
    STATIC_BUFFER,
    STATIC_WRITER,
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
class Pointer;

struct ArgumentSpec;

struct DeclareSettings {
    bool is_set = false;            // has the user set these with a (declare)?
    bool inline_by_default = false; // if a function, inline when possible?
    bool save_code = true;          // if a function, should we save the code?
    bool allow_inline = false;      // should we allow the user to use this an inline function
    bool print_asm = false;         // should we print out the asm for this function?
};

// InternedSymbolPtr как в OpenGOAL
struct InternedSymbolPtr {
    const char *name_ptr;

    std::string class_name() const {
        return "InternedSymbolPtr";
    }
    bool starts_with_colon() const {
        return name_ptr && name_ptr[0] != '\0' && name_ptr[0] == ':';
    }

    struct hash {
        auto operator()(const InternedSymbolPtr &x) const {
            return std::hash<const void *>()((const void *)x.name_ptr);
        }
    };

    const char *c_str() const {
        return name_ptr;
    }
    std::string as_string() {
        return std::string(name_ptr);
    }

    // Добавляем этот оператор
    operator std::string() const {
        return name_ptr ? std::string(name_ptr) : std::string();
    }

    // И, возможно, этот, чтобы можно было передавать в функции типа printf или fmod
    operator const char *() const {
        return name_ptr;
    }

    bool operator==(const char *msg) const {
        return strcmp(msg, name_ptr) == 0;
    }
    bool operator!=(const char *msg) const {
        return strcmp(msg, name_ptr) != 0;
    }
    bool operator==(const std::string &str) const {
        return str == name_ptr;
    }
    bool operator!=(const std::string &str) const {
        return str != name_ptr;
    }
    bool operator==(const InternedSymbolPtr &other) const {
        return other.name_ptr == name_ptr;
    }
    bool operator!=(const InternedSymbolPtr &other) const {
        return other.name_ptr != name_ptr;
    }
};

// FixedObject шаблон как в OpenGOAL
template <typename T> std::string fixed_to_string(T x);

template <> std::string fixed_to_string<FloatType>(FloatType x);

template <> std::string fixed_to_string<char>(char x);

template <> std::string fixed_to_string<IntType>(IntType x);

template <> std::string fixed_to_string<InternedSymbolPtr>(InternedSymbolPtr x);

template <typename T> class FixedObject {
  public:
    T value;

    explicit FixedObject(T v) : value(v) {}
    FixedObject() = default;

    std::string print() const {
        return fixed_to_string(value);
    }

    Object inspect() const;

    bool operator==(const FixedObject<T> &other) const {
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
    virtual bool is_table() const {
        // has methods get_at and set_at?
        return false;
    }
    virtual Object   make_step_accessor(const Object &key);
    virtual Object   get_at(const Object &key);
    virtual void     set_at(const Object &key, const Object &val);
    virtual uint32_t as_crc32() {
        return 0;
    };
    virtual Object      inspect() const = 0;
    virtual std::string print() const = 0;
    virtual std::string printc() const {
        return print();
    }
    virtual std::string class_name() const {
        return "HeapObject";
    }
    virtual std::string type_name() const {
        return "none";
    }
};

class NativeRef : public HeapObject {
  public:
    // NativeRef просто говорит: "Я — ссылка на что-то нативное".
    // Тут можно оставить только метод для интроспекции или получения сырого адреса самого объекта.
    virtual ~NativeRef() = default;
    std::string class_name() const override {
        return "NativeRef";
    }
};

// Main Object class
class Object {
    friend class EnvironmentPrettyPrinter;

  public:
    ObjectType type = ObjectType::NONE;

    // For fixed types (value semantics) - как в OpenGOAL
    union {
        IntegerObject integer_obj;
        FloatObject   float_obj;
        CharObject    char_obj;
        SymbolObject  symbol_obj;
    };

    // For heap types (reference semantics)
    std::shared_ptr<HeapObject> heap_obj;

    virtual std::string class_name() const {
        return "Pointer";
    }
    // Тот самый делегат который сообщает о текущей таблицк
    static void set_symbol_table(SymbolTable *table) {
        s_table = table;
    }
    static inline SymbolTable *get_symbol_table();
    static inline SymbolTable &symbol_table();
    static InternedSymbolPtr   intern(const char *name);
    // адресация к объекту -> key
    Object step(const Object &key) const;

    // Constructors for fixed types
    static Object make_none();
    static Object make_integer(IntType value);
    static Object make_float(FloatType value);
    static Object make_char(char value);
    static Object make_null();
    static Object make_list(const std::vector<Object> &elements);
    static Object make_array(const std::vector<Object> &elements);
    static Object make_vector(const std::vector<Object> &elements);
    static Object make_symbol(const char *name);
    static Object make_keyword(const char *name);
    static Object make_symbol(std::string name) {
        return make_symbol(name.c_str());
    }
    static Object make_keyword(std::string name) {
        return make_keyword(name.c_str());
    }
    static Object make_boolean(bool v) {
        return v ? make_symbol("#t") : make_symbol("#f");
    };
    static Object make_string(const std::string &text);
    static Object make_pair(const Object &car, const Object &cdr);
    static Object make_lambda(const ArgumentSpec &args, const Object &body,
                              const std::shared_ptr<EnvironmentObject> &env);
    static Object make_macro(const ArgumentSpec &args, const Object &body,
                             const std::shared_ptr<EnvironmentObject> &env);
    static Object make_hash_table(int size = 16);
    static Object make_hash_table(Object type_name, int size = 16);
    static Object make_reader(TextStream *textStream);
    static Object make_pointer(std::shared_ptr<Pointer> pointer);
    static Object make_pointer(void *raw_ptr, std::string type);
    static Object make_native_ref(std::shared_ptr<HeapObject> heap_object);
    static Object make_heap_object(std::shared_ptr<HeapObject> heap_object, ObjectType type);

    // String representation
    std::string print() const;
    std::string printc() const {
        return is_heap_object() && heap_obj ? heap_obj->printc() : print();
    } // сырой формат например без "" для строки
    std::string inspect_short() const;
    Object      inspect() const;

    std::string type_name() const {
        return object_type_to_string(type);
    }

    // Type checking
    bool is_type(ObjectType atype) const {
        return type == atype;
    }
    bool is_none() const {
        return type == ObjectType::NONE;
    }
    bool is_heap_object() const {
        return heap_obj != nullptr;
    }
    bool is_integer() const {
        return type == ObjectType::INTEGER;
    }
    bool is_float() const {
        return type == ObjectType::FLOAT;
    }
    bool is_char() const {
        return type == ObjectType::CHAR;
    }
    bool is_string() const {
        return type == ObjectType::STRING;
    }
    bool is_symbol() const {
        return type == ObjectType::SYMBOL;
    }
    bool is_string_or_symbol() const {
        return type == ObjectType::STRING || type == ObjectType::SYMBOL;
    }
    bool is_keyword() const {
        return type == ObjectType::SYMBOL && symbol_obj.value.starts_with_colon();
    }
    bool is_pair() const {
        return type == ObjectType::PAIR;
    }
    bool is_dotted_syntax();
    bool is_pointer() const {
        return type == ObjectType::POINTER;
    }
    bool is_native_ref() const {
        return type == ObjectType::NATIVE_REF;
    }
    bool is_array() const {
        return type == ObjectType::ARRAY;
    }
    bool is_null() const {
        return type == ObjectType::EMPTY_LIST;
    }
    bool is_not_null() const {
        return type != ObjectType::EMPTY_LIST;
    }
    bool is_list() const {
        return is_null() || is_pair();
    }
    bool is_lambda() const {
        return type == ObjectType::LAMBDA;
    }
    bool is_macro() const {
        return type == ObjectType::MACRO;
    }
    bool is_vector() const {
        return type == ObjectType::ARRAY;
    }
    bool is_hash_table() const {
        return type == ObjectType::STRING_HASH_TABLE;
    }
    bool is_env() const {
        return type == ObjectType::ENVIRONMENT;
    }
    bool is_reader() const {
        return type == ObjectType::READER;
    }
    bool is_static_buffer() const {
        return type == ObjectType::STATIC_BUFFER;
    }
    bool is_buffer_writer() const {
        return type == ObjectType::STATIC_WRITER;
    }
    bool is_primitive() const {
        return type == ObjectType::PRIMITIVE;
    }
    bool is_special_form() const {
        return type == ObjectType::SPECIAL_FORM;
    }
    bool is_callable() const {
        return type == ObjectType::SPECIAL_FORM || type == ObjectType::PRIMITIVE;
    }
    bool is_boolean() const {
        return is_symbol() && (as_symbol() == "#t" || as_symbol() == "#f");
    }

    // Evaluates the truthiness of an object. Since the Object class lacks access
    // to the Symbol Table, it must perform string comparisons, which is inefficient.
    // For better performance, the Interpreter uses its own 'truthy()' method,
    // which compares pre-interned symbols directly.
    bool as_boolean() const {
        if (is_null())
            return false;
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
    bool truthy(InternedSymbolPtr false_symbol) const {
        // Ложь — это если объект является пустым списком ИЛИ символом #f
        if (is_null())
            return false;
        return !(is_symbol() && as_symbol().name_ptr == false_symbol.name_ptr);
    }

    // Value access with type checking
    char                               as_char() const;
    IntType                            as_integer() const;
    FloatType                          as_float() const;
    StringObject                      *as_string() const;
    ArrayObject                       *as_array() const;
    HashTableObject                   *as_hash_table() const;
    MacroObject                       *as_macro() const;
    LambdaObject                      *as_lambda() const;
    EnvironmentObject                 *as_env() const;
    ReaderObject                      *as_reader() const;
    Pointer                           *as_pointer() const;
    NativeRef                         *as_native_ref() const;
    HeapObject                        *as_heap_object() const;
    const IntegerObject               &as_integer_obj() const;
    const InternedSymbolPtr           &as_symbol() const;
    std::shared_ptr<EnvironmentObject> as_env_ptr() const;
    std::string                        to_std_string() const;
    uint32_t                           as_crc32() const;
    std::vector<Object>                as_c_vector() const;
    std::vector<std::string>           as_c_vector_of_strings() const;
    PairObject                        *as_pair() const;

    template <typename T> std::shared_ptr<T> as_native_ref() const {
        // 1. Проверяем, что объект вообще является нативной ссылкой (инкапсулированным указателем)
        if (!heap_obj) {
            throw std::runtime_error("as_native_ref called on the object with heap_obj null " +
                                     object_type_to_string(type) + " " + print());
            return nullptr;
        }

        // 2. Извлекаем базовый указатель на HeapObject (или твой базовый класс для нативов)
        // Предполагаем, что m_data.heap_obj хранит shared_ptr
        auto base_ptr = heap_obj;

        // 3. Пытаемся безопасно привести к целевому типу T
        auto casted_ptr = std::dynamic_pointer_cast<T>(base_ptr);

        return casted_ptr;
    }

    bool operator==(const Object &other) const;
    bool operator!=(const Object &other) const {
        return !(*this == other);
    }
    static void for_each_in_list(const Object &list, const std::function<void(const Object &)> &f);

  private:
    void                throw_type_error(const std::string &expected) const;
    static SymbolTable *s_table; // Просто статический указатель
};

// Now define PairObject AFTER Object
class PairObject : public HeapObject {
  public:
    Object car;
    Object cdr;
    PairObject() = default;
    PairObject(const Object &car, const Object &cdr) : car(car), cdr(cdr) {}
    ~PairObject() override = default;

    std::string print() const override;
    Object      inspect() const override;

    int lenght() {
        int  count = 1;
        auto lst = cdr;
        while (lst.is_pair()) {
            count++;
            lst = lst.as_pair()->cdr;
        }
        return count;
    }

    Object make_step_accessor(const Object &key) override {
        if (key.is_integer()) {
            int index = key.as_integer();
            if (index < 0)
                return Object::make_none();

            auto current = this;

            // Шагаем по списку
            for (int i = 0; i < index; ++i) {
                Object next = current->cdr; // Предполагаю, что поле называется cdr
                if (next.is_pair()) {
                    current = next.as_pair();
                } else {
                    // Список закончился раньше, чем мы дошли до нужного индекса
                    return Object::make_none();
                }
            }

            return current->car; // Предполагаю, что поле называется car
        }
        return Object::make_none();
    }

    std::string class_name() const override {
        return "PairObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::PAIR);
    }
};

class StringObject : public HeapObject {
  public:
    std::string data;
    explicit StringObject(std::string text) : data(std::move(text)) {}
    ~StringObject() override = default;

    int length() const {
        return data.length();
    }
    bool empty() const {
        return data.empty();
    }

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

    std::string class_name() const override {
        return "StringObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::STRING);
    }
    // Неявное преобразование в std::string
    operator std::string() const {
        return data;
    }

    operator const char *() const {
        return data.c_str();
    }
};

class ArrayObject : public HeapObject {
  public:
    std::vector<Object> data;

    explicit ArrayObject(std::vector<Object> elements) : data(std::move(elements)) {}
    ~ArrayObject() override = default;

    int size() {
        return data.size();
    }

    Object &get(int index) {
        return data[index];
    }
    void set(int index, Object value) {
        data[index] = value;
    }

    const Object &operator[](size_t idx) const {
        return data.at(idx);
    }
    Object &operator[](size_t idx) {
        return data.at(idx);
    }

    std::string print() const override {
        std::string result = "#(";
        if (data.empty()) {
            return result + ")";
        }
        for (const auto &obj : data) {
            result += obj.print() + " ";
        }
        result.pop_back(); // remove last space
        return result + ")";
    }

    Object inspect() const override;

    Object make_step_accessor(const Object &key) override {
        if (key.is_integer()) {
            int index = key.as_integer();

            // Защита от выхода за границы
            if (index >= 0 && index < static_cast<int>(data.size())) {
                return data[index];
            }
            return Object::make_none();
        }

        // Можно добавить свойство 'length' для массива
        if (key.is_symbol() && key.to_std_string() == "length") {
            return Object::make_integer(data.size());
        }

        return Object::make_none();
    }
    // has methods get_at and set_at?

    bool is_table() const override {
        return true;
    }

    Object get_at(const Object &key) override {
        if (key.is_integer()) {
            int index = key.as_integer();
            if (index >= 0 && index < static_cast<int>(data.size())) {
                return data[index];
            }
        }
        return Object::make_none();
    }

    void set_at(const Object &key, const Object &val) override {
        if (key.is_integer()) {
            int index = key.as_integer();
            if (index >= 0 && index < static_cast<int>(data.size())) {
                data[index] = val;
            }
        }
    }
    std::string class_name() const override {
        return "ArrayObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::ARRAY);
    }
};

class HashTableObject : public HeapObject {
  public:
    std::unordered_map<std::string, Object> data;
    Object                                  type;

    HashTableObject() = default;
    HashTableObject(int size = 16) : data(size), type() {};
    HashTableObject(Object type, int size = 16) : data(size), type(type) {};

    ~HashTableObject() override = default;

    std::string print() const override {
        // Короткий системный принт: #<hash-table size:5>
        if (!type.is_none()) {
            return fmt::format("#<hash-table type:{} size:{}>", type.print(), data.size());
        }
        return fmt::format("#<hash-table size:{}>", data.size());
    }

    std::string print_long() const {
        std::string result = "{";
        for (const auto &kv : data) {
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

    Object      inspect() const override;
    std::string class_name() const override {
        return "HashTableObject";
    }
    std::string type_name() const override {
        if (!type.is_none()) {
            return fmt::format("{}::{}>", object_type_to_string(ObjectType::STRING_HASH_TABLE),
                               type.print());
        }
        return object_type_to_string(ObjectType::STRING_HASH_TABLE);
    }
    // Метод получения: возвращает ссылку на объект.
    // Если ключа нет, unordered_map создаст объект по умолчанию.
    Object &get(const std::string &key) {
        return data[key];
    }

    // Метод установки: записывает значение и возвращает ссылку на обновленное место.
    Object &set(const std::string &key, Object value) {
        data[key] = std::move(value); // используем move для эффективности
        return data[key];
    }

    // Доступ по строковому ключу (неконстантный):
    // стандартное поведение для ассоциативных контейнеров.
    Object &operator[](const std::string &key) {
        return data[key];
    }

    // Доступ по индексу (size_t):
    // В unordered_map нет прямого доступа по индексу, как в векторе.
    // Если это необходимо, используем итераторы (но помни, что порядок не гарантирован).
    const Object &operator[](size_t idx) const {
        if (idx >= data.size()) {
            throw std::out_of_range("HashTable index out of bounds");
        }
        auto it = data.begin();
        std::advance(it, idx);
        return it->second;
    }

    Object make_step_accessor(const Object &key) override {
        if (key.is_symbol() || key.is_string()) {
            auto skey = key.to_std_string();
            return data[skey];
        }
        return Object::make_none();
    }
    // has methods get_at and set_at?
    bool is_table() const override {
        return true;
    }

    Object get_at(const Object &key) override {
        if (key.is_string()) {
            return data[key.to_std_string()];
        }
        return Object::make_none();
    }

    void set_at(const Object &key, const Object &val) override {
        if (key.is_string()) {
            data[key.to_std_string()] = val;
        }
    }
};

template <typename T> class InternedPtrMap {
    friend class EnvironmentPrettyPrinter;

  private:
    struct Entry {
        const char *key = nullptr;
        T           value;
    };

    std::vector<Entry> m_entries;

  public:
    InternedPtrMap(const InternedPtrMap &) = delete;
    InternedPtrMap &operator=(const InternedPtrMap &) = delete;
    InternedPtrMap() {
        clear();
    }

    int size() const {
        return m_entries.size();
    }
    const std::vector<Entry> &get_all_entries() const {
        return m_entries;
    }

    T *lookup(InternedSymbolPtr str) {
        if (m_entries.size() < 10) {
            for (auto &e : m_entries) {
                if (e.key == str.name_ptr) { // ← Сравниваем указатели!
                    return &e.value;
                }
            }
            return nullptr;
        }
        uint32_t hash =
            util::compute_crc32(str.name_ptr, sizeof(const char *)); // ← Используем name_ptr

        // probe
        for (uint32_t i = 0; i < m_entries.size(); i++) {
            uint32_t slot_addr = (hash + i) & m_mask;
            auto    &slot = m_entries[slot_addr];
            if (!slot.key) {
                return nullptr;
            } else {
                if (slot.key != str.name_ptr) { // ← Сравниваем указатели!
                    continue;
                }
                return &slot.value;
            }
        }
        ASSERT_NOT_REACHED();
    }

    void set(InternedSymbolPtr ptr, const T &obj) {
        uint32_t hash =
            util::compute_crc32(ptr.name_ptr, sizeof(const char *)); // ← Используем name_ptr

        // probe
        for (uint32_t i = 0; i < m_entries.size(); i++) {
            uint32_t slot_addr = (hash + i) & m_mask;
            auto    &slot = m_entries[slot_addr];
            if (!slot.key) {
                // not found, insert!
                slot.key = ptr.name_ptr; // ← Сохраняем указатель!
                slot.value = obj;
                m_used_entries++;
                if (m_used_entries >= m_next_resize) {
                    resize();
                }
                return;
            } else {
                if (slot.key == ptr.name_ptr) { // ← Сравниваем указатели!
                    slot.value = obj;
                    return;
                }
            }
        }
        ASSERT_NOT_REACHED();
    }

    void clear() {
        m_entries.clear();
        m_power_of_two_size = 3; // 2 ^ 3 = 8
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
        for (const auto &old_entry : m_entries) {
            if (old_entry.key) {
                bool     done = false;
                uint32_t hash = util::compute_crc32(old_entry.key, sizeof(const char *));
                for (uint32_t i = 0; i < new_entries.size(); i++) {
                    uint32_t slot_addr = (hash + i) & m_mask;
                    auto    &slot = new_entries[slot_addr];
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
    int                    m_power_of_two_size = 0;
    int                    m_used_entries = 0;
    int                    m_next_resize = 0;
    uint32_t               m_mask = 0;
    static constexpr float kMaxUsed = 0.7;
};

class SymbolTable {
  public:
    struct TypeSymbols {
        Object kw_undefined;
        Object kw_optional;
        Object kw_key;
        Object kw_rest;
        Object sym_true;
        Object sym_false;
        Object type_null;
        Object type_special_form;
        Object type_primitive;
        Object type_int;
        Object type_float;
        Object type_char;
        Object type_symbol;
        Object type_string;
        Object type_pair;
        Object type_array;
        Object type_hash_table;
        Object type_lambda;
        Object type_macro;
        Object type_environment;
        Object type_reader;
        Object type_none;
        Object type_pointer;
        Object type_native_ref;
        Object type_static_buffer;
        Object type_static_writer;

        Object true_or_false(bool val) {
            return val ? sym_true : sym_false;
        }
    } core;
    void   init_core_symbols();
    Object object_type_to_symbol(ObjectType type);

  public:
    SymbolTable(const SymbolTable &) = delete;
    SymbolTable &operator=(const SymbolTable &) = delete;
    SymbolTable();
    ~SymbolTable();

    InternedSymbolPtr intern(const char *str);
    Object            make_symbol(const char *name);
    Object            make_keyword(const char *name);
    Object            make_symbol(std::string name);
    Object            make_keyword(std::string name);

    // Метод для итерации по символам
    template <typename F> void for_each_symbol(F func) const {
        for (const auto &entry : m_entries) {
            if (entry.name) {
                func(InternedSymbolPtr{entry.name});
            }
        }
    }

    size_t get_symbol_count() const {
        return m_used_entries;
    }

  private:
    void resize();
    int  m_power_of_two_size = 0;
    struct Entry {
        uint32_t    hash = 0;
        const char *name = nullptr;
    };
    std::vector<Entry>     m_entries;
    int                    m_used_entries = 0;
    int                    m_next_resize = 0;
    uint32_t               m_mask = 0;
    static constexpr float kMaxUsed = 0.7;
};

using EnvironmentMap = InternedPtrMap<Object>;

class EnvironmentObject : public HeapObject {
  public:
    std::string                        name;
    std::shared_ptr<EnvironmentObject> parent_env;
    EnvironmentMap                     vars;
    bool                               is_function;
    Object                             meta_data; // metadata от компилятора или интерпретатора

    EnvironmentObject() = default;
    EnvironmentObject(std::shared_ptr<EnvironmentObject> parent) : parent_env(std::move(parent)) {}
    ~EnvironmentObject() override = default;

    int size() const {
        return vars.size();
    }

    Object *find(const char *n, SymbolTable *st) {
        return vars.lookup(st->intern(n));
    }

    Object *find(InternedSymbolPtr ptr) {
        return vars.lookup(ptr);
    }

    static Object make_new() {
        Object obj;
        obj.type = ObjectType::ENVIRONMENT;
        obj.heap_obj = std::make_shared<EnvironmentObject>();
        return obj;
    }

    static Object make_new(std::string                        name,
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
        return fmt::format("#<env {} parent:{} @{:p}>", name.empty() ? "anonymous" : name,
                           parent_env ? parent_env->name : "none", (void *)this);
    }

    Object inspect() const override;

    std::string class_name() const override {
        return "EnvironmentObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::ENVIRONMENT);
    }

    std::shared_ptr<EnvironmentObject> function_env() {
        if (is_function)
            return std::static_pointer_cast<EnvironmentObject>(shared_from_this());
        auto current = parent_env;
        while (current) {
            if (current->is_function)
                return current;
            current = current->parent_env;
        }
        return std::static_pointer_cast<EnvironmentObject>(shared_from_this());
    }

    void add_meta(Object key, Object meta) {
        if (meta_data.is_none())
            meta_data = Object::make_null();
        meta_data = Object::make_pair(Object::make_pair(key, meta), meta_data);
    }

    void add_meta(std::string key, Object meta) {
        add_meta(Object::make_symbol(key), meta);
    }

    Object get_metadata(Object &key) const {
        Object result = Object::make_none();

        // Вероятно, for_each_in_list принимает const Object&
        Object::for_each_in_list(meta_data, [&](const Object &pair) {
            if (pair.is_pair() && pair.as_pair()->car == key) {
                result = pair.as_pair()->cdr;
                return false; // прервать поиск
            }
            return true; // продолжить поиск
        });

        return result;
    }

    Object get_metadata() const {
        return meta_data;
    }

    bool is_metadata_set() {
        return !meta_data.is_none();
    }
};

// Аргументы функций
struct Arguments {
    std::vector<Object>           unnamed;
    std::map<std::string, Object> named;
    Object                        rest;

    Object inspect() const;

    Object get_named(const std::string &name, const Object &default_value) {
        auto it = named.find(name);
        return it != named.end() ? it->second : default_value;
    }

    Object get_named(const std::string &name) {
        return named.at(name);
    }

    bool has_named(const std::string &name) {
        return named.find(name) != named.end();
    }

    // Проверяем, есть ли элементы в rest
    bool has_rest() const {
        // В Лиспе rest есть, если это не пустой список (null)
        // и не отсутствие объекта (none)
        return !rest.is_none() && !rest.is_null();
    }
    bool rest_empty() const {
        return !has_rest();
    }
    // Получаем размер списка rest (O(n))
    int rest_size() const {
        int    size = 0;
        Object current = rest;
        while (current.is_pair()) {
            size++;
            current = current.as_pair()->cdr;
        }
        return size;
    }

    // Получаем элемент по индексу (O(n))
    Object get_rest_at(const int index) {
        Object current = rest;
        int    i = 0;

        while (current.is_pair()) {
            if (i == index) {
                return current.as_pair()->car;
            }
            i++;
            current = current.as_pair()->cdr;
        }

        // Если индекс вне диапазона, можно вернуть None или бросить ошибку
        return Object::make_none();
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
    std::string name{};
    bool        is_optional;     // или просто проверка has_default
    Object      default_value{}; // NIL по умолчанию для опциональных
};
/**
 * @brief Спецификация аргументов функции (lambda-list).
 * * Описывает структуру ожидаемых входных данных, разделяя их на категории
 * согласно стандартам Lisp (позиционные, опциональные, ключевые и rest-аргументы).
 */
struct ArgumentSpec {
    /** @brief Разрешить использование ключевых слов (&key), например: :mode 'fast. */
    bool keys = true;
    /** @brief Разрешить неограниченное количество неименованных аргументов (для встроенных
     * функций). */
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

    ArgumentSpec() : keys(false), varargs(false) {}
    ArgumentSpec(bool keys, bool varargs) : keys(keys), varargs(varargs) {}

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

    Object to_object() const;
    Object inspect() const;

    const PositionalArg &operator[](size_t index) const {
        if (index >= unnamed.size())
            throw std::out_of_range("ArgumentSpec index out of range");
        return unnamed[index];
    }

    const PositionalArg *get_unnamed_spec(const std::string &name) const {
        for (const auto &arg : unnamed) {
            if (arg.name == name)
                return &arg;
        }
        return nullptr;
    }
    /**
     * @brief Проверяет, определен ли позиционный аргумент (обязательный или опциональный) с данным
     * именем.
     * @param name Имя аргумента для поиска.
     * @return true, если аргумент найден в списке позиционных параметров.
     */
    bool has_unnamed(const std::string &name) const {
        // Используем std::find_if для поиска в векторе структур
        return std::any_of(unnamed.begin(), unnamed.end(),
                           [&name](const PositionalArg &arg) { return arg.name == name; });
    }

    bool has_named(const std::string &name) const {
        return named.find(name) != named.end();
    }

    const NamedArg &get_named(const std::string &name) const {
        auto it = named.find(name);
        if (it == named.end())
            throw std::out_of_range("Named argument not found: " + name);
        return it->second;
    }

    std::string print() const;
    std::string print_full(size_t max_len = 512, size_t max_arg_len = 64) const;

    static ArgumentSpec create(const std::vector<std::string>      &required,
                               const std::map<std::string, Object> &optional = {},
                               const std::map<std::string, Object> &keys = {},
                               const std::string                   &rest_name = "");
};

class LambdaObject : public HeapObject {
  public:
    std::string                        name;
    std::shared_ptr<EnvironmentObject> parent_env;
    Object                             body;
    ArgumentSpec                       args;

    LambdaObject() = default;
    ~LambdaObject() override = default;

    static Object make_new() {
        Object obj;
        obj.type = ObjectType::LAMBDA;
        obj.heap_obj = std::make_shared<LambdaObject>();
        return obj;
    }

    std::string print() const override {
        return name.empty() ? "#<unnamed lambda>" : "#<lambda " + name + ">";
    }

    Object inspect() const override;

    std::string class_name() const override {
        return "LambdaObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::LAMBDA);
    }
};

class MacroObject : public HeapObject {
  public:
    std::string                        name;
    std::shared_ptr<EnvironmentObject> parent_env;
    Object                             body;
    ArgumentSpec                       args;

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

    std::string class_name() const override {
        return "MacroObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::MACRO);
    }
};

class ReaderObject : public HeapObject {
  public:
    // Передаем указатель на активный поток разбора
    TextStream *ts = nullptr;

    explicit ReaderObject(TextStream *stream) : ts(stream) {}
    ~ReaderObject() override = default;

    // peek-char: смотрим символ через твой ts->peek()
    Object peek_char() const;

    // read-char: извлекаем символ через твой ts->read()
    Object read_char();

    // skip-whitespace: используем твой метод
    void skip_whitespace();

    // Проверка на конец файла
    bool        is_eof() const;
    std::string print() const override;
    Object      inspect() const override;

    std::string class_name() const override {
        return "ReaderObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::READER);
    }
};

class Pointer : public HeapObject {
  public:
    void       *m_ptr;  // Прямой адрес в памяти (base_addr + offset)
    std::string m_type; // Метаданные (как именно читать этот кусок памяти)

    Pointer() : m_ptr(nullptr), m_type("undefined") {}
    Pointer(void *ptr) : m_ptr(ptr), m_type("void") {}
    Pointer(void *ptr, std::string type) : m_ptr(ptr), m_type(type) {}

    virtual std::string get_type_name() const {
        return m_type;
    };

    Object         make_step_accessor(const Object &key) override;
    Object         get_at(const Object &key) override;
    void           set_at(const Object &key, const Object &val) override;
    virtual Object get();
    virtual void   set(const Object &val);
    virtual Object deref() {
        return get();
    }
    virtual void *resolve_addr() const {
        return m_ptr;
    }

    std::string print() const override;
    Object      inspect() const override;

    std::string class_name() const override {
        return "Pointer";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::POINTER);
    }
};

// 1. Предварительное объявление
class Interpreter;

// 2. Типы указателей на методы Интерпретатора
using SpecialFormMethod = Object (Interpreter::*)(const Object &form, const Object &rest,
                                                  const std::shared_ptr<EnvironmentObject> &env);
using BuiltinFormMethod = Object (Interpreter::*)(const Object &form, Arguments &args,
                                                  const std::shared_ptr<EnvironmentObject> &env);

class CallableObject : public HeapObject {
  public:
    virtual ~CallableObject() {}
    // Убираем виртуальный call, чтобы класс не был абстрактным
    virtual bool is_special() const = 0;
};

struct SpecialFormObject : public CallableObject {
    SpecialFormMethod method;
    ArgumentSpec      specs;
    std::string       name;

    // Явный конструктор
    SpecialFormObject(SpecialFormMethod m, ArgumentSpec *s, std::string name)
        : method(m), specs(false, true), name(name) {
        if (s)
            specs = *s;
    }

    bool is_special() const override {
        return true;
    }

    std::string print() const override {
        return fmt::format("#<special-form {}>", name);
    }

    Object inspect() const override;

    std::string class_name() const override {
        return "SpecialFormObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::SPECIAL_FORM);
    }
};

struct BuiltinFunctionObject : public CallableObject {
    BuiltinFormMethod method;
    ArgumentSpec      specs;
    std::string       name;

    BuiltinFunctionObject(BuiltinFormMethod m, ArgumentSpec *s, std::string name)
        : method(m), specs(false, true), name(name) {
        if (s)
            specs = *s;
    }

    bool is_special() const override {
        return false;
    }

    std::string print() const override {
        return fmt::format("#<primitive-procedure {}>", name);
    }

    Object inspect() const override;

    std::string class_name() const override {
        return "BuiltinFunctionObject";
    }
    std::string type_name() const override {
        return object_type_to_string(ObjectType::PRIMITIVE);
    }
};

Object build_list(std::vector<Object> &&objects);
Object build_list(const std::vector<Object> &objects);

} // namespace script

namespace std {
template <> struct hash<script::InternedSymbolPtr> {
    size_t operator()(const script::InternedSymbolPtr &s) const noexcept {
        // Поскольку символы интернированы, адрес указателя
        // сам по себе является отличным уникальным хешем.
        return std::hash<const char *>{}(s.name_ptr);
    }
};
} // namespace std
