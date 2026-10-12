# Документация стандартной библиотеки SOOT (lib.sot) v2.0

Это интерпретируемая стандартная библиотека для диалекта **SOOT/Lisp**. Она загружается в `*global-env*` (окружение интерпретатора) и содержит утилиты, макросы и функции, которые выполняются во время раскрытия макросов.

> **Важно:** Макросы из `lib.sot` живут только в `*global-env*`. Компилятор их **не видит** — он ищет макросы исключительно в `*soot-env*`. Это разделение позволяет избежать конфликта между интерпретаторными и компиляторными версиями `if`/`when`/`not`/`==`.

---

## 📚 Содержание

1. [Определение функций и макросов](#1-определение-функций-и-макросов)
2. [Управление потоком](#2-управление-потоком-control-flow)
3. [Предикаты](#3-предикаты)
4. [Числовые сравнения](#4-числовые-сравнения)
5. [Case / Case-Type](#5-case--case-type)
6. [Работа со списками](#6-работа-со-списками)
7. [Map-функции](#7-map-функции)
8. [Изменение данных (Mutation)](#8-изменение-данных-mutation)
9. [Циклы и итераторы](#9-циклы-и-итераторы)
10. [Строки](#10-строки)
11. [Хеш-таблицы](#11-хеш-таблицы)
12. [Структуры (defstruct)](#12-структуры-defstruct)
13. [Getters & Setters](#13-getters--setters)
14. [Логирование](#14-логирование)
15. [Asserts & Errors](#15-asserts--errors)
16. [Gensym](#16-gensym)
17. [Константы](#17-константы)
18. [Serialization](#18-serialization)

---

## 1. Определение функций и макросов

| Макрос | Описание | Пример |
| --- | --- | --- |
| `(defsmacro name args &rest body)` | Определяет интерпретаторный макрос (SOOT-macro). | `(defsmacro inc! (x) \`(set! ,x (+ ,x 1)))` |
| `(desfun name args &rest body)` | Определяет функцию (сахар для `define lambda`). | `(desfun add (a b) (+ a b))` |
| `(defparameter name value)` | Определяет параметр (глобальную переменную). | `(defparameter pi 3.14)` |
| `(defvar name value)` | Определяет переменную, если она ещё не определена. | `(defvar counter 0)` |
| `(defgmacro name args &rest body)` | Определяет goal-макрос в `*soot-env*` (для компилятора). | `(defgmacro my-macro (x) ...)` |
| `(declare-extern name type)` | Объявляет внешний символ. | `(declare-extern printf 'function)` |
| `(with-gensyms (syms...) &rest body)` | Генерирует уникальные имена для переменных внутри макроса, предотвращая коллизии. | `(with-gensyms (tmp) ...)` |

**Пример:**

```scheme
(defsmacro plus-one (x) `(+ ,x 1))
(plus-one 5)  ; → 6
```

---

## 2. Управление потоком (Control Flow)

### `(if clause true-branch false-branch)`

Стандартное ветвление. Если `clause` не `#f`, выполняется первая ветка, иначе — вторая.

> **Важно:** Это интерпретаторная версия `if`, которая живёт в `*global-env*`. Компилятор использует собственный встроенный `if` (IfNode).

### `(when clause &rest body)` / `(unless clause &rest body)`

Удобные обёртки для одиночных условий.

- `when` выполняет код, если условие истинно.
- `unless` — если ложно.

```scheme
(when (> x 0) (print "positive"))
(unless (> x 0) (print "non-positive"))
```

### `(not x)`

Логическое отрицание.

### `(aif condition true-branch false-branch)`

**Анафорический IF.** Вычисляет `condition` один раз и сохраняет результат в переменную `it`, доступную внутри веток.

```scheme
(aif (find-user 123)
     (print it)       ; 'it' содержит результат find-user
     (print "No user found"))
```

---

## 3. Предикаты

Все предикаты реализованы как интерпретаторные макросы и разворачиваются в вызовы `type?` с interned-символами. Сравнения работают за **O(1)** благодаря pointer equality.

| Предикат | Описание |
| --- | --- |
| `(none? x)` | Проверка на `none` |
| `(pair? x)` | Проверка на пару |
| `(string? x)` | Проверка на строку |
| `(char? x)` | Проверка на символ |
| `(symbol? x)` | Проверка на символ (symbol) |
| `(float? x)` | Проверка на float |
| `(int? x)` | Проверка на integer |
| `(number? x)` | `(or (float? x) (int? x))` |
| `(bool? x)` | Проверка на `#t` / `#f` |
| `(function? x)` | Проверка на функцию |
| `(macro? x)` | Проверка на макрос |
| `(native-obj? x)` | Проверка на native-объект |
| `(array? x)` | Проверка на массив |
| `(hash-table? x)` | Проверка на хеш-таблицу |
| `(type-type? x)` | Проверка на тип |
| `(type-spec? x)` | Проверка на type-spec |
| `(list? x)` | Проверка на корректный список (функция) |
| `(notnull? x)` | `(not (null? x))` |

**Пример:**

```scheme
(pair? '(1 2))           ; → #t
(number? 42)             ; → #t
(number? 3.14)           ; → #t
(list? '(1 2 3))         ; → #t
(list? '(1 . 2))         ; → #f
```

---

## 4. Числовые сравнения

| Макрос | Описание |
| --- | --- |
| `(neq? a b)` | `(not (eq? a b))` |
| `(!= a b)` | `(not (= a b))` — числовое неравенство |
| `(== a b)` | `(eq? a b)` |
| `(between val min max)` | `(and (<= min val) (<= val max))` |

**Пример:**

```scheme
(== 'a 'a)              ; → #t
(!= 5 3)                ; → #t
(between 5 1 10)        ; → #t
```

---

## 5. Case / Case-Type

### `(case expr &rest branches)`

Классический `case` с множественными значениями.

```scheme
(case x
  ((1 2 3) (print "small"))
  (4       (print "four"))
  (else    (print "other")))
```

### `(case-type obj &rest branches)`

Диспетчеризация по типу объекта. Каждая ветка выполняет `eq?`-проверку против interned-символа типа.

```scheme
(case-type obj
  (integer (print "int"))
  (string  (print "str"))
  (lextoken (print "token"))
  (else    (print "unknown")))
```

> **Best Practice:** Всегда добавляйте `else`-ветку при обработке критичных данных (например, `lextoken`), чтобы поймать неожиданные состояния.

---

## 6. Работа со списками

### Базовая навигация

| Функция | Описание |
| --- | --- |
| `(first x)` | Первый элемент (`car`) |
| `(rest x)` | Хвост (`cdr`) |
| `(second x)` | Второй элемент |
| `(third x)` | Третий элемент |
| `(nth n lst)` | N-й элемент |
| `(caar x)` / `(cadr x)` / `(cdar x)` / `(cddr x)` | Комбинации `car`/`cdr` |
| `(caaar x)` / `(caadr x)` / `(cadar x)` / `(caddr x)` | Тройные комбинации |
| `(cdddr x)` / `(cadddr x)` | Тройные/четверные комбинации |

### Инструменты

| Функция | Описание |
| --- | --- |
| `(make-list len val)` | Создать список из `len` копий `val` |
| `(copy-list lis)` | Копия списка |
| `(append l1 l2)` | Конкатенация списков |
| `(flatten lis)` | Развернуть вложенные списки |
| `(reverse lst)` | Обратный порядок (новая копия) |
| `(nreverse lis)` | Обратный порядок (деструктивно) |
| `(filter pred lst)` | Оставить элементы, удовлетворяющие предикату |
| `(remove item lst)` | Удалить все вхождения `item` |
| `(remove-if pred lst)` | Удалить элементы по предикату |
| `(find-if pred lst)` | Найти первый элемент, удовлетворяющий предикату |
| `(member x lst)` | Проверка наличия (возвращает хвост списка) |
| `(assoc x alist)` | Поиск пары в ассоциативном списке |
| `(substitute new old lst)` | Заменить `old` на `new` |
| `(recsubstitute dest source lis)` | Рекурсивная замена |
| `(nsubstitute new old lst)` | Деструктивная замена |

**Пример:**

```scheme
(filter (lambda (x) (> x 2)) '(1 2 3 4 5))  ; → (3 4 5)
(remove 3 '(1 2 3 4 3))                     ; → (1 2 4)
(reverse '(1 2 3))                          ; → (3 2 1)
(flatten '(1 (2 (3 4)) 5))                  ; → (1 2 3 4 5)
```

---

## 7. Map-функции

| Функция | Описание |
| --- | --- |
| `(map fun x)` | Применить `fun` к каждому элементу списка (возвращает список) |
| `(map2 fun1 fun2 lst1 lst2)` | Применить две функции к двум спискам |
| `(mapcar func lst)` | Классический `mapcar` (1 список) |
| `(mapcar2 func lst1 lst2)` | `mapcar` для двух списков |
| `(mapcar3 func lst1 lst2 lst3)` | `mapcar` для трёх списков |
| `(flat-map proc lis)` | Map + flatten |
| `(any pred lst)` | Есть ли хотя бы один элемент, удовлетворяющий предикату |

**Пример:**

```scheme
(mapcar (lambda (x) (* x 2)) '(1 2 3))       ; → (2 4 6)
(mapcar2 + '(1 2 3) '(10 20 30))             ; → (11 22 33)
(any (lambda (x) (> x 5)) '(1 2 3 6))        ; → #t
```

---

## 8. Изменение данных (Mutation)

| Макрос | Описание |
| --- | --- |
| `(1+ x)` | `(+ x 1)` |
| `(1- x)` | `(- x 1)` |
| `(inc! x)` | `(set! x (1+ x))` |
| `(dec! x)` | `(set! x (1- x))` |
| `(+! place arg)` | `(set! place (+ place arg))` |
| `(-! place arg)` | `(set! place (- place arg))` |
| `(push! lst x)` | Добавить `x` в начало `lst` (работает с символами и геттерами) |
| `(pop! lst)` | Извлечь первый элемент из `lst` и обновить переменную |

### Умный `push!`

`push!` понимает разные формы "места" (place):

```scheme
;; Символ — просто set!
(push! my-list 1)

;; Квотированный символ — разворачивается в set!
(push! 'my-list 1)  ; → (set! my-list (cons 1 my-list))

;; Форма с геттером — использует setter через defsetf
(push! (gethash 'key table) value)  ; → (sethash 'key table (cons value (gethash 'key table)))
```

**Пример:**

```scheme
(define lst '(2 3))
(push! lst 1)       ; lst → (1 2 3)
(pop! lst)          ; → 1, lst → (2 3)

(define x 10)
(inc! x)            ; x → 11
(+! x 5)            ; x → 16
```

### `(sign x)`

Возвращает `'+` для неотрицательных и `'-` для отрицательных.

---

## 9. Циклы и итераторы

### `(for-each container proc)`

Универсальный итератор. Работает со списками, массивами, строками и хеш-таблицами.

```scheme
(for-each '(1 2 3) (lambda (x) (print x)))
(for-each "abc" (lambda (c) (print c)))
(for-each hash-table (lambda (k v) (print k v)))
```

---

## 10. Строки

| Функция | Описание |
| --- | --- |
| `(string-empty? s)` | Проверка на пустую строку |
| `(intern str)` | `(string->symbol str)` |
| `(concat &rest args)` | Конкатенация строк |
| `(string->bytes s)` | Строка → список кодов символов |
| `(string-upcase s)` | Верхний регистр |
| `(string-downcase s)` | Нижний регистр |
| `(string-capitalize s)` | Title case |

**Пример:**

```scheme
(concat "Hello" " " "World")  ; → "Hello World"
(string-upcase "abc")         ; → "ABC"
(string->bytes "AB")          ; → (65 66)
```

---

## 11. Хеш-таблицы

### Обёртки

| Функция | Описание |
| --- | --- |
| `(gethash key table [defval])` | CL-style доступ (с дефолтом) |
| `(sethash key table value)` | CL-style установка |
| `(hash-table->keys ht)` | Список ключей |
| `(hash-table->values ht)` | Список значений |
| `(hash-table-keys ht)` | Список ключей (через `for-each`) |
| `(hash-table-values ht)` | Список значений (через `for-each`) |

**Пример:**

```scheme
(define ht (make-hash-table))
(sethash "name" ht "John")
(gethash "name" ht)          ; → "John"
(gethash "age" ht 0)         ; → 0
(hash-table->keys ht)        ; → ("name")
```

> `gethash` зарегистрирован как **getter** через `defsetf`, поэтому работает с `setf`:
> ```scheme
> (setf (gethash "name" ht) "Jane")
> ```

---

## 12. Структуры (defstruct)

### `(defstruct name &rest fields)`

Определяет структуру на основе хеш-таблицы с типом.

**Синтаксис полей:**

- `field` — поле с дефолтом `null`
- `(field default)` — поле с дефолтом

**Генерирует:**

1. **Конструктор** `make-<name>` с ключевыми аргументами.
2. **Предикат** `<name>?`.
3. **Геттеры** `<name>-<field>`.
4. **Сеттеры** `<name>-<field>!` (зарегистрированы через `defsetf`).

**Пример:**

```scheme
(defstruct point (x 0) (y 0))

(define p (make-point :x 10 :y 20))
(point? p)                  ; → #t
(point-x p)                 ; → 10
(point-y p)                 ; → 20

(point-x! p 100)            ; установить x = 100
(setf (point-x p) 200)      ; тоже работает через defsetf
```

### `(build-struct-from-plist args)`

Вспомогательная функция: `(:type imm :value 10)` → `((:type . imm) (:value . 10))`.

---

## 13. Getters & Setters

### `(setf place value)`

Универсальный макрос установки. Поддерживает три формы:

1. **Символ** → `set!`
2. **Форма с `->`** → `mem-set!`
3. **Форма с зарегистрированным setter'ом** → вызывает setter

```scheme
(setf x 10)                       ; → (set! x 10)
(setf (car lst) 5)                ; → (set-car! lst 5)
(setf (gethash "k" ht) "v")       ; → (sethash "k" ht "v")
(setf (point-x p) 100)            ; → (point-x! p 100)
```

### `(incf place [delta])` / `(decf place [delta])`

Инкремент/декремент места. По умолчанию `delta = 1`.

```scheme
(incf x)        ; → (setf x (+ x 1))
(incf x 10)     ; → (setf x (+ x 10))
(decf (point-x p))  ; → (setf (point-x p) (- (point-x p) 1))
```

### Регистрация сеттеров

```scheme
(defsetf 'car 'set-car!)
(defsetf 'cdr 'set-cdr!)
(defsetf 'get 'hash-table-set!)
(defsetf 'vector-ref 'vector-set!)
(defsetf 'vector-get 'vector-set!)
(defsetf 'gethash 'sethash)
```

> **Примечание:** В `lib.sot` есть опечатка — `(defsetf 'cdr 'set-cr!)` вместо `'set-cdr!`. Проверьте конфигурацию.

---

## 14. Логирование

| Функция | Описание |
| --- | --- |
| `(log-info fmt-str &rest args)` | Лог уровня `info` |
| `(log-warn fmt-str &rest args)` | Лог уровня `warn` |
| `(log-error fmt-str &rest args)` | Лог уровня `error` |

**Пример:**

```scheme
(log-info "User ~a logged in" username)
(log-error "Failed to load ~a" filename)
```

---

## 15. Asserts & Errors

### `(assert condition [message])`

Проверяет условие. Если ложно — бросает ошибку.

```scheme
(assert (> x 0) "x must be positive")
```

### `(check-type var predicate)`

Проверяет, что `var` удовлетворяет `predicate`. При неудаче — информативная ошибка.

```scheme
(check-type x number?)
; При провале: "Variable `x` failed type check `number?` because value: `"abc"` of type string"
```

---

## 16. Gensym

### `(with-gensyms (names...) &rest body)`

Генерирует уникальные символы для использования в макросах.

```scheme
(defsmacro swap! (a b)
  (with-gensyms (tmp)
    `(let ((,tmp ,a))
       (set! ,a ,b)
       (set! ,b ,tmp))))
```

---

## 17. Константы

| Константа | Значение |
| --- | --- |
| `pi` | `3.141592653589793` |
| `*soot-lib-loaded*` | `#t` после загрузки |

---

## 18. Serialization

### `(with-archive (ar buffer direction) &rest body)`

Макрос для работы с memory-archive с автоматической очисткой.

```scheme
(with-archive (ar buf :write)
  (archive-serialize ar 'vec3 :value '(:x 1 :y 2 :z 3)))
; archive-close! вызывается автоматически даже при ошибке
```

---

## 🔧 Bootstrap компилятора

### `(defmacro name args &rest body)`

Goal-макрос, который определяет макрос **в компиляторе** (в `*soot-env*`). Поддерживает docstring.

```scheme
(defmacro my-macro (x)
  "Docstring"
  `(+ ,x 1))
```

**Механика:**

1. Вызывает `update-macro-metadata` для сохранения docstring и аргументов.
2. Через `seval` и `defgmacro` определяет макрос в `*soot-env*`.

---

## 📝 Сводная таблица: что где живёт

| Форма | Где определена | Кто использует |
| --- | --- | --- |
| `if`, `when`, `not`, `unless`, `aif` | `lib.sot` (`*global-env*`) | Интерпретатор при раскрытии макросов |
| `if`, `when`, `not` (компиляторные) | `lib.soc` / `m_form_table` | Компилятор |
| `defmacro` | `lib.sot` (bootstrap) | Пользователь (goal-макросы) |
| `defsmacro` | `lib.sot` | Автор библиотеки |
| `desfun` | `lib.sot` | Автор библиотеки |
| `case`, `case-type` | `lib.sot` | Интерпретатор |
| `==`, `!=`, `neq?`, `between` | `lib.sot` | Интерпретатор |
| `setf`, `incf`, `decf` | `lib.sot` | Пользователь (интерпретатор) |

---

## ⚠️ Известные особенности

1. **Опечатка в `lib.sot`:** `(defsetf 'cdr 'set-cr!)` — вероятно, должно быть `'set-cdr!`.
2. **`unwind-protect`:** Используется в `with-archive`, но не определён в приведённом `lib.sot`. Проверьте, определён ли он в `lib.soc` или другом месте.
3. **`progn`:** Также используется в `with-archive`, но не определён в `lib.sot`.
4. **`map` vs `mapcar`:** В `lib.sot` есть обе функции. `map` определена вручную, `mapcar` — отдельная реализация.
5. **`append` переопределён:** В `lib.sot` определена своя версия `append`, которая может конфликтовать с встроенной. Проверьте, какая используется.

---

**Версия:** SOOT Core Library v2.0  
**Синтаксис:** Scheme-like с Common Lisp влияниями  
**Загружается в:** `*global-env*` (интерпретатор)  
**Не видна компилятору:** Да (по дизайну)