# Документация стандартной библиотеки CORE (v1.0)

Это базовая библиотека для диалекта **SOOT/Lisp**. Она расширяет ядро языка, добавляя удобный синтаксис, функции для работы со списками и безопасные макросы.

## 1. Определение структур

| Команда | Описание | Пример |
| --- | --- | --- |
| `(defmacro name args &body)` | Создает новый макрос. | `(defmacro plus-one (x) (+ ,x 1))` |
| `(defun name args &body)` | Определяет функцию (синтаксический сахар для `define lambda`). | `(defun add (a b) (+ a b))` |
| `(with-gensyms (syms...) &body)` | Генерирует уникальные имена для переменных внутри макроса, предотвращая коллизии. | `(with-gensyms (tmp) ...)` |

---

## 2. Управление потоком (Control Flow)

### `(if clause true-branch false-branch)`

Стандартное ветвление. Если `clause` не `#f`, выполняется первая ветка, иначе — вторая.

### `(when clause &body)` / `(unless clause &body)`

Удобные обертки для одиночных условий. `when` выполняет код, если условие истинно. `unless` — если ложно.

### `(aif condition true-branch false-branch)`

**Анафорический IF.** Вычисляет `condition` один раз и сохраняет результат в переменную `it`, которая доступна внутри веток.

```scheme
(aif (find-user 123)
     (print it)       ;; 'it' содержит результат find-user
     (print "No user found"))

```

---

## 3. Работа со списками

### Базовая навигация

* `first`, `second`, `third` — получение 1-го, 2-го или 3-го элемента.
* `rest` — получение хвоста списка (cdr).
* `caar`, `cadr`, `cdar`, `cddr` — комбинации операций car/cdr.

### Инструменты

* `(length lst)`: Возвращает количество элементов в списке. Реализовано итеративно (безопасно для глубоких списков).
* `(reverse lst)`: Создает новую копию списка с обратным порядком элементов.
* `(member x lst)`: Проверяет наличие элемента `x` в списке. Если найден, возвращает часть списка начиная с него.
* `(assoc x alist)`: Ищет пару в ассоциативном списке по ключу.

---

## 4. Циклы

### `(dotimes (var count [result]) &body)`

Выполняет тело цикла `count` раз. Переменная `var` меняется от `0` до `count - 1`.

```scheme
(dotimes (i 5) (print i)) ;; Напечатает 0 1 2 3 4

```

### `(dolist (var list) &body)`

Проходит по каждому элементу списка `list`, присваивая его переменной `var`.

---

## 5. Изменение данных (Mutation)

Эти макросы изменяют значение переменной (place) "на месте":

* `(inc! x)` / `(dec! x)`: Увеличить/уменьшить значение на 1.
* `(+! place val)` / `(-! place val)`: Прибавить/отнять `val` от переменной.
* `(push! lst obj)`: Добавить `obj` в начало списка `lst`.
* `(pop! lst)`: Извлечь первый элемент из списка `lst` и вернуть его. Переменная `lst` при этом обновляется.

---

## 6. Предикаты (Проверки)

Функции, возвращающие логическое значение:

* **Типы:** `string?`, `float?`, `integer?`, `number?`, `pair?`, `symbol?`.
* **Сравнение:** * `(neq? a b)`: Истина, если объекты **не** идентичны (`eq?`).
* `(!= a b)`: Истина, если числа **не** равны по значению.



---

### Подсказка для разработчика

Чтобы проверить, загружена ли библиотека в текущем окружении, используйте переменную `*core-lib-loaded*`.

Вот профессиональный комментарий для документации или `CONTRIBUTING.md`, который объясняет, как работает эта система. Текст написан на английском (так как это стандарт для исходного кода), с четким разделением на логику и преимущества.

---

### 📝 Documentation: Type System & Dispatching

#### Core Type Representation

The interpreter utilizes a **Static Symbol Mapping** for object types. Instead of returning raw strings or integers, the `type?` function returns interned symbols stored in `SymbolTable::core`.

* **Mechanism:** Every `ObjectType` maps directly to a pre-allocated `Object` (Symbol) during the Reader's initialization.
* **Performance:** Type comparisons are  operations because they rely on pointer equality (via `eq?`) rather than string comparisons.

#### High-Level Dispatching: `case-type`

To handle different object types gracefully, use the `case-type` macro. It provides a clean, declarative syntax for type-based branching.

**Syntax:**

```lisp
(case-type <expression>
  (<type-symbol-1> <body-1>)
  (<type-symbol-2> <body-2>)
  (else            <default-body>))

```

**Implementation Details:**

1. **Single Evaluation:** The input expression is evaluated exactly once and bound to a `gensym`'ed variable.
2. **Expansion:** The macro expands into a `let` block containing a `cond` structure.
3. **Efficiency:** Each branch performs a direct `eq?` check against interned type symbols (`integer`, `string`, `lextoken`, etc.).

#### Best Practices

* Use `case-type` instead of nested `if` or `cond` with manual `type?` calls.
* Always provide an `else` branch when handling compiler-critical data like `lextoken` to catch unexpected states.
* Refer to `SymbolTable::core` in C++ when adding new primitive types to ensure they are available to the Lisp environment.

---

**Что дальше?**
Если ты планируешь активно работать с токенами, я могу помочь написать версию `case-token`, которая будет проверять не только тип `lextoken`, но и его внутренний подтип (например, `OP_CODE`, `REGISTER`, `LABEL`) в одну строчку. Хочешь?