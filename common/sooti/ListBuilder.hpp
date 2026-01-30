#pragma once

#include "Object.hpp"
#include "TextDb.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace script {

    class Interpreter;
    struct ListBuilder {
        Object head;
        std::shared_ptr<PairObject> prev_tail;
        std::shared_ptr<PairObject> tail;
        int size = 0;

        // Конструкторы
        ListBuilder() { head = Object::make_null(); }
        // Добавляем этот, чтобы lb{symbols} работало (даже если symbols не используется)
        ListBuilder(SymbolTable& /*symbols*/) { head = Object::make_null(); }

        // Твой старый добрый push_back (rvalue)
        std::shared_ptr<PairObject> push_back(Object&& o) {
            size++;
            std::shared_ptr<PairObject> next = std::make_shared<PairObject>(std::move(o), Object::make_null());
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
        std::shared_ptr<PairObject> push_back(const Object& o) {
            Object copy = o;
            return push_back(std::move(copy));
        }

        // Исправленный push_kv: убираем лишний &, если symbols уже ссылка
        void push_kv(SymbolTable& symbols, const char* key_name, Object value) {
            push_back(Object::make_keyword(key_name));
            push_back(std::move(value));
        }

        Object pop_back() {
            if (!tail) return Object::make_null(); // Защита от пустого списка

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
        Object finalize() {
            if (tail) {
                tail->cdr = Object::make_null();
            } else {
                head = Object::make_null();
            }
            return head; 
        }
    };

} // namespace script
