#pragma once

#include "Object.hpp"
#include "TextDb.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace script {

class Interpreter;
struct ListBuilder {
    Object                      head;
    std::shared_ptr<PairObject> prev_tail;
    std::shared_ptr<PairObject> tail;
    int                         size = 0;

    // Конструкторы
    ListBuilder() {
        head = Object::make_null();
    }

    // Твой старый добрый push_back (rvalue)
    std::shared_ptr<PairObject> push_back(Object &&o) {
        size++;
        std::shared_ptr<PairObject> next =
            std::make_shared<PairObject>(std::move(o), Object::make_null());
        if (!tail) {
            head.type = ObjectType::PAIR;
            head.heap_obj = next;
        } else {
            tail->cdr.type = ObjectType::PAIR;
            tail->cdr.heap_obj = next;
            prev_tail = tail;
        }
        tail = next;
        return tail;
    }

    // ВАЖНО: Добавляем версию для lvalue (копирование)
    // Это исправит ошибку "cannot bind rvalue reference... to lvalue"
    std::shared_ptr<PairObject> push_back(const Object &o) {
        Object copy = o;
        return push_back(std::move(copy));
    }

    // Исправленный push_kv: убираем лишний &, если symbols уже ссылка
    void push_kv(const char *key_name, Object value) {
        push_back(Object::make_keyword(key_name));
        push_back(std::move(value));
    }

    Object pop_back() {
        if (!tail)
            return Object::make_null(); // Защита от пустого списка

        Object obj = tail->car;

        // Откатываем tail назад
        tail = std::move(prev_tail);
        size--;

        // Если список стал пустым после удаления
        if (!tail) {
            head = Object::make_null();
        } else {
            // Зачищаем ссылку на удаленный элемент в новом хвосте
            tail->cdr = Object::make_null();
        }

        return obj;
    }
    Object build() {
        // Закрываем список нулем только если cdr всё еще пустой.
        // Если там уже что-то есть (от точки), значит список уже завершен.
        if (tail && tail->cdr.is_null()) {
            tail->cdr = Object::make_null();
        } else if (!tail) {
            head = Object::make_null();
        }
        return head;
    }

    // --- Fluent API Методы ---

    ListBuilder &add(Object o) {
        push_back(std::move(o));
        return *this;
    }

    ListBuilder &add_key_value(std::string key, Object value) {
        add_keyword(key);
        add(value);
        return *this;
    }

    ListBuilder &add(char c) {
        return add(Object::make_char(c));
    }

    ListBuilder &add_boolean(const bool value) {
        return add(value ? Object::make_symbol("#t") : Object::make_symbol("#f"));
    }

    ListBuilder &add_symbol(const std::string &name) {
        return add(Object::make_symbol(name));
    }

    ListBuilder &add_keyword(const std::string &name) {
        // Если у тебя ключи — это символы начинающиеся с ':', используй make_keyword
        return add(Object::make_keyword(name.c_str()));
    }

    ListBuilder &add_integer(int64_t val) {
        return add(Object::make_integer(val));
    }

    ListBuilder &add_float(double val) {
        return add(Object::make_float(val));
    }

    ListBuilder &add_string(const std::string &val) {
        return add(Object::make_string(val));
    }

    ListBuilder &add_native_ref(std::shared_ptr<HeapObject> ptr) {
        return add(Object::make_native_ref(ptr));
    }
};

} // namespace script
