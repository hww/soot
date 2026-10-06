# GOAL: низкоуровневый справочник

Документ описывает синтаксис и примитивы GOAL, важные для понимания того, **как компилятор обрабатывает код**: объявления, размещение в памяти, типы, методы, состояния, приведения. Игровая логика не рассматривается.

---

## 1. Базовые типы (built-in types)

Предоставляются ядром и runtime-ом.

### Числовые
- `int`, `uint` — регистровой ширины
- `int8/16/32/64/128`, `uint8/16/32/64/128`
- `float`
- `binteger` — упакованное целое (младшие 3 бита = 0)
- `sinteger`, `uinteger`

### Ссылочные
- `object` — базовый для всех ссылок
- `basic` — с runtime-заголовком (тип, size, method-table)
- `structure` — без заголовка
- `pointer` — нетипизированный указатель
- `function`
- `symbol`, `string`, `type`

### Специальные
- `none` — void
- `pair`
- `number`
- `array`, `inline-array`

```lisp
(define-extern foo int)
(define-extern bar (function int int int))
```

---

## 2. `basic` vs `structure`

Ключевое различие для компилятора: наличие runtime-заголовка.

| | `basic` | `structure` |
|---|---|---|
| Runtime-тип | да | нет |
| Методы | да | нет |
| `type-type?` | работает | нет |
| Размер | +заголовок | только данные |
| Создание | `object-new` | `object-new` |

```lisp
(deftype my-basic (basic)
  ((value int :offset 4)))

(deftype my-struct (structure)
  ((x float)
   (y float)))
```

Компилятор для `basic` генерирует таблицу методов и type-объект, для `structure` — только layout.

---

## 3. Декларация типов (`deftype`)

```lisp
(deftype process-tree (basic)
  ((name     basic :offset-assert 4)
   (parent   (pointer process-tree) :offset-assert 12)
   (child    (pointer process-tree) :offset-assert 20)
   (self     process-tree :offset-assert 28))
  (:methods
   (new (symbol type basic) _type_)
   (activate (_type_ process-tree basic pointer) process-tree))
  :size-assert #x20
  :method-count-assert 14
  :flag-assert #x90000000c)
```

Атрибуты полей, влияющие на layout:

- `:offset N`, `:offset-assert N` — фиксация смещения
- `:size N` — битовая ширина (bitfield)
- `:inline` — значение внутри структуры, не ссылка
- `:dynamic` — массив переменной длины в хвосте
- `:overlay-at path` — наложение поля на другое смещение (union)
- `:pack-me` — без выравнивания
- `:allow-misaligned` — разрешить невыровненный доступ

Атрибуты типа:

- `:size-assert` — проверка размера
- `:method-count-assert` — проверка числа методов
- `:flag-assert` — общая проверка layout
- `:no-runtime-type` — не генерировать type-объект (тип задан в ядре)
- `:state-methods` — список состояний (states)

---

## 4. Enum и Bitfield

### Enum

```lisp
(defenum language-enum
  :type int64
  (english) (french) (german))
```

Значения присваиваются 0,1,2..., можно указать явно: `(english 5)`.

### Bitfield

```lisp
(defenum process-mask
  :bitfield #t
  :type uint32
  (execute 0)   ;; бит 0 → маска 1
  (draw 1)      ;; бит 1 → маска 2
  (pause 2))    ;; бит 2 → маска 4
```

Компилятор превращает имя в маску (сдвиг 1 на номер бита). Работа — через `logtest?`, `logior!`, `logclear!`, `logxor!`.

---

## 5. Константы

```lisp
(defconstant COLLISION_MISS -100000000.0)
(defconstant QUAT_SCALE 0.000030517578125)
(defglobalconstant BIG_COLLIDE_CACHE_SIZE 5000)
```

`defconstant` попадает в kernel-defs и доступен на этапе компиляции; `defglobalconstant` — тоже, но не экспортируется в kernel.

Глобальные переменные:

```lisp
(define *walk-mods* (new 'static 'surface ...))
(define-perm *camera* camera-master #f)  ;; не сбрасывается при загрузке уровня
```

---

## 6. Forward declaration и extern

### `define-extern`
Объявление символа с типом без определения — для символов из C-ядра или других файлов.

```lisp
(define-extern #f symbol)
(define-extern function type)
(define-extern malloc (function symbol int pointer))
```

### `declare-type`
Предварительное объявление типа — разрывает циклы.

```lisp
(declare-type collide-shape basic)
(declare-type joint-control basic)
```

### `declare-file`
Помечает файл как debug-only; компилятор включает его только в debug-сборке.

```lisp
(declare-file (debug))
```

---

## 7. Методы: декларация и дефиниция

### Декларация в `deftype`

```lisp
(:methods
 (new (symbol type int) _type_)
 (current-cycle-distance (_type_) float))
```

Первый аргумент — `_type_` (`this`), последний — возвращаемый тип. Порядковый номер метода в `:methods` = индекс в method-table.

### Дефиниция

```lisp
(defmethod new joint-control ((allocation symbol) (type-to-make type) (arg0 int))
  ...)
```

Синтаксис: `(defmethod имя Тип ((arg тип) ...) тело)`.

### Вызов родительского метода

```lisp
(defmethod params-init ((this barrel) (arg0 entity))
  (let ((t9-0 (method-of-type crate params-init))) (t9-0 this arg0))
  ...)
```

### `:replace`
Заменяет унаследованный метод, а не добавляет новый слот в method-table.

```lisp
(relocate (_type_ kheap (pointer uint8)) none :replace)
```

---

## 8. Создание экземпляров

### `object-new`

Низкоуровневое создание `basic`/`structure`:

```lisp
(object-new allocation type-to-make size)
```

Возвращает указатель или 0.

### `new`

Высокоуровневый конструктор с вызовом соответствующего `new`-метода:

```lisp
(new 'process 'collide-shape-moving this (collide-list-enum usually-hit-by-player))
```

Первый аргумент — allocation:
- `'static` — при загрузке, без runtime
- `'global` — глобальная куча
- `'process` — куча процесса
- `'debug` — debug-куча
- `'stack` — на стеке

### `new-dynamic-structure`

Для типов с `:dynamic`-массивом:

```lisp
(new-dynamic-structure allocation type-to-make
                       (the-as int (+ (-> type-to-make size) (* 48 n))))
```

### Удаление

```lisp
(delete-basic obj)
```

---

## 9. Функции

```lisp
(defun ray-sphere-intersect ((o vector) (d vector) (c vector) (r float))
  (local-vars (zero uint128) (v1-1 uint128))
  (the-as float result))
```

- `local-vars` — объявление переменных вне лексического блока (восстанавливает регистры).
- `(the-as T x)` — небезопасное приведение.
- `(the T x)` — безопасное (с runtime-проверкой).

### Анонимные

```lisp
(lambda ((arg0 surface) (arg1 object)) (set! (-> arg0 fric) 151756.8))
```

### Behavior

Тип-функция с поддержкой `suspend`, `go`, `return`:

```lisp
(defbehavior reset-follow camera-master ()
  (set! (-> self tpos-old quad) (-> (target-cam-pos) quad)))
```

Сигнатура: `(function ... :behavior Тип)`.

### `asm-func` и `mips2c`

Функция, определённая на ассемблере:

```lisp
(def-mips2c cspace<-parented-transformq-joint! (function cspace transformq none))
```

Компилятор не генерирует для неё код, а берёт готовый.

---

## 10. Массивы

### `inline-array`
Элементы лежат подряд; для классов — `inline-array-class` с обязательным `heap-base`.

```lisp
(deftype skeleton (inline-array-class)
  ((bones bone :inline :dynamic)))
(set! (-> skeleton heap-base) (the-as uint 96))
```

### `array`
Массив ссылок; создаётся `boxed-array`:

```lisp
(new 'static 'boxed-array :type string "a" "b")
```

### Доступ

```lisp
(-> arr i)
(-> arr data i)
```

---

## 11. Dereference и приведение типов

- `(-> obj a b c)` — доступ к полю
- `(&-> obj a b)` — адрес поля
- `(&+ ptr n)` — арифметика указателей
- `(the-as T x)` — переинтерпретация битов
- `(the T x)` — проверка runtime-типа
- `(rtype-of x)` — получить runtime-тип
- `(type-type? t1 t2)` — проверка наследования

```lisp
(the-as float arg0)
(the collider arg0)
(type-type? (-> self type) process-drawable)
```

---

## 12. Inline, align, прочие атрибуты полей

- `:inline` — значение внутри структуры
- `:dynamic` — хвостовой массив переменной длины
- `:offset N` / `:offset-assert N` — смещение
- `:overlay-at` — union (несколько полей на одном смещении)
- `:size N` — битовая ширина
- `:pack-me` — плотная упаковка
- `:allow-misaligned` — разрешить невыровненный доступ

Пример union-а:

```lisp
(deftype surface (basic)
  ((name symbol)
   (data float 30 :overlay-at turnv)
   (hook function 4 :overlay-at active-hook)
   (dataw uint32 2 :overlay-at mode)))
```

---

## 13. Состояния (states)

```lisp
(defstate cam-fixed (camera-slave)
  :event   (behavior ((proc process) (argc int) (message symbol) (block event-message-block)) ...)
  :enter   (behavior () ...)
  :exit    (behavior () ...)
  :trans   (behavior () ...)
  :code    (behavior () ...)
  :post    (behavior () ...))
```

Слоты обрабатываются runtime-ом процесса. `:virtual #t` — состояние может быть переопределено в подтипе.

Переходы:

```lisp
(go-virtual wait)
(go (method-of-object this nav-enemy-idle))
(cam-slave-go cam-free-floating)
```

---

## 14. `process-spawn` и `process-spawn-function`

Создание процесса с автоматической инициализацией:

```lisp
(process-spawn money :init money-init-by-other-no-bob
                s4-1 *null-vector* 5 1.0 (-> self entity) :to self)
```

Ключи: `:init`, `:from` (пул), `:to` (родитель), `:name`, `:stack-size`, `:stack`.

Функциональная форма:

```lisp
(process-spawn-function list-keeper list-keeper-init
                        :from *camera-dead-pool* :to self)
```

---

## 15. Метки и переходы

Восстановлены из MIPS-ветвлений:

```lisp
(b! #t cfg-13)
(label cfg-13)

(b! (< x y) cfg-5)
(b! (zero? a2-4) cfg-6 :delay (nop!))
```

`b!` — безусловный/условный переход; `:delay` — delay slot.

---

## 16. Inline-assembly и VU

```lisp
(rlet ((acc :class vf)
       (vf0 :class vf)
       (vf1 :class vf))
  (init-vf0-vector)
  (.lvf vf1 (&-> arg1 quad))
  (.mul.x.vf vf2 vf2 vf3))
```

- `rlet` — резервирование VU-регистров
- `with-vf`, `with-vf0`, `with-sp` — временные окружения
- `(break!)` — аналог int3
- `.lvf`, `.svf`, `.mul.x.vf` и т.п. — VU-инструкции

---

## 17. Регистровые переменные

```lisp
(local-vars (zero uint128) (v1-1 uint128) (f31-0 none))
```

Объявляет переменные, которые компилятор размещает в регистрах; часто восстанавливаются декомпилятором.

---

## 18. Директивы препроцессора

```lisp
(#when PC_PORT ...)
(#unless PC_PORT ...)
(#if PC_PORT ... ...)
```

Применяются на этапе компиляции для платформо-зависимого кода.

---

## 19. Пакеты и зависимости

```lisp
(in-package goal)
(bundles "ENGINE.CGO" "GAME.CGO")
(require "engine/math/vector.gc")
```

`require` — compile-time гарантия, что файл уже загружен.

---

## 20. Макросы и quoting

```lisp
(defmacro ja (&key (chan 0) &key (group! #f) ...)
  `(let ((ja-ch (-> self skel root-channel ,chan)))
     ,(if frame-interp `(set! (-> ja-ch frame-interp) ,frame-interp) `(none))
     ...))
```

- `` ` `` — quasiquote
- `,` — unquote
- `,@` — splice
- `'sym`, `'(a b)` — quote

---

## 21. Регистрация ассетов в compile-time

```lisp
(def-art-elt babak-ag babak-lod0-jg 0)
(def-joint-node babak-lod0-jg "align" 1)
(def-tex bigpuff effects 0)
```

Заполняют таблицы, которые использует `defskelgroup`:

```lisp
(defskelgroup *babak-sg*
  babak babak-lod0-jg -1
  ((babak-lod0-mg (meters 20)))
  :bounds (static-spherem 0 2 0 3))
```

---

## 22. `add-connection` и `define-perm`

```lisp
(add-connection *debug-engine* self camera-master-debug self #f #f)
(define-perm *camera* camera-master #f)
```

`define-perm` — глобальная переменная, сохраняющаяся между загрузками уровня.

---

## 23. Метки в сгенерированном коде

Декомпилятор генерирует блоки с метками:

```lisp
(b! #t cfg-13 :delay (nop!))
(set! v1-1 (the-as collide-work 0))
(label cfg-3)
(.add.y.vf.x vf8 vf8 vf8)
(b! (nonzero? a0-4) cfg-3 :delay (nop!))
(label cfg-6)
```

Это прямое отражение MIPS-ветвлений, важно для чтения декомпилированного кода.

---

## 24. `with-pp`

Доступ к неявному указателю процесса (`pp`), хранящемуся в регистре:

```lisp
(defun cam-slave-go ((arg0 state))
  (with-pp
    (cam-slave-init-vars)
    (let ((t9-1 (the-as (function object) enter-state)))
      (set! (-> pp next-state) arg0)
      (t9-1))))
```

---

## 25. `format` и `*temp-string*`

```lisp
(format (clear *temp-string*) "~S~S" arg0 arg1)
```

`(clear *temp-string*)` возвращает переиспользуемый буфер.

---

## 26. Инкрементальные и битовые операции

```lisp
(+! a b)   (*! a b)   (-! a b)   (/! a b)
(&+! ptr n)
(logior! x m)   (logand! x m)   (logxor! x m)   (logclear! x m)
```

Компилятор разворачивает их в соответствующие MIPS-инструкции с записью результата обратно в переменную.

---

## 27. `defenum` vs `defconstant`

- `defenum` — генерирует набор констант с автоинкрементом
- `defconstant` — одна константа
- `defglobalconstant` — константа без попадания в kernel-defs

---

## Что важно понимать про компилятор

1. **Layout фиксируется явно** через `:offset`, `:size`, `:flag-assert`, `:size-assert`. Ошибки layout ловятся на этапе загрузки.
2. **`basic`/`structure`** — фундаментальное разделение: `basic` даёт runtime-тип, `structure` — нет.
3. **Методы** нумеруются порядком в `:methods`; `:replace` и `method-of-type` управляют таблицей.
4. **States** — это фактически набор behavior-ов, которые runtime вызывает по слотам.
5. **`b!`/`label`** — прямое отражение MIPS-ветвлений, необходимое для чтения сгенерированного кода.
6. **`the-as` vs `the`** — первое без проверки, второе с runtime-проверкой.
7. **`local-vars`** — восстановление регистровых переменных.
8. **`#when`, `#unless`, `#if`** — препроцессор, применяемый до генерации кода.
9. **`def-art-elt`, `def-joint-node`, `def-tex`** — заполняют compile-time таблицы для `defskelgroup`.