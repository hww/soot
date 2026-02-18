#include "common/sooti/Object.hpp"
#include "common/CommonTypes.hpp"
#include "common/sooti/Errors.hpp"
#include "common/sooti/ListBuilder.hpp"
#include "common/sooti/Reader.hpp"
#include "common/util/Crc32.hpp"

#include "fmt/format.h"
#include <assert.h>
#include <cstring>
#include <iostream>
#include <sstream>

namespace script {

// ============================================================================
// Type Strings
// ============================================================================

std::string object_type_to_string(ObjectType type) {
    switch (type) {
    case ObjectType::NONE:
        return "none";
    case ObjectType::EMPTY_LIST:
        return "null";
    case ObjectType::INT:
        return "int";
    case ObjectType::FLOAT:
        return "float";
    case ObjectType::CHAR:
        return "char";
    case ObjectType::SYMBOL:
        return "symbol";
    case ObjectType::STRING:
        return "string";
    case ObjectType::PAIR:
        return "pair";
    case ObjectType::ARRAY:
        return "array";
    case ObjectType::FUNCTION:
        return "function";
    case ObjectType::MACRO:
        return "macro";
    case ObjectType::ENVIRONMENT:
        return "environment";
    case ObjectType::HASH_TABLE:
        return "hash-table";
    case ObjectType::READER:
        return "reader";
    case ObjectType::POINTER:
        return "pointer";
    case ObjectType::NATIVE_OBJECT:
        return "native-obj";
    case ObjectType::PRIMITIVE:
        return "primitive";
    case ObjectType::SPECIAL_FORM:
        return "specialform";
    default:
        throw std::runtime_error(
            fmt::format("unknown object type {} in object_type_to_string", (int)type));
    }
}

// ============================================================================
// SymbolTable
// ============================================================================

SymbolTable *Object::s_table = nullptr;

SymbolTable *Object::get_symbol_table() {
    if (!s_table)
        s_table = new SymbolTable();
    return s_table;
}
SymbolTable &Object::symbol_table() {
    return *get_symbol_table();
}

SymbolTable::SymbolTable() {
    m_power_of_two_size = 1; // 2 ^ 1 = 2
    m_entries.resize(2);
    m_used_entries = 0;
    m_next_resize = (m_entries.size() * kMaxUsed);
    m_mask = 0b1;
    init_core_symbols();
}

SymbolTable::~SymbolTable() {
    for (auto &e : m_entries) {
        delete[] e.name;
    }
}

void SymbolTable::init_core_symbols() {
    core.kw_optional = make_symbol(":optional");
    core.kw_key = make_symbol(":key");
    core.kw_rest = make_symbol(":rest");
    core.sym_true = make_symbol("#t");
    core.sym_false = make_symbol("#f");
    // 2. Автоматическая инициализация карты типов
    // Мы проходим по всем значениям enum до MAX_TYPES
    for (int i = 0; i < (int)ObjectType::MAX_TYPES; ++i) {
        ObjectType type = static_cast<ObjectType>(i);

        // Берем строковое имя (например, "int", "string", "native-obj")
        std::string name = object_type_to_string(type);

        // Создаем символ и кладем его в массив по индексу типа
        core.type_to_symbol_map[i] = make_symbol(name);
    }
}
const Object &SymbolTable::object_type_to_symbol(const ObjectType type) const {
    return core.type_to_symbol_map[(int)type];
}

InternedSymbolPtr SymbolTable::intern(const char *str) {
    size_t   string_len = strlen(str);
    uint32_t hash = util::compute_crc32(str, string_len);

    // probe
    for (uint32_t i = 0; i < m_entries.size(); i++) {
        uint32_t slot_addr = (hash + i) & m_mask;
        auto    &slot = m_entries[slot_addr];
        if (!slot.name) {
            // not found, insert!
            slot.hash = hash;
            auto *name = new char[string_len + 1];
            memcpy(name, str, string_len + 1);
            slot.name = name;
            m_used_entries++;

            if (m_used_entries >= m_next_resize) {
                resize();
                return intern(str);
            }
            return {name};
        } else {
            if (slot.hash != hash) {
                continue; // bad hash
            }
            if (strcmp(slot.name, str) != 0) {
                continue; // bad name
            }
            return {slot.name};
        }
    }

    // should be impossible to reach.
    ASSERT_NOT_REACHED();
}

void SymbolTable::resize() {
    m_power_of_two_size++;
    m_mask = (1U << m_power_of_two_size) - 1;

    std::vector<Entry> new_entries(m_entries.size() * 2);
    for (const auto &old_entry : m_entries) {
        if (old_entry.name) {
            bool done = false;
            for (uint32_t i = 0; i < new_entries.size(); i++) {
                uint32_t slot_addr = (old_entry.hash + i) & m_mask;
                auto    &slot = new_entries[slot_addr];
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

Object SymbolTable::make_symbol(const char *name) {
    Object obj;
    obj.type = ObjectType::SYMBOL;
    obj.symbol_obj.value = intern(name); // ← преобразуем в string
    return obj;
}

Object SymbolTable::make_keyword(const char *name) {
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

Object SymbolTable::make_symbol(std::string name) {
    return make_symbol(name.c_str());
}
Object SymbolTable::make_keyword(std::string name) {
    return make_keyword(name.c_str());
}

// ============================================================================
// Object Type
// ============================================================================

std::string Object::class_name() const {
    if (type == ObjectType::NATIVE_OBJECT && heap_obj.get() != nullptr)
        return heap_obj->class_name();
    return object_type_to_string(type);
}

Object Object::type_name_obj() const {
    if (type == ObjectType::NATIVE_OBJECT && heap_obj.get() != nullptr)
        return heap_obj->type_name_obj();
    return symbol_table().object_type_to_symbol(type);
}

bool Object::is_class_name(const Object &name) const {
    if (type == ObjectType::NATIVE_OBJECT && heap_obj.get() != nullptr)
        return heap_obj->is_class_name(name);
    return name == symbol_table().object_type_to_symbol(type);
}

std::vector<Object> Object::to_vector() const {
    std::vector<Object> result;
    Object              current = *this;

    // Идем по цепочке cdr, пока не упремся в конец списка
    while (current.is_pair()) {
        auto pair = current.as_pair();
        result.push_back(pair->car); // Сохраняем текущий элемент
        current = pair->cdr;         // Переходим к следующей паре
    }

    // В GOAL/Lisp список должен заканчиваться на 'empty-list / 'nil.
    // Если в конце осталось что-то другое (не none) — это "точечная пара" (dotted pair).
    // Для define-extern это обычно не нужно, но для надежности проверим:
    if (!current.is_none() && !current.is_null()) {
        // Опционально: можно кинуть ошибку или добавить последний элемент,
        // если твоя логика это подразумевает.
        // result.push_back(current);
    }

    return result;
}

// ============================================================================
// SymbolTable
// ============================================================================

// Специализации fixed_to_string
template <> std::string fixed_to_string<FloatType>(FloatType x) {
    std::stringstream ss;
    ss << x;
    return ss.str();
}

template <> std::string fixed_to_string<char>(char x) {
    switch (x) {
    case '\n':
        return "#\\newline";
    case ' ':
        return "#\\space";
    case '\t':
        return "#\\tab";
    case '\r':
        return "#\\return";
    case '\0':
        return "#\\null";
    case '\b':
        return "#\\backspace";
    case 27:
        return "#\\escape"; // ESC символ
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

template <> std::string fixed_to_string<IntType>(IntType x) {
    return std::to_string(x);
}

template <> std::string fixed_to_string<InternedSymbolPtr>(InternedSymbolPtr x) {
    return x.name_ptr ? std::string(x.name_ptr) : "";
}

// ============================================================================
// ERRORS
// ============================================================================

void Object::throw_type_error(const std::string &expected) const {
    throw std::runtime_error("Type error: expected " + expected + ", got " +
                             object_type_to_string(type));
}

Object Object::step(const Object &key) const {
    // Для всего, что живет в куче (HeapObject, Cell, Buffer, Array, String)
    if (this->heap_obj) {
        return this->heap_obj->make_step_accessor(key);
    }

    throw std::runtime_error(
        fmt::format("Type {} does not support '->' operator", this->class_name()));
}

// ============================================================================
// Iterators
// ============================================================================

/*!
 * Iterate through elements of a goos list and apply the given function. Throw compiler error if the
 * list is invalid.
 */
void Object::for_each_in_list(const Object &list, const std::function<void(const Object &)> &f) {
    const Object *iter = &list;
    while (iter->is_pair()) {
        auto lap = iter->as_pair();
        f(lap->car);
        iter = &lap->cdr;
    }

    if (!iter->is_null()) {
        throw EvalException(list, fmt::format("Invalid list: {}", list.print()));
    }
}

// ============================================================================
// Object factory
// ============================================================================

// 1. Для оператора (-> base key)
// По умолчанию объект не дает в себя "зайти".
Object HeapObject::make_step_accessor(const Object &key) {
    (void)key;
    // Ошибку "Object is not navigable" должен бросать сам ИНТЕРПРЕТАТОР,
    // если после всех попыток он получил undefined.
    return Object::make_none();
}
// В HeapObject.cpp
Object HeapObject::get_at(const Object &key) {
    Object target = this->make_step_accessor(key);
    if (target.is_pointer())
        return target.as_pointer()->get();
    return Object::make_none();
}

void HeapObject::set_at(const Object &key, const Object &value) {
    Object target = this->make_step_accessor(key);
    if (target.is_pointer())
        target.as_pointer()->set(value);
}

// ============================================================================
// Object factory
// ============================================================================

Object Object::make_none() {
    Object obj;
    obj.type = ObjectType::NONE;
    return obj;
}

Object Object::make_heap_obj(std::shared_ptr<HeapObject> heap_object, ObjectType type) {
    Object obj;
    obj.type = type;
    obj.heap_obj = std::move(heap_object);
    return obj;
}

Object Object::make_heap_obj(std::shared_ptr<HeapObject> heap_object) {
    Object obj;
    obj.type = ObjectType::NATIVE_OBJECT;
    obj.heap_obj = std::move(heap_object);
    return obj;
}

Object Object::make_reader(TextStream *textStream) {
    Object obj;
    obj.type = ObjectType::READER;
    obj.heap_obj = std::make_shared<ReaderObject>(textStream);
    return obj;
}

Object Object::make_pointer(std::shared_ptr<Pointer> pointer) {
    Object obj;
    obj.type = ObjectType::POINTER;
    obj.heap_obj = std::move(pointer);
    return obj;
}

Object Object::make_pointer(void *raw_ptr, std::string type) {
    // Создаем НОВЫЙ объект ячейки в куче, который будет смотреть на raw_ptr
    auto pointer_shr = std::make_shared<Pointer>(raw_ptr, type);
    return make_pointer(std::move(pointer_shr));
}

template <> Object Object::make_number(FloatType value) {
    return Object::make_float(value);
}

template <> Object Object::make_number(IntType value) {
    return Object::make_integer(value);
}

Object Object::make_integer(IntType value) {
    Object obj;
    obj.type = ObjectType::INT;
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

Object Object::make_list(const std::vector<Object> &elements) {
    return build_list(elements);
}

InternedSymbolPtr Object::intern(const char *name) {
    if (get_symbol_table())
        return get_symbol_table()->intern(name);
    throw std::runtime_error("call set_symbol_table(...) before");
}

Object Object::make_symbol(const char *name) {
    if (get_symbol_table())
        return get_symbol_table()->make_symbol(name);
    throw std::runtime_error("call set_symbol_table(...) before");
}

Object Object::make_keyword(const char *name) {
    if (get_symbol_table())
        return get_symbol_table()->make_keyword(name);
    throw std::runtime_error("call set_symbol_table(...) before");
}

Object Object::make_string(const std::string &text) {
    Object obj;
    obj.type = ObjectType::STRING;
    obj.heap_obj = std::make_shared<StringObject>(text);
    return obj;
}

Object Object::make_pair(const Object &car, const Object &cdr) {
    Object obj;
    obj.type = ObjectType::PAIR;
    obj.heap_obj = std::make_shared<PairObject>(car, cdr);
    return obj;
}

Object Object::make_array(const std::vector<Object> &elements) {
    Object obj;
    obj.type = ObjectType::ARRAY;
    obj.heap_obj = std::make_shared<ArrayObject>(elements);
    return obj;
}

Object Object::make_vector(const std::vector<Object> &elements) {
    return make_array(elements); // Векторы и массивы - одно и то же
}

Object Object::make_hash_table(int size) {
    Object obj;
    obj.type = ObjectType::HASH_TABLE;
    obj.heap_obj = std::make_shared<HashTableObject>(size);
    return obj;
}
Object Object::make_hash_table(Object type_name, int size) {
    Object obj;
    obj.type = ObjectType::HASH_TABLE;
    obj.heap_obj = std::make_shared<HashTableObject>(type_name, size);
    return obj;
}
Object Object::make_function(const ArgumentSpec &args, const Object &body,
                             const std::shared_ptr<EnvironmentObject> &env) {
    Object obj = FunctionObject::make_new();
    auto   lambda = obj.as_function();
    lambda->args = args;
    lambda->body = body;
    lambda->parent_env = env;
    return obj;
}

Object Object::make_macro(const ArgumentSpec &args, const Object &body,
                          const std::shared_ptr<EnvironmentObject> &env) {
    Object obj = MacroObject::make_new();
    auto   macro = obj.as_macro();
    macro->args = args;
    macro->body = body;
    macro->parent_env = env;
    return obj;
}
// ============================================================================
// Predicates
// ============================================================================

bool Object::is_dotted_syntax() {
    if (type != ObjectType::PAIR)
        return false;

    auto tail = as_pair()->cdr;
    // Если в хвосте не другая пара и не конец списка (null)
    // значит это структура вида (field . value)
    return tail.type != ObjectType::PAIR && tail.type != ObjectType::EMPTY_LIST;
}

// ============================================================================
// Getters
// ============================================================================

// Value HeapObjects
PairObject *Object::as_pair() const {
    if (type != ObjectType::PAIR) {
        throw std::runtime_error("as_pair called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return dynamic_cast<PairObject *>(heap_obj.get());
}

EnvironmentObject *Object::as_env() const {
    if (type != ObjectType::ENVIRONMENT) {
        throw std::runtime_error("as_env called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return static_cast<EnvironmentObject *>(heap_obj.get());
}

std::shared_ptr<EnvironmentObject> Object::as_env_ptr() const {
    if (type != ObjectType::ENVIRONMENT) {
        throw std::runtime_error("as_env called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return std::dynamic_pointer_cast<EnvironmentObject>(heap_obj);
}

StringObject *Object::as_string() const {
    if (type != ObjectType::STRING) {
        throw std::runtime_error("as_string called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return static_cast<StringObject *>(heap_obj.get());
}

std::string Object::to_std_string() const {
    switch (type) {
    case ObjectType::STRING:
        // Используем ссылку, чтобы избежать лишнего копирования до возврата
        return static_cast<StringObject *>(heap_obj.get())->data;

    case ObjectType::SYMBOL:
        // Предполагаем, что у символа есть поле value (std::string)
        return symbol_obj.value;

    default:
        throw std::runtime_error("to_std_string called on a " + object_type_to_string(type) +
                                 print());
    }
}

FunctionObject *Object::as_function() const {
    if (type != ObjectType::FUNCTION) {
        throw std::runtime_error("as_lambda called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return static_cast<FunctionObject *>(heap_obj.get());
}

MacroObject *Object::as_macro() const {
    if (type != ObjectType::MACRO) {
        throw std::runtime_error("as_macro called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return static_cast<MacroObject *>(heap_obj.get());
}

IntType Object::as_integer() const {
    if (type != ObjectType::INT) {
        throw std::runtime_error("as_integer called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return integer_obj.value;
}

FloatType Object::as_float() const {
    if (type != ObjectType::FLOAT) {
        throw std::runtime_error("as_float called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return float_obj.value;
}

char Object::as_char() const {
    if (type != ObjectType::CHAR) {
        throw std::runtime_error("as_char called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return char_obj.value;
}

const InternedSymbolPtr &Object::as_symbol() const {
    if (type != ObjectType::SYMBOL) {
        throw std::runtime_error("as_symbol called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return symbol_obj.value;
}

ArrayObject *Object::as_array() const {
    if (type != ObjectType::ARRAY) {
        throw std::runtime_error("as_array called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return static_cast<ArrayObject *>(heap_obj.get());
}

HashTableObject *Object::as_hash_table() const {
    if (type != ObjectType::HASH_TABLE) {
        throw std::runtime_error("as_string_hash_table called on a " + object_type_to_string(type) +
                                 " " + print());
    }
    return dynamic_cast<HashTableObject *>(heap_obj.get());
}

ReaderObject *Object::as_reader() const {
    if (type != ObjectType::READER) {
        throw std::runtime_error("as_reader called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return dynamic_cast<ReaderObject *>(heap_obj.get());
}

std::vector<Object> Object::as_c_vector() const {
    if (!is_list())
        throw std::runtime_error("as_vector called on a " + object_type_to_string(type) + " " +
                                 print());
    std::vector<Object> result;
    Object              current = *this;
    while (current.is_pair()) {
        result.push_back(current.as_pair()->car);
        current = current.as_pair()->cdr;
    }
    return result;
}

std::vector<std::string> Object::as_c_vector_of_strings() const {
    if (!is_list())
        throw std::runtime_error("as_c_vector_of_strings called on a " +
                                 object_type_to_string(type) + " " + print());
    std::vector<std::string> result;
    Object                   current = *this;
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

const IntegerObject &Object::as_integer_obj() const {
    if (!is_integer())
        throw_type_error("integer");
    return integer_obj;
}

Pointer *Object::as_pointer() const {
    if (type != ObjectType::POINTER) {
        throw std::runtime_error("as_pointer called on a " + object_type_to_string(type) + " " +
                                 print());
    }
    return dynamic_cast<Pointer *>(heap_obj.get());
}

HeapObject *Object::as_heap_obj() const {
    if (heap_obj == nullptr) {
        throw std::runtime_error("as_heap_obj called on a " + object_type_to_string(type) + " " +
                                 print() + " heap_obj is null");
    }
    return dynamic_cast<HeapObject *>(heap_obj.get());
}

uint32_t Object::as_crc32() const {
    switch (type) {
    case ObjectType::NONE:
        return util::compute_crc32("none");
    case ObjectType::EMPTY_LIST:
        return util::compute_crc32("null");
    case ObjectType::INT:
        return util::compute_crc32(print());
    case ObjectType::FLOAT:
        return util::compute_crc32(print());
    case ObjectType::CHAR:
        return util::compute_crc32(print());
    case ObjectType::SYMBOL:
        return util::compute_crc32(print());
    default:
        if (is_heap_object() && heap_obj)
            return heap_obj->as_crc32();
        return util::compute_crc32("unknown");
    }
}

// ============================================================================
// CALLABLE
// ============================================================================

Object SpecialFormObject::inspect() const {
    ListBuilder lb;
    lb.add(Object::make_symbol("special-form"));
    // Если нужно, сюда можно добавить адрес метода для низкоуровневой отладки
    return lb.build();
}

Object BuiltinFunctionObject::inspect() const {
    ListBuilder lb;
    lb.add(Object::make_symbol("builtin-function"));

    // Добавляем информацию о спецификации аргументов
    lb.add(Object::make_symbol(":unamed-args"));
    lb.add(Object::make_integer(specs.unnamed_size()));

    lb.add(Object::make_symbol(":named-args"));
    lb.add(Object::make_integer(specs.named_size()));

    // Если есть флаг varargs, можно добавить и его
    if (specs.varargs) {
        lb.add(Object::make_symbol(":varargs"));
        lb.add(Object::symbol_table().core.true_or_false(true));
    }

    return lb.build();
}

// ============================================================================
//   OPERATORS
// ============================================================================

// Comparison
bool Object::operator==(const Object &other) const {
    if (type != other.type)
        return false;

    switch (type) {
    case ObjectType::NONE:
        return false;

    case ObjectType::STRING:
        return as_string()->data == other.as_string()->data;
    case ObjectType::INT:
        return integer_obj.value == other.integer_obj.value;
    case ObjectType::FLOAT:
        return float_obj.value == other.float_obj.value;
    case ObjectType::CHAR:
        return char_obj.value == other.char_obj.value;
    case ObjectType::SYMBOL:
        return symbol_obj.value.name_ptr == other.symbol_obj.value.name_ptr;

    case ObjectType::ENVIRONMENT:
    case ObjectType::FUNCTION:
    case ObjectType::MACRO:
    case ObjectType::READER:
    case ObjectType::PRIMITIVE:
    case ObjectType::SPECIAL_FORM:
        return heap_obj == other.heap_obj;

    case ObjectType::EMPTY_LIST:
        return true;

    case ObjectType::PAIR:
        return as_pair()->car == other.as_pair()->car && as_pair()->cdr == other.as_pair()->cdr;
    case ObjectType::ARRAY: {
        auto this_arr = dynamic_cast<ArrayObject *>(heap_obj.get());
        auto other_arr = dynamic_cast<ArrayObject *>(other.heap_obj.get());
        if (!this_arr || !other_arr)
            return false;
        return this_arr->data == other_arr->data;
    }

    case ObjectType::HASH_TABLE:
        return as_hash_table()->data == other.as_hash_table()->data;

    default:
        throw std::runtime_error("equality not implemented for " + print());
    }
}

/*!
 * Build a list of objects from a vector of objects.
 */
Object build_list(const std::vector<Object> &objects) {
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

Object build_list(std::vector<Object> &&objects) {
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
    ListBuilder lb{};

    // 1. Позиционные аргументы
    for (const auto &arg : unnamed) {
        lb.push_back(Object::make_symbol(arg.name.c_str()));
    }

    // 2. Именованные аргументы (Keyword arguments)
    if (!named.empty()) {
        lb.push_back(Object::make_keyword("key")); // Маркер &key
        for (const auto &[name, spec] : named) {
            if (spec.has_default) {
                // Если есть дефолт: (name default)
                ListBuilder entry{};
                entry.push_back(Object::make_symbol(name.c_str()));
                entry.push_back(spec.default_value);
                lb.push_back(entry.build());
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

    return lb.build();
}

ArgumentSpec ArgumentSpec::create(const std::vector<std::string>      &required,
                                  const std::map<std::string, Object> &optional,
                                  const std::map<std::string, Object> &keys,
                                  const std::string                   &rest_name) {
    ArgumentSpec spec;
    spec.varkeys = !keys.empty();
    spec.rest = rest_name;
    spec.varargs = false; // Мы явно задаем структуру

    // 1. Обязательные позиционные аргументы
    for (const auto &name : required) {
        PositionalArg arg;
        arg.name = name;
        arg.is_optional = false;
        spec.unnamed.push_back(arg);
    }

    // 2. Опциональные позиционные аргументы (с дефолтами)
    for (const auto &[name, default_val] : optional) {
        PositionalArg arg;
        arg.name = name;
        arg.is_optional = !default_val.is_none();
        arg.default_value = default_val;
        spec.unnamed.push_back(arg);
    }

    // 3. Ключевые аргументы (&key с дефолтами)
    for (const auto &[name, default_val] : keys) {
        NamedArg arg;
        arg.has_default = !default_val.is_none();
        arg.default_value = default_val;
        spec.named[name] = arg;
    }

    return spec;
}

// ============================================================================
// Memory Cell
// ============================================================================

// Вспомогательная функция для определения размера типа на лету (только примитивы)
static size_t get_primitive_size(const std::string &type) {
    if (type == "int8" || type == "uint8" || type == "byte")
        return 1;
    if (type == "int16" || type == "uint16")
        return 2;
    if (type == "int32" || type == "uint32" || type == "float")
        return 4;
    if (type == "int64" || type == "uint64" || type == "double" || type == "pointer")
        return 8;
    return 0;
}

Object Pointer::inspect() const {
    ListBuilder lb{};
    // Используем символ 'pointer' для идентификации в инспекции
    lb.add(type_name_obj());

    lb.push_kv("address", Object::make_integer((uintptr_t)m_ptr));
    lb.push_kv("type", Object::make_string(m_type));

    return lb.build();
}

std::string Pointer::print() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "#<pointer %s @ 0x%p>", m_type.c_str(), m_ptr);
    return std::string(buf);
}

Object Pointer::make_step_accessor(const Object &key) {
    if (key.is_integer()) {
        size_t element_size = get_primitive_size(m_type);
        if (element_size == 0) {
            // Если тип сложный (структура), шаг должен вычисляться в TypeSystem,
            // но на этом уровне мы просто двигаем указатель как по байтам.
            element_size = 1;
        }

        uint8_t *new_addr = (uint8_t *)m_ptr + (key.as_integer() * element_size);
        // Возвращаем новый указатель с тем же типом, но смещенным адресом
        return Object::make_pointer(new_addr, m_type);
    }

    throw std::runtime_error("Pointer step accessor requires an integer offset");
}

Object Pointer::get() {
    if (!m_ptr)
        return Object::make_none();

    if (m_type == "int8")
        return Object::make_integer(*(int8_t *)m_ptr);
    if (m_type == "uint8" || m_type == "byte")
        return Object::make_integer(*(uint8_t *)m_ptr);

    if (m_type == "int16")
        return Object::make_integer(*(int16_t *)m_ptr);
    if (m_type == "uint16")
        return Object::make_integer(*(uint16_t *)m_ptr);

    if (m_type == "int32")
        return Object::make_integer(*(int32_t *)m_ptr);
    if (m_type == "uint32")
        return Object::make_integer(*(uint32_t *)m_ptr);

    if (m_type == "int64")
        return Object::make_integer(*(int64_t *)m_ptr);
    if (m_type == "uint64")
        return Object::make_integer((int64_t)*(uint64_t *)m_ptr);

    if (m_type == "float")
        return Object::make_float(*(float *)m_ptr);
    if (m_type == "double")
        return Object::make_float((float)*(double *)m_ptr);

    if (m_type == "pointer")
        return Object::make_integer((uintptr_t)*(void **)m_ptr);

    if (m_type == "string") {
        char *str_ptr = *(char **)m_ptr;
        return str_ptr ? Object::make_string(str_ptr) : Object::make_null();
    }

    // Если тип не примитивный (например, "vector"), возвращаем сам Pointer.
    // Это позволит продолжить цепочку (-> ptr field)
    return Object::make_heap_obj(shared_from_this());
}

void Pointer::set(const Object &val) {
    if (!m_ptr)
        return;

    if (m_type == "int8") {
        *(int8_t *)m_ptr = (int8_t)val.as_integer();
        return;
    }
    if (m_type == "uint8" || m_type == "byte") {
        *(uint8_t *)m_ptr = (uint8_t)val.as_integer();
        return;
    }

    if (m_type == "int16") {
        *(int16_t *)m_ptr = (int16_t)val.as_integer();
        return;
    }
    if (m_type == "uint16") {
        *(uint16_t *)m_ptr = (uint16_t)val.as_integer();
        return;
    }

    if (m_type == "int32") {
        *(int32_t *)m_ptr = (int32_t)val.as_integer();
        return;
    }
    if (m_type == "uint32") {
        *(uint32_t *)m_ptr = (uint32_t)val.as_integer();
        return;
    }

    if (m_type == "int64") {
        *(int64_t *)m_ptr = (int64_t)val.as_integer();
        return;
    }
    if (m_type == "uint64") {
        *(uint64_t *)m_ptr = (uint64_t)val.as_integer();
        return;
    }

    if (m_type == "float") {
        *(float *)m_ptr = val.as_float();
        return;
    }
    if (m_type == "double") {
        *(double *)m_ptr = (double)val.as_float();
        return;
    }

    if (m_type == "pointer") {
        *(uintptr_t *)m_ptr = (uintptr_t)val.as_integer();
        return;
    }

    throw std::runtime_error("Unsupported memory write for type: " + m_type);
}

void Pointer::set_at(const Object &key, const Object &value) {
    // 1. Создаем временный указатель на нужный оффсет/поле
    Object target = this->make_step_accessor(key);

    // 2. Если шаг успешен, пишем значение по новому адресу
    if (target.is_pointer()) {
        target.as_pointer()->set(value);
    } else {
        throw std::runtime_error("Pointer::set_at: failed to resolve address for key " +
                                 key.print());
    }
}
Object Pointer::get_at(const Object &key) {
    // 1. Создаем временный указатель (аксессор) на поле или элемент массива
    // Это вычисляет новый адрес (m_ptr + offset) и определяет тип поля
    Object target = this->make_step_accessor(key);

    // 2. Проверяем, что шаг прошел успешно и мы получили объект-указатель
    if (target.is_pointer()) {
        // Разыменовываем (читаем значение по вычисленному адресу)
        return target.as_pointer()->get();
    }

    // Если шаг невозможен (нет такого поля/индекса), возвращаем undefined
    return Object::make_none();
}

// ============================================================================
//    PRINTS
// ============================================================================

/**
 * Вспомогательная функция для безопасного строкового представления объекта
 */
std::string truncate_obj(const Object &obj, size_t max_arg_len) {
    std::string s = obj.print(); // Используем существующий метод print объекта
    if (s.length() <= max_arg_len)
        return s;
    return s.substr(0, max_arg_len - 3) + "...";
}

std::string truncate_obj(const std::string &s, size_t max_arg_len) {
    if (s.length() <= max_arg_len)
        return s;
    return s.substr(0, max_arg_len - 3) + "...";
}

// String representations
std::string Object::print() const {
    switch (type) {
    case ObjectType::NONE:
        return "none";
    case ObjectType::EMPTY_LIST:
        return "null";
    case ObjectType::INT:
        return integer_obj.print();
    case ObjectType::FLOAT:
        return float_obj.print();
    case ObjectType::CHAR:
        return char_obj.print();
    case ObjectType::SYMBOL:
        return symbol_obj.print();
    default:
        if (is_heap_object())
            return heap_obj.get() != nullptr ? heap_obj->print() : "[invalid-heap-object]";
        else
            return "[unknown]";
    }
}
std::string Object::printc() const {
    // сырой формат например без "" для строки
    return is_heap_object() && heap_obj ? heap_obj->printc() : print();
}
std::string ArgumentSpec::print() const {
    // Вместо "ArgumentSpec: unnamed=2..." сделаем более сжатый системный вид
    return fmt::format("#<arg-spec u:{} n:{} r:{}{}{}>", unnamed.size(), named.size(),
                       rest.empty() ? "0" : "1", varkeys ? " +rest" : "",
                       varargs ? " +vararg" : "");
}

std::string ArgumentSpec::print_full(size_t max_len, size_t max_arg_len) const {

    std::stringstream ss;

    ss << "(";

    // 1. Позиционные аргументы (unnamed)
    if (!unnamed.empty()) {
        for (size_t i = 0; i < unnamed.size(); ++i) {
            ss << truncate_obj(unnamed[i].name, max_arg_len);
            if (i < unnamed.size() - 1)
                ss << " ";
        }
    }

    // 2. Ключевые аргументы (named)
    if (!named.empty()) {
        if (!unnamed.empty())
            ss << " ";
        ss << "&key ";
        bool first = true;
        for (const auto &[name, val] : named) {
            if (!first)
                ss << " ";
            if (val.default_value.is_none())
                ss << name;
            else
                ss << "(" << name << " " << truncate_obj(val.default_value, max_arg_len) << ")";
            first = false;
        }
    }

    // 3. Остаток (rest)
    if (!rest.empty()) {
        if (!unnamed.empty() || !named.empty())
            ss << " ";
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
    return fmt::format("#<args-invoked u:{} n:{} r:{}>", unnamed.size(), named.size(), rest_size());
}

std::string Arguments::print_full(size_t max_len, size_t max_arg_len) const {

    std::stringstream ss;

    ss << "(";

    // 1. Позиционные аргументы (unnamed)
    if (!unnamed.empty()) {
        for (size_t i = 0; i < unnamed.size(); ++i) {
            ss << truncate_obj(unnamed[i], max_arg_len);
            if (i < unnamed.size() - 1)
                ss << " ";
        }
    }

    // 2. Ключевые аргументы (named)
    if (!named.empty()) {
        if (!unnamed.empty())
            ss << " ";
        ss << "&key ";
        bool first = true;
        for (const auto &[name, val] : named) {
            if (!first)
                ss << " ";
            ss << ":" << name << " " << truncate_obj(val, max_arg_len);
            first = false;
        }
    }

    // 3. Остаток (rest)
    if (has_rest()) {
        if (!unnamed.empty() || !named.empty())
            ss << " ";

        ss << "&rest: ";

        Object current = rest;
        while (current.is_pair()) {
            auto *pair = current.as_pair();

            // Печатаем текущий элемент (car ячейки)
            ss << truncate_obj(pair->car, max_arg_len);

            // Если это не последняя ячейка в цепочке, добавляем пробел
            if (pair->cdr.is_pair()) {
                ss << " ";
            }

            // Переходим к следующей паре
            current = pair->cdr;
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

// ============================================================================
//   INSPECTORS
// ============================================================================

std::string Object::inspect_short() const {
    const int max_len = 64;
    // 1. Получаем S-expression инспекта
    Object info = this->inspect();

    // 2. Превращаем структуру в строку для отображения
    std::string str = info.print();

    if (str.size() <= max_len)
        return str;
    return str.substr(0, max_len - 3) + "...";
}

Object Object::inspect() const {
    switch (type) {
    case ObjectType::EMPTY_LIST:
        return Object::make_symbol("null");

    case ObjectType::INT: {
        ListBuilder lb{};
        lb.push_back(Object::make_symbol("integer"));
        lb.push_kv("value", *this);
        return lb.build();
    }

    case ObjectType::FLOAT: {
        ListBuilder lb{};
        lb.push_back(Object::make_symbol("float"));
        lb.push_kv("value", *this);
        return lb.build();
    }

    case ObjectType::SYMBOL: {
        ListBuilder lb{};
        lb.push_back(Object::make_symbol("symbol"));
        lb.push_kv("name", *this);
        return lb.build();
    }

    case ObjectType::NONE: {
        ListBuilder lb{};
        lb.push_back(Object::make_symbol("none"));
        return lb.build();
    }

    default:
        if (is_heap_object() && heap_obj) {
            return heap_obj->inspect();
        }
        return Object::make_symbol("error-unknown");
    }
}

Object PairObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("pair"));
    lb.push_kv("car", this->car);
    lb.push_kv("cdr", this->cdr);
    return lb.build();
}

Object StringObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("string"));
    lb.push_kv("value", Object::make_string(print().c_str()));
    lb.push_kv("length", Object::make_integer(data.length()));
    return lb.build();
}

template <typename T> Object FixedObject<T>::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol(type_as_string().c_str()));
    lb.push_kv("value", Object(value)); // Убрали & перед symbols
    return lb.build();
}

Object ArrayObject::inspect() const {
    ListBuilder lb{};

    // 1. Имя типа
    lb.push_back(Object::make_symbol("array"));

    // 2. Метаданные
    lb.push_kv("length", Object::make_integer(data.size()));
    lb.push_kv("address", Object::make_integer((int64_t)this));

    // 3. Содержимое (опционально, выводим первые 10 элементов, чтобы не заспамить консоль)
    ListBuilder elements_lb{};
    size_t      limit = std::min(data.size(), (size_t)10);
    for (size_t i = 0; i < limit; ++i) {
        elements_lb.push_back(data[i]);
    }

    if (data.size() > 10) {
        elements_lb.push_back(Object::make_symbol("..."));
    }

    lb.push_kv("data", elements_lb.build());

    return lb.build();
}

// Удалено дублирующееся определение HashTableObject::inspect. Оставили одно:
Object HashTableObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("hash-table"));
    lb.push_kv("size", Object::make_integer(data.size()));

    ListBuilder entries_lb{};
    for (const auto &[key, val] : data) {
        ListBuilder pair_lb{};
        pair_lb.push_back(Object::make_string(key.c_str()));
        pair_lb.push_back(val);
        entries_lb.push_back(pair_lb.build());
    }
    lb.push_kv("entries", entries_lb.build());
    return lb.build();
}

Object EnvironmentObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("environment"));
    lb.push_kv("name",
               name.empty() ? Object::make_symbol("anonymous") : Object::make_string(name.c_str()));

    if (parent_env) {
        lb.push_kv("parent", Object::make_string(parent_env->print().c_str()));
    }

    // Если InternedPtrMap не поддерживает итераторы, выводим только количество
    lb.push_kv("bindings-count", Object::make_integer(size()));
    lb.push_kv("address", Object::make_integer((int64_t)this));
    return lb.build();
}

Object FunctionObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("lambda"));
    lb.push_kv("name",
               name.empty() ? Object::make_symbol("anonymous") : Object::make_string(name.c_str()));
    lb.push_kv("args", args.to_object());
    lb.push_kv("body", body);
    return lb.build();
}

Object MacroObject::inspect() const {
    ListBuilder lb{};

    // 1. Заголовок типа
    lb.push_back(Object::make_symbol("macro"));

    // 2. Имя макроса (если есть)
    lb.push_kv("name",
               name.empty() ? Object::make_symbol("anonymous") : Object::make_string(name.c_str()));

    // 3. Спецификация аргументов
    // Вызываем to_object, который возвращает структуру аргументов (списки имён и т.д.)
    lb.push_kv("args", args.to_object());

    // 4. Тело макроса (исходный код)
    lb.push_kv("body", body);

    // 5. Адрес в памяти для отладки
    lb.push_kv("address", Object::make_integer((int64_t)this));

    return lb.build();
}

Object ReaderObject::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("reader"));
    lb.push_kv("line", Object::make_integer(ts ? ts->line_count : 0));
    return lb.build();
}

Object ArgumentSpec::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("argument-spec"));
    lb.push_kv("has-rest", rest.empty() ? Object::make_null() : Object::make_symbol("#t"));
    lb.push_kv("structure", this->to_object());
    return lb.build();
}

Object Arguments::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("arguments-instance"));

    ListBuilder u_list{};
    for (const auto &obj : unnamed)
        u_list.push_back(obj);
    lb.push_kv("unnamed", u_list.build());

    ListBuilder n_list{};
    for (const auto &[name, obj] : named) {
        n_list.push_kv(name.c_str(), obj); // Убрали & перед symbols
    }
    lb.push_kv("named", n_list.build());

    return lb.build();
}

} // namespace script
