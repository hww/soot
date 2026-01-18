# Спецификация вывода объектов в SOOT

## 1. Метод `print()` (REPL / Обычный вывод)

Служит для краткого отображения объекта в интерактивной среде.

* **Литералы (Числа, Списки):** Выводятся в стандартном формате S-expression. Списки можно скопировать и вставить обратно (Readable).
* **Системные объекты (Place, Token, Lambda):** Выводятся в формате `#<TYPE IDENTIFIER @ADDRESS>`.
* `#<` — маркер «нечитаемого» объекта (защита ридера).
* `TYPE` — имя типа (например, `place`).
* `IDENTIFIER` — краткое имя (ключ или имя регистра) **без кавычек**.
* `@ADDRESS` — (опционально) адрес объекта в C++ для различения идентичных по контенту ссылок.



## 2. Метод `printc()` (Canonical / Clear)

Используется для вывода «содержимого» объекта без лишнего шума.

* **Строки:** Печатаются **без кавычек** (в отличие от `print`).
* **Остальные типы:** Вызывают стандартный `print()`.
* *Цель:* Обеспечить удобный вывод текста в консоль (например, при сборке ассемблера).

## 3. Метод `inspect()` (S-expression Introspection)

**Возвращает не строку, а объект SOOT (список/A-list).**

* **Формат:** `(TYPE :KEY1 VALUE1 :KEY2 VALUE2 ...)`.
* **Homoiconicity:** Результат `inspect` — это валидные данные, которые можно обрабатывать самим интерпретатором (через `getf` и т.д.).
* *Цель:* Программный доступ к внутренностям объектов (адреса, смещения в памяти, метаданные).

---

## Шаг 1: Реализация в C++ (Базовый уровень)

Начнем с добавления `printc` и изменения `inspect` в базовом классе `Object` и `HeapObject`.

### В `Object.hpp` / `Object.cpp`:

```cpp
// 1. Метод для "чистого" вывода
std::string Object::printc() const {
    if (this->is_string()) {
        return this->as_string(); // Возвращаем строку без кавычек
    }
    return this->print(); // Для всех остальных вызываем обычный print
}

// 2. Метод inspect теперь возвращает Object (список), а не string
Object Object::inspect() const {
    if (this->is_heap_object()) {
        return this->heap_obj->inspect(); // Делегируем хип-объекту
    }
    // Для скалярных типов (чисел и т.д.) возвращаем простой список
    // Например: (integer :value 7)
    return list(make_symbol(this->type_name()), make_keyword("value"), *this);
}

```

---

## Шаг 2: Обновление `PlaceObject`

Теперь применим спецификацию к нашему новому объекту.

```cpp
class PlaceObject : public HeapObject {
public:
    // Стандартный краткий принт для REPL: #<place a @0x1234>
    std::string print() const override {
        return fmt::format("#<place {} @{:p}>", 
                           key.printc(), // Ключ без кавычек внутри скобок
                           (void*)this);
    }

    // Программный инспект (возвращает список данных)
    Object inspect() const override {
        return list(
            make_symbol("place"),
            make_keyword("key"),       key,
            make_keyword("container"), container,
            make_keyword("value"),     this->get(), // Текущее значение
            make_keyword("address"),   make_integer((long)this)
        );
    }
};
```
